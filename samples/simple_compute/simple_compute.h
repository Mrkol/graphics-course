#ifndef SIMPLE_COMPUTE_H
#define SIMPLE_COMPUTE_H

#include <memory>

#include <etna/GlobalContext.hpp>
#include <etna/ComputePipeline.hpp>
#include <etna/OneShotCmdMgr.hpp>
#include <etna/BlockingTransferHelper.hpp>


class SimpleCompute
{
public:
  SimpleCompute();

  void init();
  void execute();

  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
private:
  etna::GlobalContext *context;

  std::unique_ptr<etna::OneShotCmdMgr> cmdMgr;
  std::unique_ptr<etna::BlockingTransferHelper> transferHelper;

  std::uint32_t length;

  etna::ComputePipeline pipeline;

  etna::Buffer bufA;
  etna::Buffer bufB;
  etna::Buffer bufResult;

  void setup();
  void buildCommandBuffer(vk::CommandBuffer cmd_buf);
  void readback();
};


#endif //SIMPLE_COMPUTE_H
