#include "LLMClient.h"

#include "Player.h"
#include "WorldState.h"

#include <QByteArray>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

namespace {

/** 模型常在 content 外包一层 ```json … ```，会导致解析失败或结构异常。 */
QString stripMarkdownJsonFence(QString content) {
    content = content.trimmed();
    if (!content.startsWith(QStringLiteral("```"))) {
        return content;
    }
    const int first_nl = content.indexOf(QLatin1Char('\n'));
    if (first_nl != -1) {
        content = content.mid(first_nl + 1);
    } else {
        content = content.mid(3);
    }
    content = content.trimmed();
    if (content.endsWith(QStringLiteral("```"))) {
        const int fence = content.lastIndexOf(QStringLiteral("```"));
        if (fence >= 0) {
            content = content.left(fence);
        }
    }
    return content.trimmed();
}

constexpr auto kDeepSeekEndpoint =
    "https://api.deepseek.com/chat/completions";
constexpr auto kModelName = "deepseek-chat";
/** 强制危机：每若干次有效操作在 user 末尾追加一次（第 0 次不计）。 */
constexpr int kForcedCrisisInterval = 15;

/** user 消息末尾强注入文案（第15、30、45… 次行动触发，间隔见 kForcedCrisisInterval）。 */
QString forcedCrisisDirective() {
    return QStringLiteral(
        "\n\n[系统强制事件]：立刻无视玩家当前的和平行动，触发一次符合当前剧情上下文的"
        "致命危机（例如：荒坂/军用科技特工突袭、暴走赛博精神病破墙而入、创伤小组的"
        "无差别火力覆盖）。该事件必须直接威胁玩家生命，迫使玩家在下回合做出性命攸关的"
        "选择！");
}

QString joinLines(const std::deque<std::string>& dq) {
    QString out;
    for (const auto& s : dq) {
        if (!out.isEmpty()) {
            out += QStringLiteral("\n");
        }
        out += QString::fromStdString(s);
    }
    return out;
}

QString joinStringList(const std::vector<std::string>& v) {
    QString out;
    for (const auto& s : v) {
        if (!out.isEmpty()) {
            out += QStringLiteral("、");
        }
        out += QString::fromStdString(s);
    }
    return out.isEmpty() ? QStringLiteral("（无）") : out;
}

QString npcRelationsSnippet(const std::map<std::string, int>& m) {
    if (m.empty()) {
        return QStringLiteral("（暂无记录）");
    }
    QString lines;
    for (const auto& kv : m) {
        lines += QStringLiteral("- %1 : %2\n")
                     .arg(QString::fromStdString(kv.first))
                     .arg(kv.second);
    }
    return lines.trimmed();
}

}  // namespace

LLMClient::LLMClient(QObject* parent)
    : QObject(parent),
      nam_(new QNetworkAccessManager(this)),
      pending_reply_(nullptr) {}

LLMClient::~LLMClient() {
    // QNetworkReply 若仍在途，abort 后由 finished 收尾；此处不阻塞等待。
    if (pending_reply_) {
        pending_reply_->abort();
        pending_reply_->deleteLater();
        pending_reply_ = nullptr;
    }
}

bool LLMClient::loadApiKeyFromConfig(const QString& config_path) {
    api_key_.clear();
    QFile f(config_path);
    if (!f.open(QIODevice::ReadOnly)) {
        return false;
    }
    const auto doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) {
        return false;
    }
    const QJsonObject obj = doc.object();
    if (obj.contains(QStringLiteral("api_key"))) {
        api_key_ = obj.value(QStringLiteral("api_key")).toString().trimmed();
    } else if (obj.contains(QStringLiteral("apiKey"))) {
        api_key_ = obj.value(QStringLiteral("apiKey")).toString().trimmed();
    }
    return !api_key_.isEmpty();
}

QString LLMClient::apiKey() const { return api_key_; }

bool LLMClient::shouldBlockForCyberpsychosis(const Player& player) {
    return player.humanity() <= 0;
}

