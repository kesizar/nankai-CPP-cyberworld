#pragma once

#include <QMainWindow>

#include "LLMClient.h"
#include "Player.h"
#include "WorldState.h"

class QLabel;
class QLineEdit;
class QListWidget;
class QProgressBar;
class QPushButton;
class QTextBrowser;
class QTimer;

/**
 * @brief 主界面：赛博朋克布局、游戏状态机入口、LLM 回调缝合与人性视觉反馈。
 *
 * 左侧：剧情 QTextBrowser + 单行输入与执行按钮。
 * 右侧：HP / Humanity 进度条、背包与义体列表。
 */
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override = default;

private slots:
    void onExecuteClicked();
    void onLlmCompleted(const QString& message_content);
    void onLlmFailed(const QString& error_message);
    /** 低人性（<=50）时由定时器偶尔触发，用于轻微 Glitch / 背景压迫感。 */
    void onHumanityGlitchPulse();
    void onEndgameFlashTick();

private:
    void buildUi();
    void applyCyberpunkQss();
    void loadConfigOrWarn();
    void bootstrapGameState();
    void refreshPlayerPanel();
    void setInputBusy(bool busy, const QString& placeholder = QString());
    /** 解析 DeepSeek 返回的 JSON 文本，更新 Player / WorldState。成功返回 true。 */
    bool applyLlmGameJson(const QString& json_text);
    /** PRD：赛博精神病终局 —— 清空剧情区、血红色大字、禁用输入、删档。 */
    void enterCyberpsychosisEnding();
    /** HP<=0 且模型声明传奇：最高荣誉结局（不写入存档，删除 savegame）。 */
    void enterLegendEnding();
    /** HP<=0 且非传奇：无名小卒结局（不写入存档，删除 savegame）。 */
    void enterNobodyEnding();
    void stopAmbienceTimers();
    std::string saveGamePathStd() const;
    QString saveGamePath() const;

    QWidget* central_{nullptr};
    QTextBrowser* narrative_{nullptr};
    QLineEdit* action_input_{nullptr};
    QPushButton* execute_btn_{nullptr};

    QLabel* hp_label_{nullptr};
    QProgressBar* hp_bar_{nullptr};
    QLabel* humanity_label_{nullptr};
    QProgressBar* humanity_bar_{nullptr};
    QListWidget* inventory_list_{nullptr};
    QListWidget* cyberware_list_{nullptr};
    QWidget* right_panel_{nullptr};

    Player player_{};
    WorldState world_{};
    LLMClient llm_client_{this};

    QTimer* humanity_glitch_timer_{nullptr};
    QTimer* endgame_flash_timer_{nullptr};
    int endgame_flash_remaining_{0};
    bool game_locked_{false};
    bool request_inflight_{false};
    /** 仅上一轮流式响应解析出的 is_legend；is_dead 由 HP<=0 在 C++ 侧推导。 */
    bool last_llm_is_legend_{false};
};
