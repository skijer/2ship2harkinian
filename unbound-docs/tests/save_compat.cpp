#include "2s2h/BenJsonConversions.hpp"
#include <iostream>
#include <stdexcept>

static void Require(bool condition, const char* message) {
    if (!condition) { throw std::runtime_error(message); }
}
int main() {
    try {
        Inventory source = {};
        for (size_t i = 0; i < 10; i++) { source.dungeonKeys[i] = (s8)i; }
        source.defenseHearts = 7;
        json original = source;
        Inventory roundtrip = original.get<Inventory>();
        Require(roundtrip.dungeonKeys[9] == 9 && roundtrip.defenseHearts == 7, "new inventory save roundtrip failed");
        original["dungeonKeys"].erase(original["dungeonKeys"].end() - 1);
        Inventory legacy = original.get<Inventory>();
        for (size_t i = 0; i < 9; i++) { Require(legacy.dungeonKeys[i] == i, "legacy dungeon key changed"); }
        Require(legacy.dungeonKeys[9] == -1 && legacy.defenseHearts == 7, "legacy inventory field alignment failed");
        original["dungeonKeys"].push_back(42);
        original["dungeonKeys"].push_back(99);
        Inventory future = original.get<Inventory>();
        Require(future.dungeonKeys[9] == 42 && future.defenseHearts == 7, "extra inventory fields were not bounded");
        RandoSaveInfo randomizer = {};
        for (size_t i = 0; i < 10; i++) { randomizer.foundDungeonKeys[i] = (s8)i; }
        randomizer.finalSeed = 123456;
        json seed = randomizer;
        seed["foundDungeonKeys"].erase(seed["foundDungeonKeys"].end() - 1);
        const auto loaded = seed.get<RandoSaveInfo>();
        Require(loaded.finalSeed == 123456 && loaded.foundDungeonKeys[8] == 8 && loaded.foundDungeonKeys[9] == -1,
                "legacy randomizer save failed");
        std::cout << "Inventory roundtrip, nine-slot legacy saves, extra-slot bounds, and randomizer compatibility passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
