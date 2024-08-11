#include "Renderer.h"

#include <etna/Etna.hpp>
#include <glm/ext.hpp>


void Renderer::updateView(const Camera &main, const Camera &shadow)
{
  ///// calc camera matrix

  // TODO: coordinate systems are all over the place in the sample, investigate and fix properly
  glm::mat4x4 mFlipY = glm::identity<glm::mat4x4>();
  mFlipY[1][1] = -1;

  glm::mat4x4 mFlipX = glm::identity<glm::mat4x4>();
  mFlipX[0][0] = -1;

  {
    const float aspect = float(resolution.x) / float(resolution.y);
    worldViewProj = mFlipY * mFlipX * main.projTm(aspect) * main.viewTm();
  }

  ///// calc light matrix

  {
    const auto mProj =
      lightProps.usePerspectiveM ?
        glm::perspectiveLH_ZO(shadow.fov, 1.0f, 1.0f, lightProps.lightTargetDist*2.0f) :
        glm::orthoLH_ZO(-lightProps.radius, +lightProps.radius, -lightProps.radius, +lightProps.radius, 0.0f, lightProps.lightTargetDist);

    lightMatrix = mFlipX * mProj * shadow.viewTm();

    lightPos = shadow.position;
  }
}

void Renderer::updateUniformBuffer(float time)
{
  uniformParams.lightMatrix = lightMatrix;
  uniformParams.lightPos    = lightPos;
  uniformParams.time        = time;

  std::memcpy(constants.data(), &uniformParams, sizeof(uniformParams));
}

void Renderer::debugInput(const Keyboard &kb)
{
  if(kb[KeyboardKey::kQ] == ButtonState::Falling)
    drawDebugFSQuad = !drawDebugFSQuad;

  if(kb[KeyboardKey::kP] == ButtonState::Falling)
    lightProps.usePerspectiveM = !lightProps.usePerspectiveM;

  if(kb[KeyboardKey::kB] == ButtonState::Falling)
  {
#ifdef WIN32
    std::system("cd " GRAPHICS_COURSE_ROOT "/resources/shaders && python compile_shadowmap_shaders.py");
#else
    std::system("cd " GRAPHICS_COURSE_ROOT "/resources/shaders && python3 compile_shadowmap_shaders.py");
#endif
    ETNA_CHECK_VK_RESULT(context->getDevice().waitIdle());
    etna::reload_shaders();
  }
}
