#include "App.hpp"

#include <etna/Etna.hpp>
#include <etna/GlobalContext.hpp>
#include <etna/PipelineManager.hpp>
#include <chrono>
#include <etna/RenderTargetStates.hpp>
#include <etna/BlockingTransferHelper.hpp>

#include <etna/Profiling.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"


App::App()
  : resolution{1280, 720}
  , useVsync{false}
  , numFramesInFlight{NUM_FRAMES_IN_FLIGHT}
  , timer{}
  , mouse{}
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
      .numFramesInFlight = numFramesInFlight,
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
  context = &etna::get_context();


  // TODO: Initialize any additional resources you require here!
  
  initialize();

  timer = std::chrono::system_clock::now();
}

static etna::Image createTexture() {
    int texWidth, texHeight, texChannels;

    stbi_uc* pixels = stbi_load(
        INFLIGHT_FRAMES_SHADERS_ROOT "../textures/test_tex_1.png",
        &texWidth,
        &texHeight,
        &texChannels,
        STBI_rgb_alpha
    );

    VkDeviceSize imageSize = texWidth * texHeight * 4;

    if (!pixels) {
        throw std::runtime_error("failed to load texture image!");
    }

    etna::Image texture = etna::get_context().createImage(etna::Image::CreateInfo{
        .extent = vk::Extent3D{static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), 1},
        .name = "texture",
        .format = vk::Format::eR8G8B8A8Unorm,
        .imageUsage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst
        }
    );

    std::unique_ptr<etna::OneShotCmdMgr> oneShotCmdMgr = etna::get_context().createOneShotCmdMgr();

    auto blockingTransferHelper = etna::BlockingTransferHelper{
        etna::BlockingTransferHelper::CreateInfo{.stagingSize = static_cast<std::uint64_t>(imageSize)}
    };
    blockingTransferHelper.uploadImage(
        *oneShotCmdMgr, 
        texture, 
        0, 
        0,
        std::span<const std::byte>(reinterpret_cast<const std::byte*>(pixels), imageSize)
    );

    stbi_image_free(pixels);
    return texture;
}

void App::initialize()
{
    frame_count = 0;
    //generated texture
    etna::create_program(
        "texture",
        {
            INFLIGHT_FRAMES_SHADERS_ROOT "texture.frag.spv",
            INFLIGHT_FRAMES_SHADERS_ROOT "toy.vert.spv"
        }
    );

    texturePipeline = etna::get_context().getPipelineManager().createGraphicsPipeline(
        "texture",
        etna::GraphicsPipeline::CreateInfo{
            .fragmentShaderOutput = {.colorAttachmentFormats = {vk::Format::eB8G8R8A8Srgb},}
        }
    );

    textureSampler = etna::Sampler{ etna::Sampler::CreateInfo{
        .addressMode = vk::SamplerAddressMode::eMirroredRepeat,
        .name = "textureSampler"
    } };

    image = context->createImage(etna::Image::CreateInfo{
        .extent = vk::Extent3D{resolution.x, resolution.y, 1},
        .name = "texture_image",
        .format = vk::Format::eB8G8R8A8Srgb,
        .imageUsage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment
        });

    // main shader
    etna::create_program(
        "ls2",
        {
            INFLIGHT_FRAMES_SHADERS_ROOT "toy.frag.spv",
            INFLIGHT_FRAMES_SHADERS_ROOT "toy.vert.spv"
        }
    );

    graphicsPipeline = context->getPipelineManager().createGraphicsPipeline(
        "ls2",
        etna::GraphicsPipeline::CreateInfo{
                .fragmentShaderOutput = {.colorAttachmentFormats = {vk::Format::eB8G8R8A8Srgb}}
        }
    );

    texture = createTexture();

    for (auto& paramBuff : arrParamBuff) {
        paramBuff = etna::get_context().createBuffer(etna::Buffer::CreateInfo{
            .size = sizeof(UniformParams),
            .bufferUsage = vk::BufferUsageFlagBits::eUniformBuffer,
            .memoryUsage = VMA_MEMORY_USAGE_CPU_ONLY,
            .name = "params",
            });
        paramBuff.map();
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
    ZoneScopedN("Frame"); 
    {
        ZoneScopedN("Poll OS events");
        windowing.poll();
    }
    processInput();
    drawFrame();
    frame_count++;
    FrameMark;
  }
  ETNA_CHECK_VK_RESULT(etna::get_context().getDevice().waitIdle());
}
void App::processInput()
{
    ZoneScoped;
    
    // press RMB to restart shader
    if (osWindow.get()->mouse[MouseButton::mbRight] == ButtonState::Rising )
    {
        const int retval = std::system("cd " GRAPHICS_COURSE_ROOT "/build"
                                            " && cmake --build . --target inflight_frames_shaders");
        if (retval != 0)
            spdlog::warn("Shader recompilation returned a non-zero return code!");
        else
        {
            ETNA_CHECK_VK_RESULT(etna::get_context().getDevice().waitIdle());
            etna::reload_shaders();
            spdlog::info("Successfully reloaded shaders!");
        }
        timer = std::chrono::system_clock::now();
    }
    // press esc to close window   
    if (osWindow.get()->keyboard[KeyboardKey::kEscape] == ButtonState::Falling) 
    {
        osWindow.get()->askToClose();
    }
    // hold LBM to rotate camera
    if (osWindow.get()->mouse[MouseButton::mbLeft] == ButtonState::High )
    {
        mouse = osWindow.get()->mouse.freePos;
    }
}

