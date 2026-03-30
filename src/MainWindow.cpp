#include "MainWindow.h"

#include "SaveManager.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>
#include <QRandomGenerator>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QSplitter>
#include <QTextBrowser>
#include <QTimer>
#include <QVBoxLayout>

#include <string>

#include <json.hpp>

namespace {

using nlohmann::json;

/** 与 LLMClient 一致：去掉 ```json 围栏，避免 parse 得到非对象或截断。 */
QString unwrapModelJson(QString raw) {
    raw = raw.trimmed();
    if (!raw.startsWith(QStringLiteral("```"))) {
        return raw;
    }
    const int first_nl = raw.indexOf(QLatin1Char('\n'));
    if (first_nl != -1) {
        raw = raw.mid(first_nl + 1);
    } else {
        raw = raw.mid(3);
    }
    raw = raw.trimmed();
    if (raw.endsWith(QStringLiteral("```"))) {
        const int fence = raw.lastIndexOf(QStringLiteral("```"));
        if (fence >= 0) {
            raw = raw.left(fence);
        }
    }
    return raw.trimmed();
}

std::string jString(const json& j, const char* key, const std::string& def = {}) {
    const auto it = j.find(key);
    if (it == j.end() || it->is_null()) {
        return def;
    }
    if (it->is_string()) {
        return it->get<std::string>();
    }
    return def;
}

int jInt(const json& j, const char* key, int def = 0) {
    const auto it = j.find(key);
    if (it == j.end() || it->is_null()) {
        return def;
    }
    if (it->is_number_integer()) {
        return static_cast<int>(it->get<int>());
    }
    if (it->is_number_unsigned()) {
        return static_cast<int>(it->get<unsigned>());
    }
    if (it->is_number_float()) {
        return static_cast<int>(it->get<double>());
    }
    if (it->is_string()) {
        try {
            return std::stoi(it->get<std::string>());
        } catch (...) {
            return def;
        }
    }
    return def;
}

bool jBool(const json& j, const char* key, bool def = false) {
    const auto it = j.find(key);
    if (it == j.end() || it->is_null()) {
        return def;
    }
    if (it->is_boolean()) {
        return it->get<bool>();
    }
    if (it->is_number_integer()) {
        return it->get<int>() != 0;
    }
    if (it->is_string()) {
        const auto s = it->get<std::string>();
        if (s == "true" || s == "1") {
            return true;
        }
        if (s == "false" || s == "0") {
            return false;
        }
    }
    return def;
}

/**
 * 兼容：对象 {name, change}、别名字段 npc/delta、或对象数组。
 * 全程使用 find + 类型判断，不使用 at("name")，避免模型漏键崩溃。
 */
void applyNpcRelationChange(WorldState& world, const json& root) {
    const auto it = root.find("npc_relation_change");
    if (it == root.end() || it->is_null()) {
        return;
    }

    const auto apply_one = [&world](const json& nr) -> void {
        if (!nr.is_object()) {
            return;
        }
        std::string name;
        if (const auto ni = nr.find("name"); ni != nr.end() && ni->is_string()) {
            name = ni->get<std::string>();
        } else if (const auto ni = nr.find("npc"); ni != nr.end() && ni->is_string()) {
            name = ni->get<std::string>();
        }
        if (name.empty()) {
            return;
        }
        int delta = 0;
        if (const auto ci = nr.find("change"); ci != nr.end()) {
            if (ci->is_number_integer()) {
                delta = static_cast<int>(ci->get<int>());
            } else if (ci->is_number_unsigned()) {
                delta = static_cast<int>(ci->get<unsigned>());
            } else if (ci->is_number_float()) {
                delta = static_cast<int>(ci->get<double>());
            } else if (ci->is_string()) {
                try {
                    delta = std::stoi(ci->get<std::string>());
                } catch (...) {
                }
            }
        } else if (const auto di = nr.find("delta"); di != nr.end()) {
            if (di->is_number_integer()) {
                delta = static_cast<int>(di->get<int>());
            }
        }
        world.npcRelations()[name] += delta;
    };

    const json& v = *it;
    if (v.is_object()) {
        apply_one(v);
    } else if (v.is_array()) {
        for (const auto& el : v) {
            apply_one(el);
        }
    }
}

QString cyberpsychosisHtml() {
    return QStringLiteral(
        "<div style=\"color:#ff1a1a;font-size:26px;font-weight:900;text-align:center;"
        "font-family:'Consolas','Microsoft YaHei',monospace;padding:24px;\">"
        "[警告：神经阻断剂失效。你已彻底丧失人性，沦为赛博精神病。暴恐机动队 (MaxTac) "
        "已在三分钟后将你击毙。游戏结束。]"
        "</div>");
}

QString randomGlitchFragment() {
    static const QString kPool =
        QStringLiteral("█▓▒░0x7F错λΩ÷∆");
    const qint64 seed = QDateTime::currentMSecsSinceEpoch();
    QString out;
    const int len = 4 + int(seed % 6);
    for (int i = 0; i < len; ++i) {
        out.append(kPool.at(int(seed + i * 31) % kPool.size()));
    }
    return out;
}

int clampBarMax(int value) {
    return qMax(100, qMax(0, value));
}

}  // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setObjectName(QStringLiteral("MainCyberWindow"));
    buildUi();
    applyCyberpunkQss();

    humanity_glitch_timer_ = new QTimer(this);
    humanity_glitch_timer_->setSingleShot(true);
    connect(humanity_glitch_timer_, &QTimer::timeout, this,
            &MainWindow::onHumanityGlitchPulse);

    endgame_flash_timer_ = new QTimer(this);
    connect(endgame_flash_timer_, &QTimer::timeout, this,
            &MainWindow::onEndgameFlashTick);

    connect(&llm_client_, &LLMClient::completionReceived, this,
            &MainWindow::onLlmCompleted);
    connect(&llm_client_, &LLMClient::requestFailed, this,
            &MainWindow::onLlmFailed);

    loadConfigOrWarn();
    bootstrapGameState();
    refreshPlayerPanel();

    if (!game_locked_) {
        onHumanityGlitchPulse();
    }
}

