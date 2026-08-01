#pragma once

#include <string>
#include <vector>

#include "mega/app/Profile.h"

namespace mega::app {

struct Options {
    Profile profile = Profile::forEdition(Edition::Pc);
    std::string playerName = "Jogador";
    std::string brawlerId = "faisca";
    std::string serverUrl;   // empty = offline match against bots
    bool headless = false;   // ASCII terminal frontend
    bool listBrawlers = false;
    bool showHelp = false;
    std::uint32_t seed = 1337;
};

/// Parses the command line shared by every native edition.
/// Unknown flags are reported through `error` and make the caller print usage.
bool parseOptions(int argc, char** argv, Edition edition, Options& out, std::string& error);

std::string usageText(Edition edition);

/// Runs one match and returns the process exit code.
int runGame(const Options& options);

}  // namespace mega::app
