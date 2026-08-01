#include <cstdio>

#include "mega/app/Game.h"

/// Entry point for the Mega Stars Mobile Edition (iPad 9+, Redmi Note 8+).
int main(int argc, char** argv) {
    using namespace mega::app;

    Options options;
    std::string error;
    if (!parseOptions(argc, argv, Edition::Mobile, options, error)) {
        std::fprintf(stderr, "%s\n\n%s", error.c_str(), usageText(Edition::Mobile).c_str());
        return 1;
    }
    return runGame(options);
}