std::string MainWindow::saveGamePathStd() const {
    return saveGamePath().toStdString();
}

QString MainWindow::saveGamePath() const {
    return QCoreApplication::applicationDirPath() +
           QStringLiteral("/savegame.json");
}

void MainWindow::buildUi() {
    central_ = new QWidget(this);
    setCentralWidget(central_);
    auto* root = new QHBoxLayout(central_);
    root->setContentsMargins(8, 8, 8, 8);

    auto* splitter = new QSplitter(Qt::Horizontal, central_);
    root->addWidget(splitter, 1);

    auto* left = new QWidget(splitter);
    auto* left_lay = new QVBoxLayout(left);
    left_lay->setContentsMargins(4, 4, 4, 4);
    narrative_jitter_wrap_ = new QWidget(left);
    auto* nar_wrap_lay = new QVBoxLayout(narrative_jitter_wrap_);
    nar_wrap_lay->setContentsMargins(4, 4, 4, 4);
    narrative_ = new QTextBrowser(narrative_jitter_wrap_);
    narrative_->setOpenExternalLinks(false);
    narrative_->setReadOnly(true);
    nar_wrap_lay->addWidget(narrative_, 1);
    left_lay->addWidget(narrative_jitter_wrap_, 1);

    auto* row = new QHBoxLayout();
    action_input_ = new QLineEdit(left);
    action_input_->setPlaceholderText(QStringLiteral("输入你的行动……"));
    execute_btn_ = new QPushButton(QStringLiteral("执行"), left);
    row->addWidget(action_input_, 1);
    row->addWidget(execute_btn_);
    left_lay->addLayout(row);
    splitter->addWidget(left);

    right_panel_ = new QWidget(splitter);
    auto* right_lay = new QVBoxLayout(right_panel_);
    right_lay->setContentsMargins(8, 8, 8, 8);
    auto* status_title = new QLabel(QStringLiteral("/// 状态面板"), right_panel_);
    status_title->setObjectName(QStringLiteral("PanelTitle"));
    right_lay->addWidget(status_title);

    hp_label_ = new QLabel(QStringLiteral("HP"), right_panel_);
    hp_bar_ = new QProgressBar(right_panel_);
    hp_bar_->setRange(0, 100);
    hp_bar_->setTextVisible(true);
    right_lay->addWidget(hp_label_);
    right_lay->addWidget(hp_bar_);

    humanity_label_ = new QLabel(QStringLiteral("Humanity"), right_panel_);
    humanity_bar_ = new QProgressBar(right_panel_);
    humanity_bar_->setRange(0, 100);
    humanity_bar_->setTextVisible(true);
    right_lay->addWidget(humanity_label_);
    right_lay->addWidget(humanity_bar_);

    auto* inv_lbl =
        new QLabel(QStringLiteral("背包 inventory"), right_panel_);
    inv_lbl->setObjectName(QStringLiteral("SubTitle"));
    inventory_list_ = new QListWidget(right_panel_);
    auto* cyber_lbl =
        new QLabel(QStringLiteral("义体 cyberware"), right_panel_);
    cyber_lbl->setObjectName(QStringLiteral("SubTitle"));
    cyberware_list_ = new QListWidget(right_panel_);
    right_lay->addWidget(inv_lbl);
    right_lay->addWidget(inventory_list_, 1);
    right_lay->addWidget(cyber_lbl);
    right_lay->addWidget(cyberware_list_, 1);
    splitter->addWidget(right_panel_);

    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);

    connect(execute_btn_, &QPushButton::clicked, this,
            &MainWindow::onExecuteClicked);
}

