#include "WorldState.h"

WorldState::WorldState() = default;

const std::map<std::string, int>& WorldState::npcRelations() const {
    return npc_relations_;
}

std::map<std::string, int>& WorldState::npcRelations() { return npc_relations_; }

const std::string& WorldState::globalSummary() const { return global_summary_; }

void WorldState::setGlobalSummary(const std::string& summary) {
    global_summary_ = summary;
}

const std::deque<std::string>& WorldState::slidingWindow() const {
    return sliding_window_;
}

std::deque<std::string>& WorldState::slidingWindow() { return sliding_window_; }

void WorldState::pushSlidingWindow(std::string turn_snippet) {
    sliding_window_.push_back(std::move(turn_snippet));
    while (sliding_window_.size() > kSlidingWindowMax) {
        sliding_window_.pop_front();
    }
}

void WorldState::clearSlidingWindow() { sliding_window_.clear(); }
