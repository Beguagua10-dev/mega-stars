#include <cstdio>
#include <string>

#include "mega/BotBrain.h"
#include "mega/World.h"
#include "mega/net/Json.h"

namespace {

int gFailures = 0;

void check(bool condition, const char* what) {
    if (!condition) {
        ++gFailures;
        std::printf("FAIL %s\n", what);
    } else {
        std::printf("ok   %s\n", what);
    }
}

void testArenaIsSymmetric() {
    mega::Arena arena(30, 34, 7);
    bool symmetric = true;
    for (int y = 0; y < arena.height(); ++y) {
        for (int x = 0; x < arena.width(); ++x) {
            if (arena.at(x, y) != arena.at(arena.width() - 1 - x, arena.height() - 1 - y)) {
                symmetric = false;
            }
        }
    }
    check(symmetric, "arena is point symmetric");
}

void testWallsBlockMovement() {
    mega::Arena arena(30, 34, 3);
    const mega::Vec2 start{1.5f, 1.5f};
    const mega::Vec2 moved = arena.resolveMove(start, {-5.0f, -5.0f}, 0.42f);
    check(moved.x >= 1.0f && moved.y >= 1.0f, "players cannot walk through the border wall");
}

void testShootingDamagesEnemies() {
    mega::WorldConfig config;
    config.seed = 11;
    mega::World world(config);
    const mega::EntityId attacker = world.addPlayer("A", "mira", mega::Team::Blue, false);
    const mega::EntityId victim = world.addPlayer("B", "faisca", mega::Team::Red, false);

    // Skip the warm-up phase.
    for (int i = 0; i < 200; ++i) {
        world.step(1.0f / 30.0f);
    }

    mega::Player* a = world.findPlayerMutable(attacker);
    mega::Player* b = world.findPlayerMutable(victim);
    // The tiles around the gem spawn are always clear, so nothing blocks the shot.
    const mega::Vec2 center = world.arena().center();
    a->position = {center.x - 1.0f, center.y + 0.5f};
    b->position = {center.x + 1.0f, center.y + 0.5f};
    const int before = b->health;

    mega::PlayerInput input;
    input.aim = {1.0f, 0.0f};
    input.shoot = true;
    world.setInput(attacker, input);
    world.step(1.0f / 30.0f);
    world.setInput(attacker, mega::PlayerInput{});
    for (int i = 0; i < 15; ++i) {
        world.step(1.0f / 30.0f);
    }

    check(world.findPlayer(victim)->health < before, "a hit reduces the target's health");
    check(world.findPlayer(attacker)->superCharge > 0, "hits charge the super");
}

void testGemsScoreForTheTeam() {
    mega::World world;
    const mega::EntityId id = world.addPlayer("A", "faisca", mega::Team::Blue, false);
    for (int i = 0; i < 300; ++i) {
        world.findPlayerMutable(id)->position = world.arena().center();
        world.step(1.0f / 30.0f);
        if (world.match().teamGems[0] > 0) {
            break;
        }
    }
    check(world.match().teamGems[0] > 0, "standing on a gem scores for the blue team");
}

void testBotsMove() {
    mega::World world;
    world.addPlayer("Bot", "bruto", mega::Team::Red, true);
    const mega::Vec2 start = world.players().front().position;
    for (int i = 0; i < 120; ++i) {
        mega::driveBots(world);
        world.step(1.0f / 30.0f);
    }
    check(mega::distance(world.players().front().position, start) > 0.5f, "bots walk around");
}

void testJsonParsing() {
    const mega::net::Json root = mega::net::Json::parse(
        R"({"t":"state","match":{"ph":1,"g0":3},"players":[{"id":7,"n":"Ana \u00e9"}]})");
    check(root["t"].asString() == "state", "json reads strings");
    check(root["match"]["g0"].asInt() == 3, "json reads nested numbers");
    check(root["players"].size() == 1, "json reads arrays");
    check(root["players"].items()[0]["id"].asInt() == 7, "json reads array objects");
    check(mega::net::Json::escape("a\"b") == "a\\\"b", "json escapes quotes");
}

}  // namespace

int main() {
    testArenaIsSymmetric();
    testWallsBlockMovement();
    testShootingDamagesEnemies();
    testGemsScoreForTheTeam();
    testBotsMove();
    testJsonParsing();

    if (gFailures > 0) {
        std::printf("\n%d teste(s) falharam\n", gFailures);
        return 1;
    }
    std::puts("\nTodos os testes passaram");
    return 0;
}
