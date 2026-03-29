#pragma once

#include <deque>
#include <map>
#include <string>

/**
 * @brief 夜之城世界状态：NPC 好感、全局摘要、最近 5 回合滑动窗口。
 *
 * sliding_window 满 5 条时弹出最旧记录（FIFO）。
 */
class WorldState {
public:
    static constexpr std::size_t kSlidingWindowMax = 5;

    WorldState();

    const std::map<std::string, int>& npcRelations() const;
    std::map<std::string, int>& npcRelations();

    const std::string& globalSummary() const;
    void setGlobalSummary(const std::string& summary);

    const std::deque<std::string>& slidingWindow() const;
    std::deque<std::string>& slidingWindow();

    /** 追加一条回合摘要/对话要点，并维护窗口长度为 kSlidingWindowMax。 */
    void pushSlidingWindow(std::string turn_snippet);

    void clearSlidingWindow();

private:
    std::map<std::string, int> npc_relations_;
    std::string global_summary_;
    std::deque<std::string> sliding_window_;
};
