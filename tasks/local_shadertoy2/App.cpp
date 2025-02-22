#define STB_IMAGE_IMPLEMENTATION
#include "App.hpp"

App::App()
  : resolution{1280, 720}, useVsync{true} {
  // Инициализация Vulkan с нужными расширениями
  {
    auto glfwInstExts = windowing.getRequiredVulkanInstanceExtensions();
    std::vector<const char*> instanceExtensions{glfwInstExts.begin(), glfwInstExts.end()};
    std::vector<const char*> deviceExtensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    etna::initialize(etna::InitParams{
      .applicationName = "Local Shadertoy",
      .applicationVersion = VK_MAKE_VERSION(0, 1, 0),
      .instanceExtensions = instanceExtensions,
      .deviceExtensions = deviceExtensions,
      .physicalDeviceIndexOverride = {},
      .numFramesInFlight = 1,
    });
  }

  // Создание окна ОС
  osWindow = windowing.createWindow(OsWindow::CreateInfo{
    .resolution = resolution,
  });

  // Связываем окно ОС с Vulkan
  {
    auto surface = osWindow->createVkSurface(etna::get_context().getInstance());
    vkWindow = etna::get_context().createWindow(etna::Window::CreateInfo{
      .surface = std::move(surface),
    });
    auto [w, h] = vkWindow->recreateSwapchain(etna::Window::DesiredProperties{
      .resolution = {resolution.x, resolution.y},
      .vsync = useVsync,
    });
    resolution = {w, h};
  }

  // Создаем командные менеджеры
  commandManager = etna::get_context().createPerFrameCmdMgr();
  oneShotManager = etna::get_context().createOneShotCmdMgr();

  // Создаем вычислительную программу и пайплайн
  etna::create_program("texture", {LOCAL_SHADERTOY2_SHADERS_ROOT "texture.comp.spv"});
  computePipeline = etna::get_context().getPipelineManager().createComputePipeline("texture", {});
  sampler = etna::Sampler(etna::Sampler::CreateInfo{.name = "computeSampler"});

  bufImage = etna::get_context().createImage(etna::Image::CreateInfo{
    .extent = vk::Extent3D{resolution.x, resolution.y, 1},
    .name = "output",
    .format = vk::Format::eR8G8B8A8Unorm,
    .imageUsage = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferSrc |
                  vk::ImageUsageFlagBits::eSampled
  });

  // Создаем графическую программу и пайплайн
  etna::create_program(
    "image",
    {LOCAL_SHADERTOY2_SHADERS_ROOT "toy.vert.spv", LOCAL_SHADERTOY2_SHADERS_ROOT "toy.frag.spv"});
  graphicsPipeline = etna::get_context().getPipelineManager().createGraphicsPipeline(
    "image",
    etna::GraphicsPipeline::CreateInfo{
      .fragmentShaderOutput = {
        .colorAttachmentFormats = {vk::Format::eB8G8R8A8Srgb},
        .depthAttachmentFormat = vk::Format::eD32Sfloat,
      },
    });

  graphicsSampler = etna::Sampler(etna::Sampler::CreateInfo{
    .addressMode = vk::SamplerAddressMode::eRepeat,
    .name = "graphicsSampler",
  });

  int width, height, channels;
  const auto file = stbi_load(
    LOCAL_SHADERTOY2_SHADERS_ROOT "../../../../resources/textures/test_tex_1.png",
    &width,
    &height,
    &channels,
    STBI_rgb_alpha);

  // Проверяем успешность загрузки текстуры
  if (!file) {
    throw std::runtime_error("Failed to load texture: test_tex_1.png");
  }

  image = etna::get_context().createImage(etna::Image::CreateInfo{
    .extent = vk::Extent3D{static_cast<unsigned int>(width), static_cast<unsigned int>(height), 1},
    .name = "texture",
    .format = vk::Format::eR8G8B8A8Unorm,
    .imageUsage = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferDst |
                  vk::ImageUsageFlagBits::eSampled
  });

  etna::BlockingTransferHelper(etna::BlockingTransferHelper::CreateInfo{
    .stagingSize = static_cast<std::uint32_t>(width * height),
  })
    .uploadImage(
      *oneShotManager,
      image,
      0,
      0,
      std::span(reinterpret_cast<const std::byte*>(file), width * height * 4));

  // Освобождаем память после загрузки текстуры
  stbi_image_free(file);
}

App::~App() {
  ETNA_CHECK_VK_RESULT(etna::get_context().getDevice().waitIdle());
}


void App::run() {
  while (!osWindow->isBeingClosed()) {
    windowing.poll();
    drawFrame();
  }
  // Ждем завершения всех команд перед закрытием приложения
  ETNA_CHECK_VK_RESULT(etna::get_context().getDevice().waitIdle());
}

