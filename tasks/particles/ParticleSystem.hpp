#pragma once
#include "etna/BlockingTransferHelper.hpp"
#include "etna/GraphicsPipeline.hpp"
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/vec3.hpp>
#include <etna/Vulkan.hpp>


struct ParticleSystem {

    struct PushConsts {
        glm::vec4 camUp;
        glm::vec4 camRight;
        glm::vec4 emitterPos;
        glm::mat4x4 viewProj;
    };

    struct Vertex {
        glm::vec4 position;
    };

    struct ParticleEmitter {
        glm::vec3 pos;
        void update(glm::vec3 camera_pos, float delta_time);
        // void draw(glm::mat4x4 view_proj, vk::CommandBuffer cmd_buf) const;  should this even exist?

        struct Particle {
            // nothing for now
        };

        std::vector<Particle> particlesVec;

        void killParticle(int index);
        void spawnParticles(int count);
    };

    std::vector<ParticleEmitter> emitters;
    vk::Buffer vertexBuffer;

    ParticleSystem();

    void setupPipeline(vk::Format swapchain_format);

    void update(glm::vec3 camera_pos, float delta_time);
    void draw(glm::mat4x4 view_proj, vk::CommandBuffer cmd_buf) const;

    glm::vec3 cameraPosition;

    const char* SHADER_NAME = "particle_program";
    etna::GraphicsPipeline pipeline;
    etna::BlockingTransferHelper transferHelper;
};
