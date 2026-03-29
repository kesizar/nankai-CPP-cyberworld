#pragma once

#include <string>
#include <vector>

/**
 * @brief 玩家实体：生命值、人性、累计操作次数、背包与已安装义体。
 *
 * 与 PRD 对齐：humanity 初始 100；action_count 用于压力机制（每 30 步可触发强制危机）。
 */
class Player {
public:
    Player();

    int hp() const;
    void setHp(int value);

    int humanity() const;
    void setHumanity(int value);

    int actionCount() const;
    void setActionCount(int value);
    void incrementActionCount(int delta = 1);

    const std::vector<std::string>& inventory() const;
    std::vector<std::string>& inventory();

    const std::vector<std::string>& cyberware() const;
    std::vector<std::string>& cyberware();

    void addInventoryItem(const std::string& item);
    void addCyberware(const std::string& name);

private:
    int hp_;
    int humanity_;
    int action_count_;
    std::vector<std::string> inventory_;
    std::vector<std::string> cyberware_;
};
