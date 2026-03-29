#pragma once

#include <string>

class Player;
class WorldState;

/**
 * @brief 使用 nlohmann::json 将 Player 与 WorldState 与 savegame.json 互转。
 *
 * 默认路径为工程运行目录下的 savegame.json，可由调用方覆盖。
 */
class SaveManager {
public:
    static bool saveToFile(const Player& player,
                           const WorldState& world,
                           const std::string& path = "savegame.json");

    static bool loadFromFile(Player& player,
                             WorldState& world,
                             const std::string& path = "savegame.json");

    /** 删除本地存档文件（用于赛博精神病终局等流程）。文件不存在时返回 false。 */
    static bool deleteSaveFile(const std::string& path = "savegame.json");
};
