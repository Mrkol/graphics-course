#include "App.hpp"

#include <etna/Etna.hpp>


int main()
{
  {
    App app;
    app.run();
  }

  if (etna::is_initilized())
    etna::shutdown();

  return 0;
}