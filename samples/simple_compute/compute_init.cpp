#include "simple_compute.h"

#include <etna/Etna.hpp>


void SimpleCompute::init()
{
  etna::initialize(
    etna::InitParams{
      .applicationName = "ComputeSample", .applicationVersion = VK_MAKE_VERSION(0, 1, 0),
      // Uncomment if etna selects the incorrect GPU for you
      // .physicalDeviceIndexOverride = 0,
    });

  context = &etna::get_context();

  cmdMgr = context->createOneShotCmdMgr();

  transferHelper =
    std::make_unique<etna::BlockingTransferHelper>(etna::BlockingTransferHelper::CreateInfo{
      .stagingSize = static_cast<std::uint32_t>(length * sizeof(float)),
    });
}
