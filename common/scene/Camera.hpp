#pragma once

#include <glm/glm.hpp>
#include <glm/ext.hpp>


struct Camera
{
  glm::vec3 position;
  glm::quat rotation;
  float fov{60};
  float zNear{0.01f};
  float zFar{1000};

  void lookAt(glm::vec3 from, glm::vec3 to, glm::vec3 up)
  {
    position = from;
    rotation = glm::quatLookAtLH(normalize(to - from), normalize(up));
  }

  void rotate(float up_angle_deg, float right_angle_deg)
  {
    glm::quat yaw = glm::angleAxis(glm::radians(right_angle_deg), glm::vec3{0, -1, 0});
    glm::quat pitch = glm::angleAxis(glm::radians(up_angle_deg), glm::vec3{1, 0, 0});
    rotation = yaw * rotation * pitch;
  }

  void move(glm::vec3 offset) { position += offset; }

  const glm::vec3 right() const { return rotation * glm::vec3{-1, 0, 0}; }

  const glm::vec3 up() const { return rotation * glm::vec3{0, 1, 0}; }

  const glm::vec3 forward() const { return rotation * glm::vec3{0, 0, 1}; }

  glm::mat4x4 viewItm() const
  {
    return translate(glm::identity<glm::mat4>(), position) * mat4_cast(rotation);
  }

  glm::mat4x4 viewTm() const { return inverse(viewItm()); }

  glm::mat4x4 projTm(float aspect) const
  {
    return glm::perspectiveLH_ZO(-glm::radians(fov), aspect, zNear, zFar);
  }
};