void MainWindow::applyCyberpunkQss() {
    const QString qss = QStringLiteral(
        "QMainWindow, QWidget { background-color: #141418; color: #5dffc8; "
        "font-family: 'Microsoft YaHei','Segoe UI',sans-serif; font-size: 14px; }"
        "QTextBrowser {"
        "  background-color: #0d0d10;"
        "  color: #5dffc8;"
        "  border: 1px solid #1f3a3a;"
        "  border-radius: 4px;"
        "  selection-background-color: #255050;"
        "}"
        "QLineEdit {"
        "  background-color: #0d0d10;"
        "  color: #5dffc8;"
        "  border: 1px solid #2b4a4a;"
        "  border-radius: 4px;"
        "  padding: 6px;"
        "}"
        "QPushButton {"
        "  background-color: #1b2a2a;"
        "  color: #ffe066;"
        "  border: 1px solid #ffe066;"
        "  border-radius: 4px;"
        "  padding: 8px 18px;"
        "  font-weight: bold;"
        "}"
        "QPushButton:hover { background-color: #234040; }"
        "QPushButton:pressed { background-color: #0f2020; }"
        "QPushButton:disabled { color: #666; border-color: #444; }"
        "QLabel#PanelTitle { color: #ffe066; font-weight: bold; font-size: 15px; }"
        "QLabel#SubTitle { color: #7df9ff; font-size: 13px; }"
        "QProgressBar {"
        "  border: 1px solid #2a3a3a;"
        "  border-radius: 4px;"
        "  text-align: center;"
        "  background-color: #101018;"
        "  color: #e8fff7;"
        "}"
        "QListWidget {"
        "  background-color: #0d0d10;"
        "  color: #5dffc8;"
        "  border: 1px solid #1f3a3a;"
        "  border-radius: 4px;"
        "}");
    qApp->setStyleSheet(qss);
}

void MainWindow::loadConfigOrWarn() {
    const QString cfg =
        QCoreApplication::applicationDirPath() + QStringLiteral("/config.json");
    if (!llm_client_.loadApiKeyFromConfig(cfg)) {
        QMessageBox::warning(
            this, QStringLiteral("配置缺失"),
            QStringLiteral(
                "未能从 %1 读取 API Key。\n请将 api_key 写入 config.json 后重启。\n"
                "（仍可先浏览界面，但无法发起演算请求）")
                .arg(cfg));
    }
}

void MainWindow::bootstrapGameState() {
    if (SaveManager::loadFromFile(player_, world_, saveGamePathStd())) {
        narrative_->append(
            QStringLiteral("<p style=\"color:#ffe066\">【系统】存档已载入。</p>"));
    } else {
        narrative_->append(QStringLiteral(
            "<p style=\"color:#7df9ff\">夜之城的雨洗不掉街头的污垢，只能让霓虹灯的倒影显得更加泥泞。</p>"
            "<p style=\"color:#5dffc8\">昨天，学院的西装暴徒把你像件不合格的零件一样扫地出门。他们说你缺乏“公司基因”，其实你只是卡里没钱。"
            "你的母亲上周死在发臭的诊所担架上，创伤小组看了一眼她掉线的保险卡，连浮空车的引擎都没熄就飞走了。</p>"
            "<p style=\"color:#5dffc8\">现在，这间逼仄的公寓里只剩下合成面条的馊味、一沓催款单，还有桌上那块用旧油布裹着的军用级义体。"
            "它很沉，带着臭氧和死亡的冷硬气息。这是一张通往地狱的单程票，而你正准备将它接入脊椎。</p>"
            "<p style=\"color:#5dffc8\">在这座吃人的城市里，无名小卒和街头传奇之间唯一的区别，就是你燃烧殆尽时引发的爆炸有多响。</p>"
            "<p style=\"color:#ffe066;font-weight:bold\">是时候点燃这根火柴了。</p>"
            "<p style=\"color:#5dffc8\">在下方输入行动，点击<strong>执行</strong>开始。</p>"));
    }
    if (player_.humanity() <= 0) {
        enterCyberpsychosisEnding();
    }
}

