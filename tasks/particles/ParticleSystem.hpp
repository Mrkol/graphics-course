#pragma once
#include <glm/vec3.hpp>
#include <etna/Vulkan.hpp>


struct ParticleSystem {
    struct ParticleEmitter {
        glm::vec3 pos;
        void update(glm::vec3 camera_pos, float delta_time);
        void draw(vk::CommandBuffer cmd_buf);

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

    void update(glm::vec3 camera_pos, float delta_time);

    void draw(vk::CommandBuffer cmd_buf);
};
