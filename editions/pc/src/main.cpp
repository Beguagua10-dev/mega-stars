#include <cstdio>

#include "mega/app/Game.h"

/// Entry point for the Mega Stars PC Edition (Windows 10+, macOS 10.13+, Ubuntu).
int main(int argc, char** argv) {
    using namespace mega::app;

    Options options;
    std::string error;
    if (!parseOptions(argc, argv, Edition::Pc, options, error)) {
        std::fprintf(stderr, "%s\n\n%s", error.c_str(), usageText(Edition::Pc).c_str());
        return 1;
    }
    return runGame(options);
}
