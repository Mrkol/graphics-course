#include <glm/vec3.hpp>
#include <etna/Profiling.hpp>
#include "ParticleSystem.hpp"
#include "etna/Etna.hpp"
#include "etna/GlobalContext.hpp"
#include "etna/PipelineManager.hpp"


void ParticleSystem::ParticleEmitter::update([[maybe_unused]] glm::vec3 camera_pos, [[maybe_unused]] float delta_time) {
    accumulatedDesiredSpawn += spawnFrequency * delta_time;
    if (accumulatedDesiredSpawn >= 1.0f) {
        int particlesToSpawn = int(floor(accumulatedDesiredSpawn));
        spawnParticles(particlesToSpawn);
        accumulatedDesiredSpawn -= particlesToSpawn;
    }

    for (size_t i = 0; i < particlesVec.size(); ++i) {
        auto& p = particlesVec[i];

        p.pos += p.velocity * delta_time + acceleration * delta_time * delta_time / 2.0f;
        p.velocity += acceleration * delta_time;
        p.angle += p.rotationSpeed * delta_time;
        p.timeToLive -= delta_time;
    }
}

  // Returns the amount of alive particles.
void ParticleSystem::ParticleEmitter::sortParticles(glm::vec3 cam_pos) {
    std::sort(
        particlesVec.begin(),
        particlesVec.end(), 
        [cam_pos](const Particle& p1, const Particle& p2) {
                return (!isParticleAlive(p1) && isParticleAlive(p2)) ||
                ((p1.pos - cam_pos).length() > (p2.pos - cam_pos).length());
        }
    );
}

void ParticleSystem::ParticleEmitter::killParticle([[maybe_unused]] int index) {
    assert(isParticleAlive(index));
    particlesVec[index].timeToLive = 0;
}

void ParticleSystem::ParticleEmitter::resetParticle(ParticleSystem::ParticleEmitter::Particle& p) {
    auto r = [](){
        return float(rand()) / RAND_MAX - 0.5;
    };

    auto r0 = [](){
        return float(rand()) / RAND_MAX;
    };

    p.timeToLive = particleLifetime;
    p.pos = pos + glm::vec3(r(), r(), r()) * spawnZoneExtent;
    p.velocity = glm::vec3(r0(), r0(), r0()) * (startVelocityMax - startVelocityMin) + startVelocityMin;
    p.rotationSpeed = r0() * (rotationSpeedMax - rotationSpeedMin) + rotationSpeedMin;
}

void ParticleSystem::ParticleEmitter::spawnParticles(int count) {
    for (size_t i = 0; i < particlesVec.size() && count > 0; ++i) {
        auto& p = particlesVec[i];

        if (!isParticleAlive(int(i))) {
            resetParticle(p);
            --count;
        }
    }

    if (count > 0) {
        particlesVec.resize(particlesVec.size() + count);
        spawnParticles(count);
    }
}

bool ParticleSystem::ParticleEmitter::isParticleAlive(int index) const {
    return isParticleAlive(particlesVec[index]);
}

bool ParticleSystem::ParticleEmitter::isParticleAlive(const Particle& p) {
    return p.timeToLive > 0;
}

ParticleSystem::ParticleSystem()
    : oneShotCommands{etna::get_context().createOneShotCmdMgr()}
    , transferHelper{etna::BlockingTransferHelper::CreateInfo{.stagingSize = 65536}} {}

