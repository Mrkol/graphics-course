#pragma once

#include <scene/Camera.hpp>


struct FramePacket
{
  Camera mainCam;
  float currentTime = 0;
};
