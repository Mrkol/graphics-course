#include "Skybox.hpp"
#include "StartupCommands.hpp"

#include <cmath>

#include <imgui.h>
#include <spdlog/spdlog.h>

#include <etna/Etna.hpp>
#include <etna/GlobalContext.hpp>
#include <etna/PipelineManager.hpp>


namespace {

constexpr uint32_t faceSize = 2048;
constexpr uint32_t faceCount = 6;
constexpr uint32_t noiseSize = 64;
constexpr uint32_t rowsPerDispatch = 128;
constexpr float maxMarchDistance = 7500.0f;
constexpr vk::ImageLayout readOnly = vk::ImageLayout::eShaderReadOnlyOptimal;

struct BakeConstants {
    glm::vec4 sunDirectionIntensity;
    glm::vec4 sunColor;
    glm::vec4 clouds;
    glm::vec4 layer;
    glm::ivec4 steps;
};
static_assert(sizeof(BakeConstants) == 80);

}

void Skybox::Load() {
    auto& context = etna::get_context();
    constexpr auto imageUsage = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled;
    cubemap = context.createImage({
        .extent = {faceSize, faceSize, 1},
        .name = "procedural sky cubemap",
        .format = vk::Format::eR16G16B16A16Sfloat,
        .imageUsage = imageUsage,
        .layers = faceCount,
        .flags = vk::ImageCreateFlagBits::eCubeCompatible,
    });
    noise = context.createImage({
        .extent = {noiseSize, noiseSize, noiseSize},
        .name = "cloud noise volume",
        .format = vk::Format::eR32Sfloat,
        .imageUsage = imageUsage,
        .type = vk::ImageType::e3D,
    });
    cubemapSampler = etna::Sampler({
        .filter = vk::Filter::eLinear,
        .addressMode = vk::SamplerAddressMode::eClampToEdge,
        .name = "sky sampler",
        .maxLod = 0.0f,
    });
    noiseSampler = etna::Sampler({
        .filter = vk::Filter::eLinear,
        .addressMode = vk::SamplerAddressMode::eRepeat,
        .name = "cloud noise sampler",
        .maxLod = 0.0f,
    });

    GenerateNoise();
    etna::create_program("sky bake", {SKYBOX_SHADERS_ROOT "sky_bake.comp.spv"});
    bakePipeline = context.getPipelineManager().createComputePipeline("sky bake", {});
    auto programInfo = etna::get_shader_program("sky bake");
    bakeDescriptors = etna::create_persistent_descriptor_set(
        programInfo.getDescriptorLayoutId(0),
        {
            etna::Binding{0, cubemap.genBinding({}, vk::ImageLayout::eGeneral, {.type = vk::ImageViewType::e2DArray})},
            etna::Binding{1, noise.genBinding(noiseSampler.get(), readOnly)},
        }
    );
    Bake();
}

void Skybox::GenerateNoise() {
    auto& context = etna::get_context();
    etna::create_program("sky noise", {SKYBOX_SHADERS_ROOT "sky_noise.comp.spv"});
    auto pipeline = context.getPipelineManager().createComputePipeline("sky noise", {});

    auto startup = StartupCommands(context);
    auto cmd = startup.Begin();
    auto programInfo = etna::get_shader_program("sky noise");
    auto descriptors = etna::create_descriptor_set(
        programInfo.getDescriptorLayoutId(0), cmd,
        {etna::Binding{0, noise.genBinding({}, vk::ImageLayout::eGeneral)}}
    );
    etna::flush_barriers(cmd);
    cmd.bindPipeline(vk::PipelineBindPoint::eCompute, pipeline.getVkPipeline());
    cmd.bindDescriptorSets(
        vk::PipelineBindPoint::eCompute, pipeline.getVkPipelineLayout(), 0, {descriptors.getVkSet()}, {}
    );
    cmd.dispatch(noiseSize / 4, noiseSize / 4, noiseSize / 4);

    etna::set_state(
        cmd, noise.get(), vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderSampledRead,
        readOnly, vk::ImageAspectFlagBits::eColor
    );
    etna::flush_barriers(cmd);
    startup.SubmitAndWait();
}