void MainWindow::applyLowHumanityChrome() {
    if (game_locked_) {
        return;
    }
    const int h = player_.humanity();
    if (h <= 30 && h > 0) {
        narrative_->setStyleSheet(QStringLiteral(
            "QTextBrowser {"
            "  background-color: #2a0c0e;"
            "  color: #7de8d4;"
            "  border: 2px solid #8b2024;"
            "  border-radius: 4px;"
            "  selection-background-color: #4a1518;"
            "}"));
        setStyleSheet(QStringLiteral(
            "QMainWindow#MainCyberWindow {"
            "  border: 3px solid #c62828;"
            "  border-radius: 8px;"
            "}"));
    } else {
        narrative_->setStyleSheet(QString());
        setStyleSheet(QString());
    }
}

void MainWindow::refreshPlayerPanel() {
    const int hp_max = clampBarMax(player_.hp());
    hp_bar_->setRange(0, hp_max);
    hp_bar_->setValue(qBound(0, player_.hp(), hp_max));
    hp_bar_->setFormat(QStringLiteral("%v / %m"));

    const int hum_max = clampBarMax(player_.humanity());
    humanity_bar_->setRange(0, hum_max);
    humanity_bar_->setValue(qBound(0, player_.humanity(), hum_max));
    humanity_bar_->setFormat(QStringLiteral("%v / %m"));

    /**
     * Humanity 条：分段警示色（>50 青绿、31–50 琥珀、1–30 赤红、<=0 深红底）。
     * 与 PRD 第五部分「<=30 暗红警示」呼应，用 chunk 颜色动态切换。
     */
    if (player_.humanity() > 50) {
        humanity_bar_->setStyleSheet(QStringLiteral(
            "QProgressBar::chunk { background-color: qlineargradient("
            "x1:0,y1:0,x2:1,y2:0, stop:0 #00ffd0, stop:1 #2d7fff); }"));
    } else if (player_.humanity() > 30) {
        humanity_bar_->setStyleSheet(QStringLiteral(
            "QProgressBar::chunk { background-color: qlineargradient("
            "x1:0,y1:0,x2:1,y2:0, stop:0 #ffcc33, stop:1 #ff6b2d); }"));
    } else if (player_.humanity() > 0) {
        humanity_bar_->setStyleSheet(QStringLiteral(
            "QProgressBar::chunk { background-color: #b31217; }"));
    } else {
        humanity_bar_->setStyleSheet(QStringLiteral(
            "QProgressBar::chunk { background-color: #4a0000; }"));
    }

    inventory_list_->clear();
    for (const auto& s : player_.inventory()) {
        inventory_list_->addItem(QString::fromStdString(s));
    }
    cyberware_list_->clear();
    for (const auto& s : player_.cyberware()) {
        cyberware_list_->addItem(QString::fromStdString(s));
    }

    /**
     * 低人性视觉联动（<=50）：右侧面板偶尔透出暗红压迫；更细节的闪烁在 onHumanityGlitchPulse。
     */
    if (player_.humanity() <= 30) {
        right_panel_->setStyleSheet(QStringLiteral(
            "QWidget { background-color: rgba(80, 12, 12, 55); border-radius: 6px; }"));
    } else if (player_.humanity() <= 50) {
        right_panel_->setStyleSheet(QStringLiteral(
            "QWidget { background-color: rgba(55, 20, 20, 35); border-radius: 6px; }"));
    } else {
        right_panel_->setStyleSheet(QString());
    }

    applyLowHumanityChrome();
}

void MainWindow::setInputBusy(bool busy, const QString& placeholder) {
    request_inflight_ = busy;
    if (game_locked_) {
        action_input_->setEnabled(false);
        execute_btn_->setEnabled(false);
        return;
    }
    action_input_->setEnabled(!busy);
    execute_btn_->setEnabled(!busy);
    if (busy) {
        action_input_->setPlaceholderText(placeholder.isEmpty()
            ? QStringLiteral("系统演算中……")
            : placeholder);
    } else {
        action_input_->setPlaceholderText(
            QStringLiteral("输入你的行动……"));
    }
}

