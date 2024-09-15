#pragma once

#include <scene/Camera.hpp>


/**
 * Contains data sent from the gameplay/logic part of the application
 * to the renderer on every frame.
 */
struct FramePacket
{
  Camera mainCam;
  Camera shadowCam;
  float currentTime = 0;
};
