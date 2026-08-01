#pragma once

#include <vector>

#include "mega/Types.h"

namespace mega {

/// The roster of original Mega Stars characters.
const std::vector<BrawlerDef>& brawlerRoster();

/// Returns the definition for `id`, or the first roster entry when unknown.
const BrawlerDef& findBrawler(const std::string& id);

}  // namespace mega