void Skybox::DrawGui() {
    auto displaySize = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos({displaySize.x - 16.0f, 16.0f}, ImGuiCond_FirstUseEver, {1.0f, 0.0f});
    ImGui::SetNextWindowSize({360.0f, 0.0f}, ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Procedural sky")) {
        constexpr auto clamp = ImGuiSliderFlags_AlwaysClamp;
        ImGui::SliderFloat("Sun azimuth", &parameters.sunAzimuth, -180.0f, 180.0f, "%.0f deg", clamp);
        ImGui::SliderFloat("Sun elevation", &parameters.sunElevation, 0.0f, 90.0f, "%.0f deg", clamp);
        ImGui::ColorEdit3("Sun color", &parameters.sunColor.x, ImGuiColorEditFlags_Float);
        ImGui::SliderFloat("Sun intensity", &parameters.sunIntensity, 0.0f, 12.0f, "%.2f", clamp);

        ImGui::SeparatorText("Clouds");
        ImGui::SliderFloat("Coverage", &parameters.coverage, 0.0f, 1.0f, "%.2f", clamp);
        ImGui::SliderFloat("Density", &parameters.density, 0.0f, 0.10f, "%.3f", clamp);
        ImGui::SliderFloat("Noise scale", &parameters.scale, 0.0003f, 0.008f, "%.4f", clamp);
        ImGui::SliderFloat("Layer base", &parameters.layerBase, 20.0f, parameters.layerTop - 30.0f, "%.0f m", clamp);
        ImGui::SliderFloat("Layer top", &parameters.layerTop, parameters.layerBase + 30.0f, 1600.0f, "%.0f m", clamp);
        ImGui::SliderInt("Primary steps", &parameters.primarySteps, 4, 64, "%d", clamp);
        ImGui::SliderInt("Shadow steps", &parameters.shadowSteps, 0, 12, "%d", clamp);
    }
    ImGui::End();
    editing = ImGui::IsAnyItemActive();
}

void Skybox::BakeIfDirty() {
    if (!editing && parameters != bakedParameters) Bake();
}

glm::vec4 Skybox::SunDirectionIntensity(const Parameters& settings) {
    auto azimuth = glm::radians(settings.sunAzimuth);
    auto elevation = glm::radians(settings.sunElevation);
    auto horizontal = std::cos(elevation);
    return {horizontal * std::sin(azimuth), std::sin(elevation), horizontal * std::cos(azimuth), settings.sunIntensity};
}

glm::vec4 Skybox::GetSunDirectionIntensity() const {
    return SunDirectionIntensity(bakedParameters);
}

etna::Binding Skybox::GetBinding() const {
    return etna::Binding{4, cubemap.genBinding(cubemapSampler.get(), readOnly, {.type = vk::ImageViewType::eCube})};
}

void Skybox::Bake() {
    auto startup = StartupCommands(etna::get_context());
    auto cmd = startup.Begin();
    etna::set_state(
        cmd, cubemap.get(), vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderStorageWrite,
        vk::ImageLayout::eGeneral, vk::ImageAspectFlagBits::eColor
    );
    etna::flush_barriers(cmd);

    auto layout = bakePipeline.getVkPipelineLayout();
    cmd.bindPipeline(vk::PipelineBindPoint::eCompute, bakePipeline.getVkPipeline());
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, layout, 0, {bakeDescriptors.getVkSet()}, {});
    auto constants = BakeConstants{
        .sunDirectionIntensity = SunDirectionIntensity(parameters),
        .sunColor = {parameters.sunColor, 0.0f},
        .clouds = {parameters.coverage, parameters.density, parameters.scale, 0.0f},
        .layer = {parameters.layerBase, parameters.layerTop, maxMarchDistance, 0.0f},
        .steps = {parameters.primarySteps, parameters.shadowSteps, 0, 0},
    };

    for (uint32_t firstRow = 0; firstRow < faceSize; firstRow += rowsPerDispatch) {
        constants.steps.z = static_cast<int>(firstRow);
        cmd.pushConstants(layout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(constants), &constants);
        cmd.dispatch(faceSize / 8, rowsPerDispatch / 8, faceCount);
    }

    etna::set_state(
        cmd, cubemap.get(), vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderSampledRead,
        readOnly, vk::ImageAspectFlagBits::eColor
    );
    etna::flush_barriers(cmd);
    startup.SubmitAndWait();
    bakedParameters = parameters;

    spdlog::info("Baked sky cubemap");
}
