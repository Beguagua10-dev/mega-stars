#include <cstdio>

#include "mega/app/Game.h"

/// Entry point for the Mega Stars Pocket Edition (Galaxy J5 Prime, iPhone 6).
int main(int argc, char** argv) {
    using namespace mega::app;

    Options options;
    std::string error;
    if (!parseOptions(argc, argv, Edition::Pocket, options, error)) {
        std::fprintf(stderr, "%s\n\n%s", error.c_str(), usageText(Edition::Pocket).c_str());
        return 1;
    }
    return runGame(options);
}
