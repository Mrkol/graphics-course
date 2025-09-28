#include "App.hpp"

#include <etna/Etna.hpp>
#include <etna/GlobalContext.hpp>
#include <etna/PipelineManager.hpp>


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

    auto& context = etna::get_context();

    // Now we can create an OS window
    osWindow = windowing.createWindow(OsWindow::CreateInfo{
        .resolution = resolution,
    });

    // But we also need to hook the OS window up to Vulkan manually!
    {
        // First, we ask GLFW to provide a "surface" for the window,
        // which is an opaque description of the area where we can actually render.
        auto surface = osWindow->createVkSurface(context.getInstance());

        // Then we pass it to Etna to do the complicated work for us
        vkWindow = context.createWindow(etna::Window::CreateInfo{
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
    commandManager = context.createPerFrameCmdMgr();


    // TODO: Initialize any additional resources you require here!
    etna::create_program("local shadertoy", {LOCAL_SHADERTOY1_SHADERS_ROOT "toy.comp.spv"});

    result = context.createImage({
        .extent     = vk::Extent3D{resolution.x, resolution.y, 1},
        .name       = "picture",
        .format     = vk::Format::eR8G8B8A8Unorm,
        .imageUsage = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferSrc,
    });

    pipeline = context.getPipelineManager().createComputePipeline(
        "local shadertoy",
        {}
    );

    sampler = etna::Sampler({
        .name = "sampler",
    });
}

void App::update() {
    glm::vec2 mouse = osWindow.get()->mouse.freePos;

    pushConstants = PushConstants{
        .resolutionX = resolution.x,
        .resolutionY = resolution.y,
        .mouseX      = mouse.x,
        .mouseY      = mouse.y,
        .time        = std::chrono::duration<float>(std::chrono::steady_clock::now() - start).count(),
    };
}

App::~App()
{
    ETNA_CHECK_VK_RESULT(etna::get_context().getDevice().waitIdle());
}

void App::run()
{
    start = std::chrono::steady_clock::now();

    while (!osWindow->isBeingClosed())
    {
        windowing.poll();
        
        update();
        drawFrame();
    }

    // We need to wait for the GPU to execute the last frame before destroying
    // all resources and closing the application.
    ETNA_CHECK_VK_RESULT(etna::get_context().getDevice().waitIdle());
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
            // First of all, we need to "initialize" th "backbuffer", aka the current swapchain
            // image, into a state that is appropriate for us working with it. The initial state
            // is considered to be "undefined" (aka "I contain trash memory"), by the way.
            // "Transfer" in vulkanese means "copy or blit".
            // Note that Etna sometimes calls this for you to make life simpler, read Etna's code!
            etna::set_state(
                currentCmdBuf,
                backbuffer,
                // We are going to use the texture at the transfer stage...
                vk::PipelineStageFlagBits2::eTransfer,
                // ...to transfer-write stuff into it...
                vk::AccessFlagBits2::eTransferWrite,
                // ...and want it to have the appropriate layout.
                vk::ImageLayout::eTransferDstOptimal,
                vk::ImageAspectFlagBits::eColor
            );
            // The set_state doesn't actually record any commands, they are deferred to
            // the moment you call flush_barriers.
            // As with set_state, Etna sometimes flushes on it's own.
            // Usually, flushes should be placed before "action", i.e. compute dispatches
            // and blit/copy operations.
            etna::flush_barriers(currentCmdBuf);


            // TODO: Record your commands here!
            auto programInfo = etna::get_shader_program("local shadertoy");
            const auto descriptorSet = etna::create_descriptor_set(
                programInfo.getDescriptorLayoutId(0),
                currentCmdBuf,
                { etna::Binding{0, result.genBinding(sampler.get(), vk::ImageLayout::eGeneral)} }
            );

            auto vkSet = descriptorSet.getVkSet();

            currentCmdBuf.bindPipeline(
                vk::PipelineBindPoint::eCompute, 
                pipeline.getVkPipeline()
            );
            currentCmdBuf.bindDescriptorSets(
                vk::PipelineBindPoint::eCompute,
                pipeline.getVkPipelineLayout(),
                0, 
                1, 
                &vkSet, 
                0, 
                nullptr
            );
            currentCmdBuf.pushConstants(
                pipeline.getVkPipelineLayout(),
                vk::ShaderStageFlagBits::eCompute,
                0,
                sizeof(pushConstants),
                &pushConstants
            );
            etna::flush_barriers(currentCmdBuf);

            currentCmdBuf.dispatch(
                (resolution.x + 31) / 32,
                (resolution.y + 31) / 32, 
                1
            );
            etna::set_state(
                currentCmdBuf,
                result.get(),
                vk::PipelineStageFlagBits2::eTransfer,
                vk::AccessFlagBits2::eTransferRead,
                vk::ImageLayout::eTransferSrcOptimal,
                vk::ImageAspectFlagBits::eColor
            );
            etna::flush_barriers(currentCmdBuf);

            const auto subresurce = vk::ImageSubresourceLayers{vk::ImageAspectFlagBits::eColor, 0, 0, 1};
            const auto offsets    = vk::ArrayWrapper1D<vk::Offset3D, 2UL>{
                { vk::Offset3D{0, 0, 0}, vk::Offset3D{int32_t(resolution.x), int32_t(resolution.y), int32_t(1)} },
            };
            const vk::ImageBlit kRegion = {
                .srcSubresource = subresurce,
                .srcOffsets     = offsets,
                .dstSubresource = subresurce,
                .dstOffsets     = offsets,
            };

            currentCmdBuf.blitImage(
                result.get(),
                vk::ImageLayout::eTransferSrcOptimal,
                backbuffer,
                vk::ImageLayout::eTransferDstOptimal,
                1,
                &kRegion,
                vk::Filter::eLinear
            );

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
                vk::ImageAspectFlagBits::eColor
            );
            // And of course flush the layout transition.
            etna::flush_barriers(currentCmdBuf);
        }
        ETNA_CHECK_VK_RESULT(currentCmdBuf.end());

        // We are done recording GPU commands now and we can send them to be executed by the GPU.
        // Note that the GPU won't start executing our commands before the semaphore is
        // signalled, which will happen when the OS says that the next swapchain image is ready.
        auto renderingDone = commandManager->submit(std::move(currentCmdBuf), std::move(backbufferAvailableSem));

        // Finally, present the backbuffer the screen, but only after the GPU tells the OS
        // that it is done executing the command buffer via the renderingDone semaphore.
        const bool presented = vkWindow->present(std::move(renderingDone), backbufferView);

        if (!presented) nextSwapchainImage = std::nullopt;
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