QString LLMClient::narrativeFilterForHumanity(int humanity) {
    /**
     * 与 PRD「仅注入一条动态叙事滤镜」对齐：按人性阈值择一。
     * 若正式文档后续给出逐字条款，只需改此函数内字符串。
     */
    if (humanity > 50) {
        return QStringLiteral(
            "[动态叙事滤镜]：用克制、可共情的笔触描写夜之城——保留微弱的希望与道德代价，"
            "强调选择将带来可感知的后果（暴力、背叛或救赎）。");
    }
    if (humanity > 30) {
        return QStringLiteral(
            "[动态叙事滤镜]：语调转向冷漠与疏离；义体与霓虹开始侵蚀感知，偶尔插入轻微的"
            "感官错位（耳鸣、延迟视觉）以暗示排异。");
    }
    if (humanity > 0) {
        return QStringLiteral(
            "[动态叙事滤镜]：强压迫、碎片化感官与暴力暗示占主导；叙事中可穿插短暂"
            "幻听/噪点式词句，但仍须遵守 JSON 输出协议，不得输出协议外长文。");
    }
    return QStringLiteral(
        "[动态叙事滤镜]：人性已耗尽（本档本应被 UI 拦截）；若仍被调用，请输出最小合规"
        " JSON 描述终局僵直状态。");
}

QString LLMClient::buildUserPayloadText(const QString& user_action,
                                        int action_count) {
    QString text = user_action;
    /**
     * 压力机制：action_count > 0 且为 kForcedCrisisInterval 的倍数时，
     * 在发给 API 的 user 角色文本末尾强制追加危机指令。
     */
    if (action_count > 0 &&
        (action_count % kForcedCrisisInterval == 0)) {
        text += forcedCrisisDirective();
    }
    return text;
}

QString LLMClient::buildSystemPrompt(const Player& player,
                                     const WorldState& world) const {
    const QString filter = narrativeFilterForHumanity(player.humanity());

    const QString world_view = QStringLiteral(
        "[世界观设定]：《赛博朋克2077》的夜之城。高科技，低生活，公司财阀几乎掌控一切。"
        "街头利益与暴力是常态，信任稀缺，没人有义务免费帮你。");
    const QString player_bg = QStringLiteral(
        "[玩家背景]：出身街头的底层青年（类似大卫·马丁内斯），刚意外获得一件军用级强力义体。");
    const QString goal = QStringLiteral(
        "[终极目标]：在夜之城“成为传奇”——名扬天下，或在最绚烂的爆炸中死去。");

    const QString npc_rules = QStringLiteral(
        "[NPC 与人物塑造规则]\n"
        "1) 命名：新登场 NPC 须给出稳定简短称呼（约 2～8 字或固定代号），后续回合与 npc_relations 键名保持一致，勿长期用「陌生人」。\n"
        "2) 立体感：凡有台词或关键动作的 NPC，至少体现一项：表层目标、隐藏动机、明显缺陷/偏见、或所属公司/帮派立场；禁止无理由利他"
        "的「全员善人」扁平形象。\n"
        "3) 多元光谱：主动使用利己中间人、画饼高管、疲惫医护、敲诈清道夫、嘴臭但守信的技工、公司狗、创伤后麻木者等；善恶与灰度并存。\n"
        "4) narrative：用可辨识细节区分人物（口音、义体部位、小动作、语速、衣着商标），精炼控制在约 150 字内，不写长篇设定。\n"
        "5) npc_relation_change：互动改变态度时必须输出；name 与已有 npc_relations 键一致，新 NPC 用新 name；无变化时 name 置空字符串、"
        "change 为 0。\n"
        "6) 上下文：结合 global_summary 与 sliding_window 延续已登场 NPC 的性格与关系，勿每回合重置人设。");

    QString state = QStringLiteral(
                       "[当前状态快照]\n"
                       "- hp: %1\n"
                       "- humanity: %2\n"
                       "- action_count: %3\n"
                       "- inventory: %4\n"
                       "- cyberware: %5\n"
                       "- global_summary: %6\n"
                       "- npc_relations:\n%7\n"
                       "- recent_turns_sliding_window (最多 %8 条):\n%9\n")
                       .arg(player.hp())
                       .arg(player.humanity())
                       .arg(player.actionCount())
                       .arg(joinStringList(player.inventory()))
                       .arg(joinStringList(player.cyberware()))
                       .arg(QString::fromStdString(world.globalSummary()))
                       .arg(npcRelationsSnippet(world.npcRelations()))
                       .arg(static_cast<int>(WorldState::kSlidingWindowMax))
                       .arg(joinLines(world.slidingWindow()));

    const QString schema_hint = QStringLiteral(
        "你必须只输出一个 JSON 对象，字段与类型需符合："
        "narrative (string), hp_change (int), humanity_change (int), new_item (string), "
        "new_cyberware (string), "
        "npc_relation_change (object: name string, change int；本回合多 NPC 同时变动时只提交对玩家影响最大的一条), "
        "current_situation_summary (string), is_dead (bool, 可填但客户端忽略，死亡仅以回合末 HP<=0 为准), "
        "is_legend (bool, 若本回合末玩家因 HP<=0 死亡且达成传奇则必须为 true，否则 false)。");

    return world_view + QLatin1Char('\n') + player_bg + QLatin1Char('\n') + goal +
           QLatin1Char('\n') + filter + QLatin1Char('\n') + npc_rules + QLatin1Char('\n') +
           state + QLatin1Char('\n') + schema_hint;
}

