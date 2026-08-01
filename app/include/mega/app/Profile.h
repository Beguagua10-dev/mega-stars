#pragma once

#include <string>

namespace mega::app {

/// Which edition binary is running. Editions differ only in presentation and
/// performance budget - the simulation is identical everywhere.
enum class Edition { Pc, Mobile, Pocket };

/// Performance/quality budget applied to the renderer and the simulation rate.
struct Profile {
    Edition edition = Edition::Pc;
    int windowWidth = 1280;
    int windowHeight = 720;
    int targetFps = 60;
    int tickRate = 30;          // simulation ticks per second
    float tilePixels = 44.0f;   // zoom level
    bool particles = true;
    bool smoothCamera = true;
    bool touchControls = false;
    bool showKillFeed = true;
    int maxBots = 5;

    static Profile forEdition(Edition edition);
    static const char* editionName(Edition edition);
};

}  // namespace mega::app