void MainWindow::onExecuteClicked() {
    if (game_locked_ || request_inflight_) {
        return;
    }
    if (LLMClient::shouldBlockForCyberpsychosis(player_)) {
        enterCyberpsychosisEnding();
        return;
    }
    const QString text = action_input_->text().trimmed();
    if (text.isEmpty()) {
        return;
    }
    if (llm_client_.apiKey().isEmpty()) {
        narrative_->append(QStringLiteral(
            "<p style=\"color:#ffe066\">【提示】请先配置 config.json 中的 api_key。</p>"));
        return;
    }

    player_.incrementActionCount(1);
    action_input_->clear();
    setInputBusy(true);

    llm_client_.requestChatCompletion(player_, world_, text);
}

void MainWindow::onLlmCompleted(const QString& message_content) {
    setInputBusy(false);

    if (game_locked_) {
        return;
    }

    if (!applyLlmGameJson(message_content)) {
        narrative_->append(QStringLiteral(
            "<p style=\"color:#ffe066\">【协议错误】模型返回的 JSON 无法解析，请重试。</p>"));
        SaveManager::saveToFile(player_, world_, saveGamePathStd());
        return;
    }

    refreshPlayerPanel();

    if (player_.humanity() <= 0) {
        enterCyberpsychosisEnding();
        return;
    }

    /** is_dead ≡ (HP<=0)，仅此条件；与 is_legend 组合分流终局。 */
    const bool is_dead = (player_.hp() <= 0);
    if (is_dead) {
        if (last_llm_is_legend_) {
            enterLegendEnding();
        } else {
            enterNobodyEnding();
        }
        return;
    }

    SaveManager::saveToFile(player_, world_, saveGamePathStd());
}

void MainWindow::onLlmFailed(const QString& error_message) {
    setInputBusy(false);
    if (!game_locked_) {
        narrative_->append(QStringLiteral("<p style=\"color:#ffe066\">【演算失败】%1</p>")
                               .arg(error_message.toHtmlEscaped()));
    }
}

bool MainWindow::applyLlmGameJson(const QString& json_text) {
    last_llm_is_legend_ = false;
    try {
        const json root = json::parse(unwrapModelJson(json_text).toStdString());
        if (!root.is_object()) {
            narrative_->append(QStringLiteral(
                "<p style=\"color:#ffe066\">【解析异常】模型内容不是 JSON 对象。</p>"));
            return false;
        }

        const std::string narrative_raw = jString(root, "narrative");
        if (narrative_raw.empty()) {
            narrative_->append(QStringLiteral(
                "<p style=\"color:#ffe066\">【解析异常】缺少 narrative 或为空。</p>"));
            return false;
        }

        const int hp_delta = jInt(root, "hp_change", 0);
        const int hum_delta = jInt(root, "humanity_change", 0);
        const std::string new_item = jString(root, "new_item");
        const std::string new_cw = jString(root, "new_cyberware");
        const std::string summary = jString(root, "current_situation_summary");
        /** is_dead 仅由回合末 HP<=0 判定，不再读取模型 JSON。 */
        last_llm_is_legend_ = jBool(root, "is_legend", false);

        const QString narrative = QString::fromStdString(narrative_raw);
        narrative_->append(
            QStringLiteral("<p style=\"color:#5dffc8\">%1</p>")
                .arg(narrative.toHtmlEscaped().replace(QLatin1Char('\n'), QStringLiteral("<br/>"))));

        player_.setHp(player_.hp() + hp_delta);
        player_.setHumanity(player_.humanity() + hum_delta);
        if (!new_item.empty()) {
            player_.addInventoryItem(new_item);
        }
        if (!new_cw.empty()) {
            player_.addCyberware(new_cw);
        }
        applyNpcRelationChange(world_, root);
        world_.setGlobalSummary(summary);
        world_.pushSlidingWindow(narrative_raw);

        return true;
    } catch (const std::exception& ex) {
        narrative_->append(QStringLiteral("<p style=\"color:#ffe066\">【解析异常】%1</p>")
                               .arg(QString::fromUtf8(ex.what()).toHtmlEscaped()));
        return false;
    }
}