void App::drawFrame() {
  auto currentCmdBuf = commandManager->acquireNext();
  etna::begin_frame();
  auto nextSwapchainImage = vkWindow->acquireNext();

  if (nextSwapchainImage) {
    auto [backbuffer, backbufferView, backbufferAvailableSem] = *nextSwapchainImage;

    ETNA_CHECK_VK_RESULT(currentCmdBuf.begin(vk::CommandBufferBeginInfo{}));
    {
      // Инициализируем backbuffer для работы (переводим его в нужное состояние)
      etna::set_state(
        currentCmdBuf,
        backbuffer,
        vk::PipelineStageFlagBits2::eTransfer,
        vk::AccessFlagBits2::eTransferWrite,
        vk::ImageLayout::eTransferDstOptimal,
        vk::ImageAspectFlagBits::eColor);
      etna::flush_barriers(currentCmdBuf);

      auto computeShader = etna::get_shader_program("texture");
      auto computeDescriptorSet = etna::create_descriptor_set(
        computeShader.getDescriptorLayoutId(0),
        currentCmdBuf,
        {etna::Binding{0, bufImage.genBinding(sampler.get(), vk::ImageLayout::eGeneral)}});
      const vk::DescriptorSet computeVkDescriptorSet = computeDescriptorSet.getVkSet();

      currentCmdBuf.bindPipeline(vk::PipelineBindPoint::eCompute, computePipeline.getVkPipeline());
      currentCmdBuf.bindDescriptorSets(
        vk::PipelineBindPoint::eCompute,
        computePipeline.getVkPipelineLayout(),
        0,
        1,
        &computeVkDescriptorSet,
        0,
        nullptr);

      int64_t elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::system_clock::now() - start)
                                .count();

      params = {
        .size_x = resolution.x,
        .size_y = resolution.y,
        .time = elapsedTime / 1000.f,
      };

      currentCmdBuf.pushConstants(
        computePipeline.getVkPipelineLayout(),
        vk::ShaderStageFlagBits::eCompute,
        0,
        sizeof(params),
        &params);
      etna::flush_barriers(currentCmdBuf);

      currentCmdBuf.dispatch((resolution.x + 31) / 32, (resolution.y + 31) / 32, 1);

      auto graphicsShader = etna::get_shader_program("image");
      auto graphicsDescriptorSet = etna::create_descriptor_set(
        graphicsShader.getDescriptorLayoutId(0),
        currentCmdBuf,
        {etna::Binding{0, bufImage.genBinding(sampler.get(), vk::ImageLayout::eGeneral)},
         etna::Binding{
           1, image.genBinding(graphicsSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)}});
      const vk::DescriptorSet graphicsVkDescriptorSet = graphicsDescriptorSet.getVkSet();

      {
        etna::RenderTargetState renderTargets{
          currentCmdBuf,
          {{0, 0}, {resolution.x, resolution.y}},
          {{.image = backbuffer, .view = backbufferView}},
          {}};

        currentCmdBuf.bindPipeline(
          vk::PipelineBindPoint::eGraphics, graphicsPipeline.getVkPipeline());
        currentCmdBuf.bindDescriptorSets(
          vk::PipelineBindPoint::eGraphics,
          graphicsPipeline.getVkPipelineLayout(),
          0,
          1,
          &graphicsVkDescriptorSet,
          0,
          nullptr);

        currentCmdBuf.pushConstants<vk::DispatchLoaderDynamic>(
          graphicsPipeline.getVkPipelineLayout(),
          vk::ShaderStageFlagBits::eFragment,
          0,
          sizeof(params),
          &params);

        currentCmdBuf.draw(3, 1, 0, 0);
      }

      etna::flush_barriers(currentCmdBuf);

      // Переход backbuffer в состояние, подходящее для показа на экране
      etna::set_state(
        currentCmdBuf,
        backbuffer,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        {},
        vk::ImageLayout::ePresentSrcKHR,
        vk::ImageAspectFlagBits::eColor);
      etna::flush_barriers(currentCmdBuf);
    }
    ETNA_CHECK_VK_RESULT(currentCmdBuf.end());


    auto renderingDone = commandManager->submit(std::move(currentCmdBuf), std::move(backbufferAvailableSem));
    const bool presented = vkWindow->present(std::move(renderingDone), backbufferView);
    if (!presented)
      nextSwapchainImage = std::nullopt;
  }

  etna::end_frame();

  // Если окно не свернуто, но swapchain недоступен, пересоздаем его
  if (!nextSwapchainImage && osWindow->getResolution() != glm::uvec2{0, 0}) {
    auto [w, h] = vkWindow->recreateSwapchain(etna::Window::DesiredProperties{
      .resolution = {resolution.x, resolution.y},
      .vsync = useVsync,
    });
    ETNA_VERIFY((resolution == glm::uvec2{w, h}));
  }
}
