#pragma once

#include <glm/glm.hpp>

#include <etna/ComputePipeline.hpp>
#include <etna/DescriptorSet.hpp>
#include <etna/Image.hpp>
#include <etna/Sampler.hpp>


class Skybox {
public:
    void Load();
    void DrawGui();
    void BakeIfDirty();

    glm::vec4 GetSunDirectionIntensity() const;
    glm::vec3 GetSunColor() const { return bakedParameters.sunColor; }
    etna::Binding GetBinding() const;

private:
    struct Parameters {
        float sunAzimuth = -135.0f;
        float sunElevation = 14.0f;
        glm::vec3 sunColor{1.0f, 0.92f, 0.8f};
        float sunIntensity = 3.0f;
        float coverage = 0.40f;
        float density = 0.028f;
        float scale = 0.0015f;
        float layerBase = 220.0f;
        float layerTop = 430.0f;
        int primarySteps = 48;
        int shadowSteps = 6;

        bool operator==(const Parameters&) const = default;
    };

    static glm::vec4 SunDirectionIntensity(const Parameters& settings);
    void GenerateNoise();
    void Bake();

    etna::Image cubemap;
    etna::Image noise;
    etna::Sampler cubemapSampler;
    etna::Sampler noiseSampler;
    etna::ComputePipeline bakePipeline;
    etna::PersistentDescriptorSet bakeDescriptors;
    Parameters parameters;
    Parameters bakedParameters;
    bool editing = false;
};
