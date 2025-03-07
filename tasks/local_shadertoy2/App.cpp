#include "App.hpp"

#include <etna/Etna.hpp>
#include <etna/GlobalContext.hpp>
#include <etna/PipelineManager.hpp>
#include <etna/BlockingTransferHelper.hpp>
#include <etna/RenderTargetStates.hpp>

#include <stb_image.h>

#include <chrono>
#include <iostream>

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
    // Generally, in Vulkan, we call the GPU a "device" and the CPU/OS combination a "host."
    std::vector<const char*> deviceExtensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    // Etna does all of the Vulkan initialization heavy lifting.
    // You can skip figuring out how it works for now.
    etna::initialize(etna::InitParams{
      .applicationName = "Local Shadertoy",
      .applicationVersion = VK_MAKE_VERSION(0, 1, 0),
      .instanceExtensions = instanceExtensions,
      .deviceExtensions = deviceExtensions,
      // Replace with an index if etna detects your preferred GPU incorrectly
      .physicalDeviceIndexOverride = {},
      .numFramesInFlight = 1,
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

  sampler = etna::Sampler(etna::Sampler::CreateInfo{
		  .name = "sampler"
  });

  gtxt = loadTexture(GRAPHICS_COURSE_RESOURCES_ROOT "/scenes/lovely_town/textures/material_8_metallicRoughness.png", "gTexture");
  skytxt = loadTexture(GRAPHICS_COURSE_RESOURCES_ROOT "/textures/test_tex_1.png", "skyTexture");

  auto& ctx = etna::get_context();
  auto& manager = ctx.getPipelineManager();

  etna::create_program(
    "toy",
    {
      LOCAL_SHADERTOY2_SHADERS_ROOT "rect.vert.spv",
      LOCAL_SHADERTOY2_SHADERS_ROOT "toy.frag.spv"
    });

  toyPipeline = { };
  toyPipeline = manager.createGraphicsPipeline(
    "toy",
    {
      etna::GraphicsPipeline::CreateInfo {
        .fragmentShaderOutput = {
          .colorAttachmentFormats = {
	    vk::Format::eB8G8R8A8Srgb
	  },
        }
      }
    }
  );

  etna::create_program(
    "gen",
    {
      LOCAL_SHADERTOY2_SHADERS_ROOT "rect.vert.spv",
      LOCAL_SHADERTOY2_SHADERS_ROOT "gen.frag.spv",
    });

  genPipeline = manager.createGraphicsPipeline(
    "gen",
    {
      etna::GraphicsPipeline::CreateInfo {
        .fragmentShaderOutput = {
          .colorAttachmentFormats = {
	    vk::Format::eB8G8R8A8Srgb
	  },
        }
      }
     });

  etna::Image::CreateInfo info{
    .extent =
      vk::Extent3D{
        .width = 1280,
        .height = 720,
        .depth = 1,
      },
    .name = "genTexture",
    .format = vk::Format::eB8G8R8A8Srgb,
    .imageUsage = vk::ImageUsageFlagBits::eSampled |
	    	  vk::ImageUsageFlagBits::eColorAttachment
  };
  gentxt = etna::get_context().createImage(info);
}

etna::Image App::loadTexture(const std::string &path, const std::string &texture_name)
{
  int x, y, channels;
  void *imageData = static_cast<void*>(stbi_load(path.c_str(), &x, &y, &channels, 4));
  if (!imageData)
  {
    std::cerr << "Error loading \"" << texture_name << "\" (" << path.c_str() << "): " << stbi_failure_reason() << std::endl;
    std::terminate();
  }

  etna::Image::CreateInfo info{
    .extent = vk::Extent3D {
        .width = static_cast<uint32_t>(x),
        .height = static_cast<uint32_t>(y),
        .depth = 1,
    },
    .name = texture_name,
    .format = vk::Format::eR8G8B8A8Unorm,
    .imageUsage = vk::ImageUsageFlagBits::eSampled |
	          vk::ImageUsageFlagBits::eTransferDst,
  };

  etna::Image img = etna::get_context().createImage(info);

  auto buf = commandManager->acquireNext();
  ETNA_CHECK_VK_RESULT(buf.begin(vk::CommandBufferBeginInfo{}));

  std::unique_ptr<etna::OneShotCmdMgr> mgr = etna::get_context().createOneShotCmdMgr();
  etna::BlockingTransferHelper({static_cast<vk::DeviceSize>(x * y * sizeof(uint32_t))})
    .uploadImage(
      *mgr,
      img,
      0,
      0,
      std::span<std::byte>(static_cast<std::byte*>(imageData), x * y * sizeof(uint32_t)));
  stbi_image_free(imageData);
  ETNA_CHECK_VK_RESULT(buf.end());

  buf = commandManager->acquireNext();
  ETNA_CHECK_VK_RESULT(buf.begin(vk::CommandBufferBeginInfo{}));

  etna::set_state(
    buf,
    img.get(),
    vk::PipelineStageFlagBits2::eFragmentShader,
    {vk::AccessFlagBits2::eShaderRead},
    vk::ImageLayout::eShaderReadOnlyOptimal,
    vk::ImageAspectFlagBits::eColor);
  etna::flush_barriers(buf);
  ETNA_CHECK_VK_RESULT(buf.end());

  return img;
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

  // We need to wait for the GPU to execute the last frame before destroying
  // all resources and closing the application.
  ETNA_CHECK_VK_RESULT(etna::get_context().getDevice().waitIdle());
}

auto App::getParams()
{
  static auto start = std::chrono::steady_clock::now();
  auto end = std::chrono::steady_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() / 1000.f;
 
  struct { struct {
	  uint32_t x;
	  uint32_t y;
  	} resolution;
        float time;
  } params = {{resolution.x, resolution.y}, elapsed};

  return params;
}

void App::drawFrame()
{
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

      auto genImg = gentxt.get();
      auto genView = gentxt.getView({});

      {
      /* Block for RenderTargetState sync. */
      etna::RenderTargetState state {
        currentCmdBuf,
	{{ }, {1280, 720}}, 
	{{genImg, genView}}, 
	{ },
      };
      currentCmdBuf.bindPipeline(vk::PipelineBindPoint::eGraphics, genPipeline.getVkPipeline());
      
      auto genInfo = etna::get_shader_program("gen");
      auto descriptor = etna::create_descriptor_set(
        genInfo.getDescriptorLayoutId(0),
	currentCmdBuf,
	{
	  etna::Binding {
      	    0,
	    gtxt.genBinding(sampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)
	  },
	}
      );

      auto vkSet = descriptor.getVkSet();

      currentCmdBuf.bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics,
	genPipeline.getVkPipelineLayout(),
	0,
	1,
	&vkSet,
	0,
	NULL
      );
 
      auto params = getParams();
      currentCmdBuf.pushConstants(genPipeline.getVkPipelineLayout(), 
		      		  vk::ShaderStageFlagBits::eFragment,
		     		  0, sizeof(params), &params);
      currentCmdBuf.draw(3, 1, 0, 0);

      etna::set_state(
        currentCmdBuf,
	genImg,
	vk::PipelineStageFlagBits2::eFragmentShader,
	vk::AccessFlagBits2::eShaderRead,
	vk::ImageLayout::eShaderReadOnlyOptimal,
	vk::ImageAspectFlagBits::eColor
      );
      }
      
      {
      auto toyInfo = etna::get_shader_program("toy");
      etna::RenderTargetState toyState = {
        currentCmdBuf,
	{{ }, {resolution.x, resolution.y}},
	{{backbuffer, backbufferView}},
	{ },
      };

      currentCmdBuf.bindPipeline(vk::PipelineBindPoint::eGraphics, toyPipeline.getVkPipeline());
      
      auto descriptor = etna::create_descriptor_set(
        toyInfo.getDescriptorLayoutId(0),
	currentCmdBuf,
	{
	  etna::Binding {
	    0,
	    gentxt.genBinding(sampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)
	  },
	  etna::Binding {
      	    1,
	    gtxt.genBinding(sampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)
	  },
	  etna::Binding {
	    2,
	    skytxt.genBinding(sampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)
	  },
	}
      );

      auto vkSet = descriptor.getVkSet();

      currentCmdBuf.bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics,
	toyPipeline.getVkPipelineLayout(),
	0,
	1,
	&vkSet,
	0,
	NULL
      );
      
      auto params = getParams();
      currentCmdBuf.pushConstants(toyPipeline.getVkPipelineLayout(), 
		      		  vk::ShaderStageFlagBits::eFragment,
		     		  0, sizeof(params), &params);
      currentCmdBuf.draw(3, 1, 0, 0);
      }

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
    }
    ETNA_CHECK_VK_RESULT(currentCmdBuf.end());

    // We are done recording GPU commands now and we can send them to be executed by the GPU.
    // Note that the GPU won't start executing our commands before the semaphore is
    // signalled, which will happen when the OS says that the next swapchain image is ready.
    auto renderingDone =
      commandManager->submit(std::move(currentCmdBuf), std::move(backbufferAvailableSem));

    // Finally, present the backbuffer the screen, but only after the GPU tells the OS
    // that it is done executing the command buffer via the renderingDone semaphore.
    const bool presented = vkWindow->present(std::move(renderingDone), backbufferView);

    if (!presented)
      nextSwapchainImage = std::nullopt;
  }

  etna::end_frame();

  // After a window us un-minimized, we need to restore the swapchain to continue rendering.
  if (!nextSwapchainImage && osWindow->getResolution() != glm::uvec2{0, 0})
  {
    auto [w, h] = vkWindow->recreateSwapchain(etna::Window::DesiredProperties{
      .resolution = {resolution.x, resolution.y},
      .vsync = useVsync,
    });
    ETNA_VERIFY((resolution == glm::uvec2{w, h}));
  }
}
