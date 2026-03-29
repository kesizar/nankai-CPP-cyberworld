#include "SaveManager.h"

#include "Player.h"
#include "WorldState.h"

#include <cstdio>

#include <fstream>
#include <json.hpp>

namespace {

using json = nlohmann::json;

json playerToJson(const Player& p) {
    json j;
    j["hp"] = p.hp();
    j["humanity"] = p.humanity();
    j["action_count"] = p.actionCount();
    j["inventory"] = p.inventory();
    j["cyberware"] = p.cyberware();
    return j;
}

void playerFromJson(const json& j, Player& p) {
    p.setHp(j.at("hp").get<int>());
    p.setHumanity(j.at("humanity").get<int>());
    p.setActionCount(j.at("action_count").get<int>());
    p.inventory() = j.at("inventory").get<std::vector<std::string>>();
    p.cyberware() = j.at("cyberware").get<std::vector<std::string>>();
}

json worldToJson(const WorldState& w) {
    json j;
    j["npc_relations"] = json::object();
    for (const auto& kv : w.npcRelations()) {
        j["npc_relations"][kv.first] = kv.second;
    }
    j["global_summary"] = w.globalSummary();
    j["sliding_window"] = json::array();
    for (const auto& line : w.slidingWindow()) {
        j["sliding_window"].push_back(line);
    }
    return j;
}

void worldFromJson(const json& j, WorldState& w) {
    w.npcRelations().clear();
    const auto& rel = j.at("npc_relations");
    for (auto it = rel.begin(); it != rel.end(); ++it) {
        w.npcRelations()[it.key()] = it.value().get<int>();
    }
    w.setGlobalSummary(j.at("global_summary").get<std::string>());
    w.slidingWindow().clear();
    for (const auto& item : j.at("sliding_window")) {
        w.slidingWindow().push_back(item.get<std::string>());
    }
}

}  // namespace

bool SaveManager::saveToFile(const Player& player,
                             const WorldState& world,
                             const std::string& path) {
    try {
        json root;
        root["player"] = playerToJson(player);
        root["world"] = worldToJson(world);
        std::ofstream out(path, std::ios::out | std::ios::trunc);
        if (!out) {
            return false;
        }
        out << root.dump(2);
        return static_cast<bool>(out);
    } catch (...) {
        return false;
    }
}

bool SaveManager::loadFromFile(Player& player,
                               WorldState& world,
                               const std::string& path) {
    try {
        std::ifstream in(path, std::ios::in);
        if (!in) {
            return false;
        }
        json root;
        in >> root;
        playerFromJson(root.at("player"), player);
        worldFromJson(root.at("world"), world);
        return true;
    } catch (...) {
        return false;
    }
}

bool SaveManager::deleteSaveFile(const std::string& path) {
    return std::remove(path.c_str()) == 0;
}
