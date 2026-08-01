#include "mega/app/Profile.h"

namespace mega::app {

Profile Profile::forEdition(Edition edition) {
    Profile p;
    p.edition = edition;
    switch (edition) {
        case Edition::Pc:
            break;

        case Edition::Mobile:
            // Baseline: iPad 9, Redmi Note 8.
            p.windowWidth = 1280;
            p.windowHeight = 720;
            p.targetFps = 60;
            p.tickRate = 30;
            p.tilePixels = 40.0f;
            p.touchControls = true;
            break;

        case Edition::Pocket:
            // Baseline: Galaxy J5 Prime, iPhone 6. Everything expensive is off
            // and the simulation runs at half the tick rate of the PC build.
            p.windowWidth = 960;
            p.windowHeight = 540;
            p.targetFps = 30;
            p.tickRate = 15;
            p.tilePixels = 32.0f;
            p.particles = false;
            p.smoothCamera = false;
            p.touchControls = true;
            p.showKillFeed = false;
            p.maxBots = 3;
            break;
    }
    return p;
}

const char* Profile::editionName(Edition edition) {
    switch (edition) {
        case Edition::Pc:
            return "PC Edition";
        case Edition::Mobile:
            return "Mobile Edition";
        case Edition::Pocket:
            return "Pocket Edition";
    }
    return "Unknown Edition";
}

}  // namespace mega::app
