#pragma once

#include <deque>
#include <memory>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

#include "mega/Arena.h"
#include "mega/Brawlers.h"
#include "mega/Snapshot.h"
#include "mega/Types.h"

namespace mega {

struct MatchRules {
    int gemsToWin = 10;
    float countdownSeconds = 15.0f;
    float matchSeconds = 150.0f;
    float gemSpawnSeconds = 2.5f;
    float respawnSeconds = 3.0f;
    int playersPerTeam = 3;
};

struct WorldConfig {
    int arenaWidth = 30;
    int arenaHeight = 34;
    std::uint32_t seed = 1337;
    MatchRules rules;
};

/// A short, human readable log line describing something that just happened
/// (a kill, a pickup, the countdown starting). Clients render these as the
/// kill feed and the ASCII edition prints them directly.
struct GameEvent {
    float time = 0.0f;
    std::string text;
};

/// Authoritative simulation shared by every native edition. It is completely
/// free of rendering, threading and networking code so the PC, mobile, pocket
/// and headless builds all run byte-identical game logic.
class World {
public:
    explicit World(const WorldConfig& config = {});

    EntityId addPlayer(const std::string& name, const std::string& brawlerId, Team team, bool bot);
    void removePlayer(EntityId id);
    void setInput(EntityId id, const PlayerInput& input);

    void step(float dt);

    /// Replaces the locally simulated entities with server-authoritative state.
    /// Used by the native editions when they are connected to a match server;
    /// the arena itself is generated from the shared seed on both sides.
    void applySnapshot(const Snapshot& snapshot);

    /// Replaces the locally generated arena with the one the server sent.
    void setArena(const Arena& arena) { arena_ = arena; }

    const Arena& arena() const { return arena_; }
    const MatchState& match() const { return match_; }
    const std::vector<Player>& players() const { return players_; }
    const std::vector<Projectile>& projectiles() const { return projectiles_; }
    const std::vector<Gem>& gems() const { return gems_; }
    const std::deque<GameEvent>& events() const { return events_; }
    const WorldConfig& config() const { return config_; }

    const Player* findPlayer(EntityId id) const;
    Player* findPlayerMutable(EntityId id);

private:
    void stepPlayer(Player& player, float dt);
    void fire(Player& player);
    void fireSuper(Player& player);
    void stepProjectiles(float dt);
    void advanceProjectile(Projectile& proj, float dt);
    void stepGems(float dt);
    void stepMatch(float dt);
    void killPlayer(Player& victim, Player* killer);
    void spawnGem();
    void dropGems(const Player& victim);
    void respawn(Player& player);
    void pushEvent(const std::string& text);
    int teamIndex(Team t) const { return t == Team::Blue ? 0 : 1; }
    int teamPlayerCount(Team t) const;

    WorldConfig config_;
    Arena arena_;
    MatchState match_;
    std::vector<Player> players_;
    std::vector<Projectile> projectiles_;
    std::vector<Gem> gems_;
    std::unordered_map<EntityId, PlayerInput> inputs_;
    std::deque<GameEvent> events_;
    std::mt19937 rng_;
    EntityId nextId_ = 1;
    float gemSpawnTimer_ = 0.0f;
};

}  // namespace mega
