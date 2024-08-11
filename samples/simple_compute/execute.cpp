#include "simple_compute.h"

#include <etna/Etna.hpp>


void SimpleCompute::execute()
{
  setup();

  auto cmdBuf = cmdMgr->start();

  buildCommandBuffer(cmdBuf);

  cmdMgr->submitAndWait(std::move(cmdBuf));

  readback();
}
