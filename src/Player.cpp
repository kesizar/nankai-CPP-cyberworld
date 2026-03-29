#include "Player.h"

Player::Player()
    : hp_(100),
      humanity_(100),
      action_count_(0) {}

int Player::hp() const { return hp_; }

void Player::setHp(int value) { hp_ = value; }

int Player::humanity() const { return humanity_; }

void Player::setHumanity(int value) { humanity_ = value; }

int Player::actionCount() const { return action_count_; }

void Player::setActionCount(int value) { action_count_ = value; }

void Player::incrementActionCount(int delta) { action_count_ += delta; }

const std::vector<std::string>& Player::inventory() const { return inventory_; }

std::vector<std::string>& Player::inventory() { return inventory_; }

const std::vector<std::string>& Player::cyberware() const { return cyberware_; }

std::vector<std::string>& Player::cyberware() { return cyberware_; }

void Player::addInventoryItem(const std::string& item) {
    inventory_.push_back(item);
}

void Player::addCyberware(const std::string& name) {
    cyberware_.push_back(name);
}
