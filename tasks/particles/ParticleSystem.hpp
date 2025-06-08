#pragma once
#include "etna/BlockingTransferHelper.hpp"
#include "etna/GraphicsPipeline.hpp"
#include "etna/DescriptorSet.hpp"
#include <filesystem>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/vec3.hpp>
#include <etna/Vulkan.hpp>


struct ParticleSystem {
    struct DrawParams {
        glm::vec4 posAndAngle;
        glm::vec4 color;
    };

    struct PushConsts {
        glm::vec4 camPos;
        glm::mat4x4 viewProj;
    };

    struct ParticleEmitter {
        // std::filesystem::path texture_path;
        // void loadTextureByPath();

        glm::vec3 pos;
        glm::vec3 startVelocityMax;
        glm::vec3 startVelocityMin;
        float accumulatedDesiredSpawn;  // When spawn frequency is low, this thing slowly accumulates.
        float spawnFrequency = 100.0f;
        float particleLifetime = 1.0f;
        float rotationSpeedMin = -1.0f;
        float rotationSpeedMax = 1.0f;
        glm::vec3 spawnZoneExtent = glm::vec3(1);
        glm::vec3 acceleration;
        glm::vec4 startColor{0.0f, 0.0f, 1.0f, 1.0f};
        glm::vec4 endColor{1.0f, 0.0f, 0.0f, 0.0f};
        void update(glm::vec3 camera_pos, float delta_time);

        struct Particle {
            glm::vec3 pos;
            glm::vec3 velocity;
            float angle;
            float rotationSpeed;
            float timeToLive;
        };

        std::vector<Particle> particlesVec;

        void sortParticles(glm::vec3 cam_pos);  // Returns the amount of alive particles.
        void killParticle(int index);
        void spawnParticles(int count);
        void resetParticle(Particle& p);
        bool isParticleAlive(int index) const;
        static bool isParticleAlive(const Particle& p);
    };

    std::unique_ptr<etna::OneShotCmdMgr> oneShotCommands;

    std::vector<ParticleEmitter> emitters;
    std::vector<DrawParams> drawParams;
    etna::Buffer drawParamsBuffer;
    etna::DescriptorSet drawParamsDescriptorSet;

    ParticleSystem();

    void setupPipeline(vk::Format swapchain_format);

    void update(glm::vec3 camera_pos, float delta_time);
    void draw(glm::mat4x4 view_proj, vk::CommandBuffer cmd_buf);

    void sortEmitters(glm::vec3 cam_pos);

    glm::vec3 cameraPosition;

    const char* SHADER_NAME = "particle_program";
    etna::GraphicsPipeline pipeline;
    etna::BlockingTransferHelper transferHelper;
};