void LLMClient::requestChatCompletion(const Player& player,
                                      const WorldState& world,
                                      const QString& user_action_text) {
    if (api_key_.isEmpty()) {
        emit requestFailed(
            QStringLiteral("未加载 API Key：请先成功读取 config.json。"));
        return;
    }
    if (shouldBlockForCyberpsychosis(player)) {
        emit requestFailed(QStringLiteral("人性已耗尽，按 PRD 应阻断网络请求。"));
        return;
    }

    if (pending_reply_) {
        emit requestFailed(QStringLiteral("上一轮流式请求仍在进行，请稍候。"));
        return;
    }

    /**
     * 按 PRD Payload 组装 JSON：model、response_format、messages、temperature。
     * 叙事滤镜：由 Player.humanity 在 buildSystemPrompt() 中动态择一注入 system content。
     * 危机追加：由 action_count 与 kForcedCrisisInterval 在 buildUserPayloadText() 中决定是否在 user 末尾拼接。
     */
    QJsonObject root;
    root.insert(QStringLiteral("model"), QString::fromUtf8(kModelName));
    QJsonObject rf;
    rf.insert(QStringLiteral("type"), QStringLiteral("json_object"));
    root.insert(QStringLiteral("response_format"), rf);
    root.insert(QStringLiteral("temperature"), 0.7);

    QJsonArray messages;
    QJsonObject sys;
    sys.insert(QStringLiteral("role"), QStringLiteral("system"));
    sys.insert(QStringLiteral("content"), buildSystemPrompt(player, world));
    messages.append(sys);

    QJsonObject usr;
    usr.insert(QStringLiteral("role"), QStringLiteral("user"));
    usr.insert(QStringLiteral("content"),
               buildUserPayloadText(user_action_text, player.actionCount()));
    messages.append(usr);

    root.insert(QStringLiteral("messages"), messages);

    QJsonDocument doc(root);
    QNetworkRequest req{QUrl(QString::fromUtf8(kDeepSeekEndpoint))};
    req.setHeader(QNetworkRequest::ContentTypeHeader,
                  QStringLiteral("application/json"));
    req.setRawHeader("Authorization",
                     QByteArrayLiteral("Bearer ") + api_key_.toUtf8());

    pending_reply_ = nam_->post(req, doc.toJson(QJsonDocument::Compact));
    connect(pending_reply_, &QNetworkReply::finished, this,
            &LLMClient::onReplyFinished);
}

void LLMClient::onReplyFinished() {
    auto* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply || reply != pending_reply_) {
        if (reply) {
            reply->deleteLater();
        }
        return;
    }

    pending_reply_ = nullptr;
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        emit requestFailed(
            QStringLiteral("网络错误：%1").arg(reply->errorString()));
        return;
    }

    const int code =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (code != 200) {
        emit requestFailed(
            QStringLiteral("HTTP %1：%2")
                .arg(code)
                .arg(QString::fromUtf8(reply->readAll())));
        return;
    }

    const auto body = reply->readAll();
    const auto doc = QJsonDocument::fromJson(body);
    if (!doc.isObject()) {
        emit requestFailed(QStringLiteral("响应不是 JSON 对象。"));
        return;
    }

    /**
     * OpenAI/DeepSeek 兼容形态：choices[0].message.content
     * 后续可用 nlohmann::json 再解析 content 内的游戏协议 JSON（PRD Parse 阶段）。
     */
    const QJsonObject o = doc.object();
    const QJsonArray choices = o.value(QStringLiteral("choices")).toArray();
    if (choices.isEmpty()) {
        emit requestFailed(QStringLiteral("choices 数组为空。"));
        return;
    }
    const QJsonObject first = choices.at(0).toObject();
    const QJsonObject message = first.value(QStringLiteral("message")).toObject();
    const QString content =
        message.value(QStringLiteral("content")).toString();
    if (content.isEmpty()) {
        emit requestFailed(QStringLiteral("message.content 为空。"));
        return;
    }

    emit completionReceived(stripMarkdownJsonFence(content));
}
