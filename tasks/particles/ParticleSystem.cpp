#include <glm/vec3.hpp>
#include "ParticleSystem.hpp"
#include "etna/Etna.hpp"
#include "etna/GlobalContext.hpp"
#include "etna/PipelineManager.hpp"


void ParticleSystem::ParticleEmitter::update([[maybe_unused]] glm::vec3 camera_pos, [[maybe_unused]] float delta_time) {
    for ([[maybe_unused]] auto& p : particlesVec) {
        // TODO
    }
}

// void ParticleSystem::ParticleEmitter::draw(glm::mat4x4 view_proj, vk::CommandBuffer cmd_buf) const {
//     // make quad
//     // upload it
//     // upload view_proj
//     // make descriptor set
//     // draw quad using a shader
//     cmd_buf.draw(4, 1, 0, 0);
// }

void ParticleSystem::ParticleEmitter::killParticle([[maybe_unused]] int index) {
    // TODO
}

void ParticleSystem::ParticleEmitter::spawnParticles(int count) {
    for (int i = 0; i < count; ++i) {
        // TODO
        // probably mark a buncha particles
    }
}

ParticleSystem::ParticleSystem()
    : transferHelper{etna::BlockingTransferHelper::CreateInfo{.stagingSize = 65536 * 4}} {}

void ParticleSystem::setupPipeline(vk::Format swapchain_format) {
    
    etna::create_program(SHADER_NAME, {
            PARTICLES_SHADERS_ROOT "particle.vert.spv",
            PARTICLES_SHADERS_ROOT "particle.frag.spv"
        });

    // DO NOT DELETE THE FOLLOWING, it will be used when we get to actual particles
    // etna::VertexShaderInputDescription vertexInputDesc{
    //     {
    //         etna::VertexShaderInputDescription::Binding{
    //             .byteStreamDescription=etna::VertexByteStreamFormatDescription{
    //                 .stride = sizeof(Vertex),
    //                 .attributes = {
    //                     etna::VertexByteStreamFormatDescription::Attribute{
    //                         .format = vk::Format::eR32G32B32A32Sfloat,
    //                         .offset = 0,
    //                     },
    //                     etna::VertexByteStreamFormatDescription::Attribute{
    //                         .format = vk::Format::eR32G32B32A32Sfloat,
    //                         .offset = sizeof(glm::vec4),
    //                     },
    //                 }
    //             }
    //         }
    //     }
    // };

    auto& pipelineManager = etna::get_context().getPipelineManager();
    pipeline = pipelineManager.createGraphicsPipeline(SHADER_NAME, {
        // .vertexShaderInput=vertexInputDesc,    // this will be added when I get to actual particles
        // blending config is done here
        .fragmentShaderOutput =
        {
          .colorAttachmentFormats = {swapchain_format},
          .depthAttachmentFormat = vk::Format::eD32Sfloat,
        }
    });
}

void ParticleSystem::update([[maybe_unused]] glm::vec3 camera_pos, [[maybe_unused]] float delta_time) {
    // TODO

    cameraPosition = camera_pos;
}

void ParticleSystem::draw(glm::mat4x4 view_proj, vk::CommandBuffer cmd_buf) const {
    // TODO: add sorting here

    for (const auto& e : emitters) {
        cmd_buf.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.getVkPipeline());

        {
            glm::vec3 look = cameraPosition - e.pos;
            glm::vec3 camRight = glm::normalize(glm::cross(glm::vec3(0, 1, 0), look));
            glm::vec3 camUp    = glm::normalize(glm::cross(camRight, look));

            PushConsts pc{
                glm::vec4(camUp, 1),
                glm::vec4(camRight, 1),
                glm::vec4(e.pos, 1),
                view_proj
            };
        
            cmd_buf.pushConstants(
                pipeline.getVkPipelineLayout(), vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
                0, sizeof(pc), &pc
            );
        }

        cmd_buf.draw(6, 1, 0, 0);
    }
}