void ParticleSystem::setupPipeline(vk::Format swapchain_format) {
    
    etna::create_program(SHADER_NAME, {
            PARTICLES_SHADERS_ROOT "particle.vert.spv",
            PARTICLES_SHADERS_ROOT "particle.frag.spv"
        });

    auto& pipelineManager = etna::get_context().getPipelineManager();
    pipeline = pipelineManager.createGraphicsPipeline(SHADER_NAME, {
        .blendingConfig = {
            .attachments={
                vk::PipelineColorBlendAttachmentState{
                    .blendEnable = vk::True,
                    .srcColorBlendFactor = vk::BlendFactor::eSrcAlpha,
                    .dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha,
                    .colorBlendOp = vk::BlendOp::eAdd,
                    .srcAlphaBlendFactor = vk::BlendFactor::eOne,
                    .dstAlphaBlendFactor = vk::BlendFactor::eZero,
                    .alphaBlendOp = vk::BlendOp::eAdd,
                    .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                        vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
                },
            },
            .logicOpEnable = false,
            .logicOp = vk::LogicOp::eAnd,
            .blendConstants = {0, 0, 0, 0}
        },
        .fragmentShaderOutput =
        {
          .colorAttachmentFormats = {swapchain_format},
          .depthAttachmentFormat = vk::Format::eD32Sfloat,
        }
    });

    drawParamsBuffer = etna::get_context().createBuffer({
        .size = sizeof(DrawParams) * 500000,
        .bufferUsage = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer,
        .name = "draw_params_buffer"
    });
}

void ParticleSystem::sortEmitters(glm::vec3 cam_pos) {
    std::sort(emitters.begin(), emitters.end(), 
        [cam_pos](const ParticleEmitter& e1, const ParticleEmitter& e2){
            return (e1.pos - cam_pos).length() > (e2.pos - cam_pos).length();
        }
    );
}

void ParticleSystem::update([[maybe_unused]] glm::vec3 camera_pos, [[maybe_unused]] float delta_time) {
    ZoneScopedN("updateParticles");

    for (auto& e : emitters) {
        e.update(camera_pos, delta_time);
    }

    cameraPosition = camera_pos;
}

void ParticleSystem::draw(glm::mat4x4 view_proj, vk::CommandBuffer cmd_buf) {
    {
        ETNA_PROFILE_GPU(cmd_buf, bindSet);

        drawParamsDescriptorSet = etna::create_descriptor_set(
            etna::get_shader_program(SHADER_NAME).getDescriptorLayoutId(0), 
            cmd_buf, 
            {
            etna::Binding{0, drawParamsBuffer.genBinding()}
            }
        );
    
        assert(drawParamsDescriptorSet.isValid());
    
        vk::DescriptorSet vkSet = drawParamsDescriptorSet.getVkSet();
        cmd_buf.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline.getVkPipelineLayout(), 0, 1, &vkSet, 0, nullptr);
    }

    size_t totalPCount = 0;
    for (auto& e : emitters) {
        e.sortParticles(cameraPosition);
        totalPCount += e.particlesVec.size();
    }

    sortEmitters(cameraPosition);

    if (drawParams.capacity() < totalPCount) {
        drawParams.reserve(totalPCount);
    }
    drawParams.clear();

    ETNA_PROFILE_GPU(cmd_buf, drawParticles);

    for (uint32_t i = 0; i < emitters.size(); ++i) {
        const ParticleEmitter& e = emitters[i];

        cmd_buf.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.getVkPipeline());

        uint32_t firstInstance = uint32_t(drawParams.size());
        uint32_t draws = 0;

        for (size_t j = 0; j < e.particlesVec.size(); ++j) {
            auto& p = e.particlesVec[j];

            if (!e.isParticleAlive(int(j))) {
                continue;
            }

            float alpha = p.timeToLive / e.particleLifetime;
            drawParams.push_back({
                glm::vec4(p.pos, p.angle),
                e.startColor * alpha + e.endColor * (1 - alpha)
            });

            ++draws;
        }

        {
            PushConsts pc{
                glm::vec4(cameraPosition, 1),
                view_proj
            };
    
            cmd_buf.pushConstants(
                pipeline.getVkPipelineLayout(), vk::ShaderStageFlagBits::eVertex,
                0, sizeof(pc), &pc
            );
        }

        cmd_buf.draw(6, draws, 0, firstInstance);
    }

    transferHelper.uploadBuffer<DrawParams>(*oneShotCommands, drawParamsBuffer, 0, drawParams);
}
