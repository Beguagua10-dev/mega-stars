#include <cstdlib>
#include <string>

#include <SDL.h>

#include "mega/app/Game.h"

#ifndef MEGA_ANDROID_EDITION_POCKET
#define MEGA_ANDROID_EDITION_POCKET 0
#endif

/// Entry point of the Android package. There is no command line here, so the
/// server address comes from the MEGA_SERVER variable when it is set and the
/// game falls back to an offline match against bots.
extern "C" int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    using namespace mega::app;

    Options options;
    options.profile = Profile::forEdition(
        MEGA_ANDROID_EDITION_POCKET ? Edition::Pocket : Edition::Mobile);
    if (const char* server = std::getenv("MEGA_SERVER")) {
        options.serverUrl = server;
    }
    return runGame(options);
}
