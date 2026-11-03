#include "App.hpp"

#include <etna/Etna.hpp>


int main() {
    {
        App().Run();
    }

    if (etna::is_initilized()) etna::shutdown();

    return 0;
}
