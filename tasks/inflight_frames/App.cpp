#include "App.hpp"
#include "etna/DescriptorSet.hpp"
#include "etna/Window.hpp"

#include <cstdint>
#include <etna/Etna.hpp>
#include <etna/GlobalContext.hpp>
#include <etna/PipelineManager.hpp>
#include <etna/RenderTargetStates.hpp>
#include <etna/BlockingTransferHelper.hpp>
#include <memory>
#include <vulkan/vulkan_enums.hpp>
#include <stb_image.h>

#include <etna/Profiling.hpp>
#include <tracy/Tracy.hpp>
#include "shaders/UniformParams.h"

App::App()
  : resolution{1280, 720}
  , useVsync{true}
{
  // First, we need to initialize Vulkan, which is not trivial because
  // extensions are required for just about anything.
  {
    // GLFW tells us which extensions it needs to present frames to the OS window.
    // Actually rendering anything to a screen is optional in Vulkan, you can
    // alternatively save rendered frames into files, send them over network, etc.
    // Instance extensions do not depend on the actual GPU, only on the OS.
    auto glfwInstExts = windowing.getRequiredVulkanInstanceExtensions();

    std::vector<const char*> instanceExtensions{glfwInstExts.begin(), glfwInstExts.end()};

    // We also need the swapchain device extension to get access to the OS
    // window from inside of Vulkan on the GPU.
    // Device extensions require HW support from the GPU.
    // Generally, in Vulkan, we call the GPU a "device" and the CPU/OS combination a
    // "host."
    std::vector<const char*> deviceExtensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    // Etna does all of the Vulkan initialization heavy lifting.
    // You can skip figuring out how it works for now.
    etna::initialize(etna::InitParams{
      .applicationName = "Inflight Frames",
      .applicationVersion = VK_MAKE_VERSION(0, 1, 0),
      .instanceExtensions = instanceExtensions,
      .deviceExtensions = deviceExtensions,
      // Replace with an index if etna detects your preferred GPU incorrectly
      .physicalDeviceIndexOverride = {},
      .numFramesInFlight = numFramesInFlight,
    });
  }

  // Now we can create an OS window
  osWindow = windowing.createWindow(OsWindow::CreateInfo{
    .resolution = resolution,
  });

  // But we also need to hook the OS window up to Vulkan manually!
  {
    // First, we ask GLFW to provide a "surface" for the window,
    // which is an opaque description of the area where we can actually render.
    auto surface = osWindow->createVkSurface(etna::get_context().getInstance());

    // Then we pass it to Etna to do the complicated work for us
    vkWindow = etna::get_context().createWindow(etna::Window::CreateInfo{
      .surface = std::move(surface),
    });

    // And finally ask Etna to create the actual swapchain so that we can
    // get (different) images each frame to render stuff into.
    // Here, we do not support window resizing, so we only need to call this once.
    auto [w, h] = vkWindow->recreateSwapchain(etna::Window::DesiredProperties{
      .resolution = {resolution.x, resolution.y},
      .vsync = useVsync,
    });

    // Technically, Vulkan might fail to initialize a swapchain with the requested
    // resolution and pick a different one. This, however, does not occur on platforms
    // we support. Still, it's better to follow the "intended" path.
    resolution = {w, h};
  }

  // Next, we need a magical Etna helper to send commands to the GPU.
  // How it is actually performed is not trivial, but we can skip this for now.
  commandManager = etna::get_context().createPerFrameCmdMgr();

  generatedTextureImage = etna::get_context().createImage({
    .extent = vk::Extent3D{resolution.x, resolution.y, 1},
    .name = "texture",
    .format = vk::Format::eB8G8R8A8Srgb,
    .imageUsage = vk::ImageUsageFlagBits::eColorAttachment |
      vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment,
  });


  defaultSampler = etna::Sampler(etna::Sampler::CreateInfo{.name = "default_sampler"});

  etna::create_program(
    "inflight_frames",
    {INFLIGHT_FRAMES_SHADERS_ROOT "toy.frag.spv", INFLIGHT_FRAMES_SHADERS_ROOT "toy.vert.spv"});
  graphicsPipeline = etna::get_context().getPipelineManager().createGraphicsPipeline(
    "local_shadertoy2",
    etna::get_context().getPipelineManager().createGraphicsPipeline("inflight_frames",
    etna::GraphicsPipeline::CreateInfo{
      .fragmentShaderOutput = {.colorAttachmentFormats = {vk::Format::eB8G8R8A8Srgb}}});

  etna::create_program(
    "texture",
    {INFLIGHT_FRAMES_SHADERS_ROOT "texture.frag.spv", INFLIGHT_FRAMES_SHADERS_ROOT "toy.vert.spv"});
  texturePipeline = etna::get_context().getPipelineManager().createGraphicsPipeline(
    "texture",
    etna::get_context().getPipelineManager().createGraphicsPipeline("inflight_frames_textures",
    etna::GraphicsPipeline::CreateInfo{
      .fragmentShaderOutput = {.colorAttachmentFormats = {vk::Format::eB8G8R8A8Srgb}}});


  mouse_pos = glm::vec2(resolution / 2u);
  
  {
    int height, width, channels;
    unsigned char* textureData = stbi_load(
      GRAPHICS_COURSE_RESOURCES_ROOT "/textures/test_tex_1.png",
      &width,
      &height,
      &channels,
      4);
    assert(textureData != nullptr);

    loadedTextureImage1 = etna::get_context().createImage({
      .extent = vk::Extent3D{(uint32_t)width, (uint32_t)height, 1},
      .name = "loaded_texture",
      .format = vk::Format::eR8G8B8A8Srgb,
      .imageUsage =
        vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
    });

    etna::BlockingTransferHelper transferHelper(
      {.stagingSize = VkDeviceSize(width * height * 4)});

    std::unique_ptr<etna::OneShotCmdMgr> oneShotCmdMgr =
      etna::get_context().createOneShotCmdMgr();

    transferHelper.uploadImage(
      *oneShotCmdMgr,
      loadedTextureImage1,
      0,
      0,
      std::span<std::byte>(
        reinterpret_cast<std::byte*>(textureData), width * height * 4));

    stbi_image_free(textureData);
  }

  {
    int height, width, channels;
    unsigned char* textureData = stbi_load(
      GRAPHICS_COURSE_RESOURCES_ROOT "/textures/texture1.bmp",
      &width,
      &height,
      &channels,
      4);
    assert(textureData != nullptr);
    stbi_image_free(textureData);
  }
  
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

    ++frameCount;

    FrameMark;
  }

  // We need to wait for the GPU to execute the last frame before destroying
  // all resources and closing the application.
  ETNA_CHECK_VK_RESULT(etna::get_context().getDevice().waitIdle());
}

void App::drawFrame()
{

  ZoneScopedN("Frame");
  // First, get a command buffer to write GPU commands into.
  auto currentCmdBuf = commandManager->acquireNext();


  // Next, tell Etna that we are going to start processing the next frame.
  etna::begin_frame();

  // And now get the image we should be rendering the picture into.
  auto nextSwapchainImage = vkWindow->acquireNext();

  // When window is minimized, we can't render anything in Windows
  // because it kills the swapchain, so we skip frames in this case.
  if (nextSwapchainImage)
  {
    auto [backbuffer, backbufferView, backbufferAvailableSem] = *nextSwapchainImage;

    ETNA_CHECK_VK_RESULT(currentCmdBuf.begin(vk::CommandBufferBeginInfo{}));
    {
      ETNA_PROFILE_GPU(currentCmdBuf, "Frame start");
      etna::set_state(
        currentCmdBuf,
        generatedTextureImage.get(),
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::AccessFlagBits2::eColorAttachmentWrite,
        vk::ImageLayout::eColorAttachmentOptimal,
        vk::ImageAspectFlagBits::eColor);
      etna::flush_barriers(currentCmdBuf);

      if (!is_textures_loaded)
      {
        {
        ETNA_PROFILE_GPU(currentCmdBuf, "Making textures");
          etna::RenderTargetState state{
            currentCmdBuf,
            {{}, {resolution.x, resolution.y}},
            {{generatedTextureImage.get(), generatedTextureImage.getView({})}},
            {}};

           etna::get_shader_program("inflight_frames_textures");

          currentCmdBuf.bindPipeline(
            vk::PipelineBindPoint::eGraphics, texturePipeline.getVkPipeline());

          currentCmdBuf.pushConstants(
            graphicsPipeline.getVkPipelineLayout(),
            vk::ShaderStageFlagBits::eFragment,
            0,
            sizeof(resolution),
            &resolution);

          currentCmdBuf.draw(3, 1, 0, 0);
        }
        is_textures_loaded = true;
      }

      etna::set_state(
        currentCmdBuf,
        generatedTextureImage.get(),
        vk::PipelineStageFlagBits2::eFragmentShader,
        vk::AccessFlagBits2::eShaderRead,
        vk::ImageLayout::eShaderReadOnlyOptimal,
        vk::ImageAspectFlagBits::eColor);

      etna::flush_barriers(currentCmdBuf);

      {
        ETNA_PROFILE_GPU(currentCmdBuf, "Making main shader");
        etna::RenderTargetState state{
          currentCmdBuf,
          {{}, {resolution.x, resolution.y}},
          {{backbuffer, backbufferView}},
          {}};


        uniformParams.iResolution_x = resolution.x;
        uniformParams.iResolution_y = resolution.y;
        if (osWindow.get()->mouse[MouseButton::mbLeft] == ButtonState::High)
        {
          mouse_pos = osWindow.get()->mouse.freePos;
          uniformParams.iMouse_x = mouse_pos.x;
          uniformParams.iMouse_y = mouse_pos.y;
        }
        uniformParams.iTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::system_clock::now() - init_time)
                                .count() /
          1000.0f;
        etna::Buffer& param_buffer = gpuSharedResource[frameCount % numFramesInFlight]->get();
        param_buffer.map();
        std::memcpy(param_buffer.data(), &uniformParams, sizeof(uniformParams));
        param_buffer.unmap();

        etna::get_shader_program("inflight_frames").getDescriptorLayoutId(0),

        auto set = etna::create_descriptor_set(
          shaderInfo.getDescriptorLayoutId(0),
          currentCmdBuf,
          {
            etna::Binding{
              0,
              generatedTextureImage.genBinding(
                defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
            etna::Binding{
              1,
              loadedTextureImage1.genBinding(
                defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
          });

        currentCmdBuf.bindPipeline(
          vk::PipelineBindPoint::eGraphics, graphicsPipeline.getVkPipeline());

        currentCmdBuf.bindDescriptorSets(
          vk::PipelineBindPoint::eGraphics,
          graphicsPipeline.getVkPipelineLayout(),
          0,
          {set.getVkSet()},
          {});


        struct PushConstants
        {
          float time;
          glm::vec2 resolution;
          glm::vec2 mouse_pos;
        };
        PushConstants pushConstants;

        pushConstants.time = std::chrono::duration_cast<std::chrono::milliseconds>(
                               std::chrono::system_clock::now() - start_time)
                               .count() /
          1000.f;

        pushConstants.resolution = resolution;

        if (osWindow.get()->mouse[MouseButton::mbLeft] == ButtonState::High)
        {
          mouse_pos = osWindow.get()->mouse.freePos;
        }
        pushConstants.mouse_pos = mouse_pos;

        currentCmdBuf.pushConstants(
          graphicsPipeline.getVkPipelineLayout(),
          vk::ShaderStageFlagBits::eFragment,
          0,
          sizeof(PushConstants),
          &pushConstants);

        currentCmdBuf.draw(3, 1, 0, 0);
      }


      // At the end of "rendering", we are required to change how the pixels of the
      // swpchain image are laid out in memory to something that is appropriate
      // for presenting to the window (while preserving the content of the pixels!).
      etna::set_state(
        currentCmdBuf,
        backbuffer,
        // This looks weird, but is correct. Ask about it later.
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        {},
        vk::ImageLayout::ePresentSrcKHR,
        vk::ImageAspectFlagBits::eColor);
      // And of course flush the layout transition.
      etna::flush_barriers(currentCmdBuf);

      ETNA_READ_BACK_GPU_PROFILING(currentCmdBuf);
    }
    ETNA_CHECK_VK_RESULT(currentCmdBuf.end());

    // We are done recording GPU commands now and we can send them to be executed by the
    // GPU. Note that the GPU won't start executing our commands before the semaphore is
    // signalled, which will happen when the OS says that the next swapchain image is
    // ready.
    auto renderingDone =
      commandManager->submit(std::move(currentCmdBuf), std::move(backbufferAvailableSem));

    // Finally, present the backbuffer the screen, but only after the GPU tells the OS
    // that it is done executing the command buffer via the renderingDone semaphore.
    const bool presented = vkWindow->present(std::move(renderingDone), backbufferView);

    if (!presented)
      nextSwapchainImage = std::nullopt;
  }

  etna::end_frame();

  // After a window us un-minimized, we need to restore the swapchain to continue
  // rendering.
  if (!nextSwapchainImage && osWindow->getResolution() != glm::uvec2{0, 0})
  {
    auto [w, h] = vkWindow->recreateSwapchain(etna::Window::DesiredProperties{
      .resolution = {resolution.x, resolution.y},
      .vsync = useVsync,
    });
    ETNA_VERIFY((resolution == glm::uvec2{w, h}));
  }
}