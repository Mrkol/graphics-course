#include "App.hpp"

#include <etna/Etna.hpp>
#include <spdlog/spdlog.h>


int main() {
    spdlog::flush_on(spdlog::level::err);
    {
        auto app = App();
        app.Run();
    }

    if (etna::is_initilized()) etna::shutdown();

    return 0;
}
