#include "App.hpp"

#include <etna/PipelineManager.hpp>
#include <etna/RenderTargetStates.hpp>
#include <etna/Etna.hpp>
#include <etna/GlobalContext.hpp>

#include "stb_image.h"


App::App()
  : resolution{1280, 720}
  , useVsync{true}
  , timeStart{std::chrono::system_clock::now()}
{
  {
    auto glfwInstExts = windowing.getRequiredVulkanInstanceExtensions();

    std::vector<const char*> instanceExtensions{glfwInstExts.begin(), glfwInstExts.end()};
    std::vector<const char*> deviceExtensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    etna::initialize(etna::InitParams{
      .applicationName = "Inflight Frames",
      .applicationVersion = VK_MAKE_VERSION(0, 1, 0),
      .instanceExtensions = instanceExtensions,
      .deviceExtensions = deviceExtensions,
      .physicalDeviceIndexOverride = {},
      .numFramesInFlight = 1,
    });
  }

  osWindow = windowing.createWindow(OsWindow::CreateInfo{
    .resolution = resolution,
  });

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

  commandManager = etna::get_context().createPerFrameCmdMgr();
  
  etna::create_program(
    "texture", 
    {INFLIGHT_FRAMES_SHADERS_ROOT "texture.comp.spv"});

  texturePipeline =
    etna::get_context().getPipelineManager().createComputePipeline("texture", {});

  textureImage = etna::get_context().createImage(etna::Image::CreateInfo{
    .extent = vk::Extent3D{resolution.x, resolution.y, 1},
    .name = "texture",
    .format = vk::Format::eR8G8B8A8Unorm,
    .imageUsage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage});

  auto createInfo = etna::Sampler::CreateInfo{
    .addressMode = vk::SamplerAddressMode::eRepeat, .name = "textureSampler"
  };

  textureSampler = etna::Sampler(createInfo);

  int width, height, channels;
  unsigned char* image_data = stbi_load(
    GRAPHICS_COURSE_RESOURCES_ROOT "/textures/test_tex_1.png", &width, &height, &channels, 4);

  fileTextureImage = etna::get_context().createImage(etna::Image::CreateInfo{
    .extent = vk::Extent3D{static_cast<unsigned>(width), static_cast<unsigned>(height), 1},
    .name = "file_texture",
    .format = vk::Format::eR8G8B8A8Unorm,
    .imageUsage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage |
      vk::ImageUsageFlagBits::eTransferDst});

  auto createInfo2 = etna::Sampler::CreateInfo{
    .addressMode = vk::SamplerAddressMode::eRepeat, .name = "fileTextureSampler"
  };

  fileTextureSampler = etna::Sampler(createInfo2);

  transferHelper =
    std::make_unique<etna::BlockingTransferHelper>(etna::BlockingTransferHelper::CreateInfo{
      .stagingSize = static_cast<std::uint32_t>(width * height),
    });

  std::unique_ptr<etna::OneShotCmdMgr> OneShotCommands = etna::get_context().createOneShotCmdMgr();

  transferHelper->uploadImage(*OneShotCommands, fileTextureImage, 0, 0,
      std::span<const std::byte>(reinterpret_cast<const std::byte*>(image_data), width * height * 4));

  stbi_image_free(image_data);

  etna::create_program(
    "shader",
    {INFLIGHT_FRAMES_SHADERS_ROOT "shader.vert.spv",
     INFLIGHT_FRAMES_SHADERS_ROOT "toy.frag.spv"});

  shaderPipeline = 
      etna::get_context().getPipelineManager().createGraphicsPipeline(
          "shader", 
          etna::GraphicsPipeline::CreateInfo{
              .fragmentShaderOutput = 
              {
                  .colorAttachmentFormats = {vk::Format::eB8G8R8A8Srgb}
              },
          });

}

App::~App()
{
  ETNA_CHECK_VK_RESULT(etna::get_context().getDevice().waitIdle());
}

void App::run()
{
  while (!osWindow->isBeingClosed())
  {
    windowing.poll();

    drawFrame();
  }

  ETNA_CHECK_VK_RESULT(etna::get_context().getDevice().waitIdle());
}

