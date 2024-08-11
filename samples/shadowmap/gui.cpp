#include "Renderer.h"

#include <imgui.h>

#include "gui/ImGuiRenderer.hpp"


void Renderer::DrawGui()
{
  guiRenderer->nextFrame();
  ImGui::NewFrame();
  {
    ImGui::Begin("Simple render settings");

    float color[3] {uniformParams.baseColor.r, uniformParams.baseColor.g, uniformParams.baseColor.b};
    ImGui::ColorEdit3("Meshes base color", color, ImGuiColorEditFlags_PickerHueWheel | ImGuiColorEditFlags_NoInputs);
    uniformParams.baseColor = {color[0], color[1], color[2]};

    float pos[3] {uniformParams.lightPos.x, uniformParams.lightPos.y, uniformParams.lightPos.z};
    ImGui::SliderFloat3("Light source position", pos, -10.f, 10.f);
    uniformParams.lightPos = {pos[0], pos[1], pos[2]};

    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

    ImGui::NewLine();

    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f),"Press 'B' to recompile and reload shaders");
    ImGui::End();
  }

  ImGui::Render();
}
