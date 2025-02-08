#include "App.hpp"


int main()
{
  {
    App app;
    app.run();
  }

  // Etna needs to be de-initialized after all resources allocated by app
  // and it's sub-fields are already freed.
  if (etna::is_initilized())
    etna::shutdown();

  return 0;
}