void App::drawFrame()
{
  auto currentCmdBuf = commandManager->acquireNext();
  etna::begin_frame();
  auto nextSwapchainImage = vkWindow->acquireNext();
  if (nextSwapchainImage)
  {
    auto [backbuffer, backbufferView, backbufferAvailableSem] = *nextSwapchainImage;

    ETNA_CHECK_VK_RESULT(currentCmdBuf.begin(vk::CommandBufferBeginInfo{}));
    {
      etna::set_state(
        currentCmdBuf,
        backbuffer,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::AccessFlagBits2::eColorAttachmentWrite,
        vk::ImageLayout::eColorAttachmentOptimal,
        vk::ImageAspectFlagBits::eColor);
      etna::flush_barriers(currentCmdBuf);

      float time =
        std::chrono::duration<float>(std::chrono::system_clock::now() - timeStart).count();

      {
        auto computeInfo = etna::get_shader_program("texture");

        auto set = etna::create_descriptor_set(
          computeInfo.getDescriptorLayoutId(0),
          currentCmdBuf,
          {
                etna::Binding{0, textureImage.genBinding({}, vk::ImageLayout::eGeneral)}
          });

        vk::DescriptorSet vkSet = set.getVkSet();

        currentCmdBuf.bindPipeline(vk::PipelineBindPoint::eCompute, texturePipeline.getVkPipeline());
        currentCmdBuf.bindDescriptorSets(vk::PipelineBindPoint::eCompute, texturePipeline.getVkPipelineLayout(),
          0, 1, &vkSet, 0, nullptr);

        struct Params
        {
          glm::uvec2 res;
          float time;
        };
        Params param{
            .res = resolution, 
            .time = time
        };
        currentCmdBuf.pushConstants( texturePipeline.getVkPipelineLayout(), vk::ShaderStageFlagBits::eCompute, 0, sizeof(param), &param);

        etna::flush_barriers(currentCmdBuf);

        currentCmdBuf.dispatch(resolution.x / 16, resolution.y / 16, 1);
      }


      etna::set_state(
        currentCmdBuf,
        textureImage.get(),
        vk::PipelineStageFlagBits2::eFragmentShader,
        vk::AccessFlagBits2::eShaderSampledRead,
        vk::ImageLayout::eShaderReadOnlyOptimal,
        vk::ImageAspectFlagBits::eColor);

      etna::flush_barriers(currentCmdBuf);

      {
        etna::RenderTargetState state{currentCmdBuf, {{}, {resolution.x, resolution.y}}, 
            {{backbuffer, backbufferView}}, {}};

        auto graphicsInfo = etna::get_shader_program("shader");

        auto set = etna::create_descriptor_set(
          graphicsInfo.getDescriptorLayoutId(0),
          currentCmdBuf,
          {
                etna::Binding{0, textureImage.genBinding(textureSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
                etna::Binding{1, fileTextureImage.genBinding(fileTextureSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)}
          });

        vk::DescriptorSet vkSet = set.getVkSet();

        currentCmdBuf.bindPipeline(vk::PipelineBindPoint::eGraphics, shaderPipeline.getVkPipeline());
        currentCmdBuf.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, shaderPipeline.getVkPipelineLayout(),
          0, 1, &vkSet, 0, nullptr);

        struct Params
        {
          glm::uvec2 res;
          glm::uvec2 mouse;
          float time;
        };
        Params param
        {
          .res = resolution, 
          .mouse = osWindow->mouse.freePos, 
          .time = time
        };
        currentCmdBuf.pushConstants(shaderPipeline.getVkPipelineLayout(), vk::ShaderStageFlagBits::eFragment, 0, sizeof(param), &param);

        currentCmdBuf.draw(3, 1, 0, 0);
      }

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

    auto renderingDone =
      commandManager->submit(std::move(currentCmdBuf), std::move(backbufferAvailableSem));

    const bool presented = vkWindow->present(std::move(renderingDone), backbufferView);

    if (!presented)
      nextSwapchainImage = std::nullopt;
  }

  etna::end_frame();

  if (!nextSwapchainImage && osWindow->getResolution() != glm::uvec2{0, 0})
  {
    auto [w, h] = vkWindow->recreateSwapchain(etna::Window::DesiredProperties{
      .resolution = {resolution.x, resolution.y},
      .vsync = useVsync,
    });
    ETNA_VERIFY((resolution == glm::uvec2{w, h}));
  }
}