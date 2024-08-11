#include "simple_compute.h"
#include <etna/Etna.hpp>

int main()
{
  {
    SimpleCompute app;

    app.init();
    app.execute();
  }

  if (etna::is_initilized())
    etna::shutdown();

  return 0;
}