void MainWindow::enterLegendEnding() {
    game_locked_ = true;
    stopAmbienceTimers();
    narrative_->append(QStringLiteral(
        "<p style=\"color:#ffe066;font-weight:bold\">【传奇结局】我们在来生酒吧见，传奇！</p>"
        "<p style=\"color:#7df9ff\">在最绚烂的火焰中，名字被刻进夜之城。</p>"));
    action_input_->setEnabled(false);
    execute_btn_->setEnabled(false);
    action_input_->clear();
    action_input_->setPlaceholderText(QStringLiteral("传奇已封笔"));
    refreshPlayerPanel();
    SaveManager::deleteSaveFile(saveGamePathStd());
}

void MainWindow::enterNobodyEnding() {
    game_locked_ = true;
    stopAmbienceTimers();
    narrative_->append(QStringLiteral(
        "<p style=\"color:#ff6666\">【结局】HP 归零 —— 无名小卒，夜之城不记得你。</p>"));
    action_input_->setEnabled(false);
    execute_btn_->setEnabled(false);
    action_input_->clear();
    action_input_->setPlaceholderText(QStringLiteral("电路已静默"));
    refreshPlayerPanel();
    SaveManager::deleteSaveFile(saveGamePathStd());
}

void MainWindow::enterCyberpsychosisEnding() {
    game_locked_ = true;
    stopAmbienceTimers();

    SaveManager::deleteSaveFile(saveGamePathStd());

    narrative_->clear();
    narrative_->setHtml(cyberpsychosisHtml());

    /**
     * 终局强反馈：短促红色闪烁若干次（不阻塞事件循环）。
     */
    endgame_flash_remaining_ = 12;
    endgame_flash_timer_->start(90);

    action_input_->setEnabled(false);
    execute_btn_->setEnabled(false);
    action_input_->clear();
    action_input_->setPlaceholderText(QStringLiteral("神经链路已熔断"));

    refreshPlayerPanel();
}

void MainWindow::stopAmbienceTimers() {
    if (humanity_glitch_timer_) {
        humanity_glitch_timer_->stop();
    }
    if (endgame_flash_timer_) {
        endgame_flash_timer_->stop();
    }
}

void MainWindow::onHumanityGlitchPulse() {
    if (game_locked_) {
        return;
    }
    /**
     * PRD：<=50 偶尔 Glitch —— 噪点 + 叙事区外包一层 contentsMargins 轻微错位（不改动 ≤30 持续暗红底）。
     */
    if (player_.humanity() <= 50 && player_.humanity() > 0) {
        narrative_->append(QStringLiteral("<span style=\"color:#8899aa;font-size:12px\">")
                           + randomGlitchFragment().toHtmlEscaped() + QStringLiteral("</span>"));

        if (narrative_jitter_wrap_) {
            if (QLayout* lay = narrative_jitter_wrap_->layout()) {
                const QMargins cm = lay->contentsMargins();
                int base_m = cm.left() > 0 ? cm.left() : 4;
                base_m = qBound(2, base_m, 8);
                const int dx = QRandomGenerator::global()->bounded(5) - 2;
                const int dy = QRandomGenerator::global()->bounded(5) - 2;
                lay->setContentsMargins(qMax(0, base_m + dx), qMax(0, base_m + dy),
                                        qMax(0, base_m - dx), qMax(0, base_m - dy));
                QTimer::singleShot(110, this, [this, base_m]() {
                    if (game_locked_ || !narrative_jitter_wrap_) {
                        return;
                    }
                    if (QLayout* l2 = narrative_jitter_wrap_->layout()) {
                        l2->setContentsMargins(base_m, base_m, base_m, base_m);
                    }
                    applyLowHumanityChrome();
                });
            }
        }
    }

    const int base = 4000 + (QRandomGenerator::global()->bounded(5000));
    humanity_glitch_timer_->start(base);
}

void MainWindow::onEndgameFlashTick() {
    if (endgame_flash_remaining_ <= 0) {
        endgame_flash_timer_->stop();
        if (narrative_) {
            narrative_->setStyleSheet(QString());
        }
        applyLowHumanityChrome();
        return;
    }
    --endgame_flash_remaining_;
    const bool on = (endgame_flash_remaining_ % 2) == 0;
    narrative_->setStyleSheet(on ? QStringLiteral(
        "QTextBrowser { background-color: #3d0505; color: #ff1a1a; "
        "border: 2px solid #ff3333; }")
                                 : QStringLiteral(
        "QTextBrowser { background-color: #140303; color: #ff1a1a; "
        "border: 2px solid #880000; }"));
}