void App::drawFrame()
{
  ZoneScoped;

  auto currentCmdBuf = commandManager->acquireNext();
  etna::begin_frame();
  auto nextSwapchainImage = vkWindow->acquireNext();
  
  if (nextSwapchainImage)
  {
    auto [backbuffer, backbufferView, backbufferAvailableSem] = *nextSwapchainImage;
    auto curr_time = std::chrono::system_clock::now();
    dt = std::chrono::duration<float>(curr_time - timer).count();
    params = UniformParams{ resolution, mouse, dt};
    std::memcpy(arrParamBuff[frame_count % NUM_FRAMES_IN_FLIGHT].data(), &params, sizeof(params));
    ETNA_CHECK_VK_RESULT(currentCmdBuf.begin(vk::CommandBufferBeginInfo{}));
    {
      ETNA_PROFILE_GPU(currentCmdBuf, "Render frame");
      
      etna::set_state(
        currentCmdBuf,
        image.get(),
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::AccessFlagBits2::eColorAttachmentWrite,
        vk::ImageLayout::eColorAttachmentOptimal,
        vk::ImageAspectFlagBits::eColor
      );
      etna::flush_barriers(currentCmdBuf);
      
      {
          ETNA_PROFILE_GPU(currentCmdBuf, "texture");
          etna::RenderTargetState state(
              currentCmdBuf,
              {{}, {resolution.x, resolution.y}},
              {{image.get(), image.getView({})}},
              {}
          );
          currentCmdBuf.bindPipeline(vk::PipelineBindPoint::eGraphics, texturePipeline.getVkPipeline());
         
          struct Params{
              glm::uvec2 res;
              float time;
          };
          Params curr_params{ resolution, dt };

          currentCmdBuf.pushConstants(texturePipeline.getVkPipelineLayout(), vk::ShaderStageFlagBits::eFragment, 0, sizeof(curr_params), &curr_params);

          currentCmdBuf.draw(3, 1, 0, 0);
      }

      etna::set_state(
        currentCmdBuf,
        image.get(),
        vk::PipelineStageFlagBits2::eFragmentShader,
        vk::AccessFlagBits2::eShaderRead,
        vk::ImageLayout::eShaderReadOnlyOptimal,
        vk::ImageAspectFlagBits::eColor
      );
      etna::flush_barriers(currentCmdBuf);

      {
        ZoneScopedN("Sleep");
        std::this_thread::sleep_for(std::chrono::milliseconds(8));
      }

      {
        ETNA_PROFILE_GPU(currentCmdBuf, "ls2");

        etna::RenderTargetState state{
        currentCmdBuf,
            {{}, {resolution.x, resolution.y}},
            {{backbuffer, backbufferView}},
            {}
        };
         
        auto ls2Info = etna::get_shader_program("ls2");

        auto set = etna::create_descriptor_set(
            ls2Info.getDescriptorLayoutId(0),
            currentCmdBuf,
            {
                etna::Binding{ 0, image.genBinding(textureSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
                etna::Binding{ 1, texture.genBinding(textureSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
                etna::Binding{ 2, arrParamBuff[frame_count % NUM_FRAMES_IN_FLIGHT].genBinding()}
            }
        );
        vk::DescriptorSet vkSet = set.getVkSet();
        currentCmdBuf.bindPipeline(vk::PipelineBindPoint::eGraphics, graphicsPipeline.getVkPipeline());
        currentCmdBuf.bindDescriptorSets( 
            vk::PipelineBindPoint::eGraphics, 
            graphicsPipeline.getVkPipelineLayout(), 
            0, 
            1, 
            &vkSet, 
            0, 
            nullptr
        );
         
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
      
      ETNA_READ_BACK_GPU_PROFILING(currentCmdBuf);
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
