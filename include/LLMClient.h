#pragma once

#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;
class Player;
class WorldState;

/**
 * @brief DeepSeek Chat Completions 异步客户端骨架（Qt 网络核心）。
 *
 * 约束（与 PRD 一致）：
 * - 禁止阻塞主线程；仅使用 QNetworkAccessManager::post + finished 信号。
 * - API Key 来自本地 config.json，严禁硬编码。
 * - humanity <= 0 时应在 UI 层终局拦截；本类仍提供静态检查辅助函数。
 */
class LLMClient : public QObject {
    Q_OBJECT

public:
    explicit LLMClient(QObject* parent = nullptr);
    ~LLMClient() override;

    /**
     * @brief 从 config.json 读取 API Key（字段名优先 "api_key"，兼容 "apiKey"）。
     * @return 成功返回 true，并可通过 apiKey() 取值；失败返回 false。
     */
    bool loadApiKeyFromConfig(const QString& config_path = QStringLiteral("config.json"));

    QString apiKey() const;

    /**
     * @brief 若人性已耗尽，上层须拒绝发网并根据 PRD 进入赛博精神病终局流程。
     */
    static bool shouldBlockForCyberpsychosis(const Player& player);

    /**
     * @brief 组装异步 POST：model、messages、response_format、temperature。
     *
     * 内部根据 Player.humanity 选择唯一叙事滤镜字符串；若 action_count > 0 且
     * action_count % 30 == 0，则在 user 文本末尾强制追加危机指令。
     */
    void requestChatCompletion(const Player& player,
                               const WorldState& world,
                               const QString& user_action_text);

signals:
    /** 收到 HTTP 200 且解析出 choices[0].message.content 的原始文本（通常为 JSON 对象字符串）。 */
    void completionReceived(const QString& message_content);
    /** 配置缺失、人性拦截、网络错误或 HTTP 非 200、JSON 字段异常等。 */
    void requestFailed(const QString& error_message);

private slots:
    void onReplyFinished();

private:
    QString buildSystemPrompt(const Player& player, const WorldState& world) const;
    static QString buildUserPayloadText(const QString& user_action, int action_count);
    /**
     * @brief PRD：动态叙事滤镜 —— 仅注入一条；PDF 未印出完整分档表，此处用分段字符串实现，可后续与策划对齐替换。
     */
    static QString narrativeFilterForHumanity(int humanity);

    QNetworkAccessManager* nam_;
    QString api_key_;
    QNetworkReply* pending_reply_;
};
