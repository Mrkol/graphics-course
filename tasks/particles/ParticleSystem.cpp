#include <glm/vec3.hpp>
#include "ParticleSystem.hpp"


void ParticleSystem::ParticleEmitter::update([[maybe_unused]] glm::vec3 camera_pos, [[maybe_unused]] float delta_time) {
    for ([[maybe_unused]] auto& p : particlesVec) {
        // TODO
    }
}

void ParticleSystem::ParticleEmitter::draw([[maybe_unused]] vk::CommandBuffer cmd_buf) {
    // TODO
}

void ParticleSystem::ParticleEmitter::killParticle([[maybe_unused]] int index) {
    // TODO
}

void ParticleSystem::ParticleEmitter::spawnParticles(int count) {
    for (int i = 0; i < count; ++i) {
        // TODO
    }
}

ParticleSystem::ParticleSystem() {
    // TODO
}

void ParticleSystem::update([[maybe_unused]] glm::vec3 camera_pos, [[maybe_unused]] float delta_time) {
    // TODO
}

void ParticleSystem::draw([[maybe_unused]] vk::CommandBuffer cmd_buf) {
    // TODO
}
