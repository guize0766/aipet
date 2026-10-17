#ifndef DESKTOPPET_H
#define DESKTOPPET_H

#include <QWidget>
#include <QLabel>
#include <QMovie>
#include <QScrollArea>
#include <QMouseEvent>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QContextMenuEvent>
#include <QTimer>

class QSettings;
class QSystemTrayIcon;

// AI 请求类型：总结 / 单块小结 / 合并总结 / 出题 / 判定答案
enum class AiAction {
    Summary,        // 短文档单次总结
    SummaryChunk,   // 长文档：某一块的小结
    SummaryMerge,   // 长文档：所有块小结合并成全文总结
    QuizQuestion,
    QuizJudge
};

// 突击抽背流程状态
enum class QuizState {
    Idle,     // 空闲
    Asking,   // 正在出题
    Judging   // 正在判定
};

class DesktopPet : public QWidget
{
    Q_OBJECT
public:
    DesktopPet(QWidget *parent = nullptr);
    ~DesktopPet();

protected:
    // 鼠标拖拽移动窗口事件
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    // 鼠标进入/离开窗口（贴边收缩后滑出/滑回）
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

    // 拖拽文件“投喂”事件
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

    // 右键菜单（退出 / 置顶 / 状态切换 / 设置 / 考考我 / 任务 / 课表 / 转身）
    void contextMenuEvent(QContextMenuEvent *event) override;
    // 窗口关闭事件（打点排查用）
    void closeEvent(QCloseEvent *event) override;

private slots:
    // 处理大模型返回的结果
    void onAIResponse(QNetworkReply *reply);
    // 弹窗设置 API Key
    void onSetApiKey();
    // 置顶开关
    void onToggleTopmost(bool checked);
    // 退出程序
    void onQuit();
    // 突击抽背：基于已吃掉的资料出题
    void onQuiz();
    // 空闲表情链：定时随机切换表情 + 俏皮话
    void onIdleTick();
    // 课表定时检查
    void onScheduleTick();
    // 帧定时器：驱动逐帧显示（支持转身镜像）
    void onFrameTick();
    // 点击互动后的短暂表情恢复
    void onTalkDone();
    // 番茄钟：每秒倒计时
    void onFocusTick();
    // 开始专注（25 分钟番茄钟）
    void onStartFocus();
    // 暂停 / 继续
    void onTogglePauseFocus();
    // 结束专注
    void onStopFocus();
    // 专注统计（近 7 天柱状图）
    void showStatsDialog();
    // 摸鱼小游戏（接星星）
    void launchStarGame();
    // 任务清单
    void onTodoList();
    // 设置课表
    void onEditSchedule();
    // 转身（左右镜像）
    void onTurnAround();
    // 查看最近总结
    void onViewSummary();
    // 贴边收缩检测（每 1.5 秒检查窗口是否靠边）
    void onDockCheck();
    // 开机自启动开关（写/删 HKCU Run 注册表项，别人可随时在菜单里关掉）
    void onToggleAutostart(bool checked);
    // 生成启动欢迎语（时间问候 + 任务/专注个性化 + 随机鼓励）
    QString buildWelcomeText();
    // 切换在线 AI / 离线 AI（Ollama）
    void onToggleAiMode(bool offline);
    // 学习看板（专注柱状图 + 学习统计 + 成就）
    void showLearnBoard();
    // 一键导出学习笔记（Markdown）
    void onExportNotes();

private:
    QLabel *petLabel;        // 显示宠物动画
    QLabel *bubbleLabel;     // 显示头顶说话气泡
    QLabel *tailLabel;       // 气泡下方的小尾巴（更像对话框）
    QMovie *petMovie;        // 播放GIF动画（帧源）
    QTimer *frameTimer;      // 逐帧取帧驱动显示（支持镜像）
    QTimer *idleTimer;       // 空闲表情链
    QTimer *scheduleTimer;   // 课表检查
    QTimer *talkTimer;       // 点击互动后恢复
    QTimer *m_focusTimer;    // 番茄钟倒计时（每秒）
    QSystemTrayIcon *m_tray; // 托盘图标（结束提醒 / 快捷菜单）
    QPoint dragPosition;     // 记录鼠标拖拽位置
    QPoint pressPos;         // 按下位置（判断单击/拖动）
    QPoint m_lastMovePos;    // 拖拽中上一次鼠标位置（估算松手速度）
    qint64 m_lastMoveMs = 0; // 拖拽中上一次移动的时间
    bool m_dragging = false; // 是否正在拖拽（松手做滑行动画）
    int m_pressZone = -1;    // 按下时命中的宠物分区：0=头部 1=身体 2=手部 -1=其他

    // ---- 陪伴感打磨：贴边收缩 + 闲置状态机 ----
    QTimer *dockTimer;       // 贴边检测定时器
    bool m_docked = false;   // 是否处于“贴边收缩”模式（只露半边脸）
    QPoint m_dockNormalPos;  // 贴边前的正常位置（滑出时恢复用）
    int m_idleStage = 0;     // 闲置档位：0=活跃 1=眨眼(30s) 2=换姿势(2min) 3=打瞌睡(5min) 4=睡觉(10min)
    QString m_companionFreq = "normal"; // 陪伴频率：calm 安静 / normal 适中 / lively 活泼
    QSet<QString> m_saidToday; // 今天已说过的碎碎念（当天不重复）
    QString m_saidDate;        // 记录碎碎念的日期（跨天清空）
    QNetworkAccessManager *netManager; // 网络请求管理器
    QSettings *m_settings;   // 本地配置（API Key 等）
    QString m_lastContent;   // 最近一次喂给 AI 的骨架（断网兜底 + 抽背出题用）

    AiAction m_pendingAction = AiAction::Summary; // （已废弃，保留兼容占位；请求类型随 reply 走）
    QuizState m_quizState = QuizState::Idle;      // 抽背流程状态
    QString m_lastQuizText;  // 当前抽背的题目文本
    QString m_lastQuizAnswer; // 当前抽背题的标准答案（记录错题用）
    QString m_lastUserAnswer; // 同学刚提交的答案（记录错题用）
    bool m_mirrored = false; // 是否左右镜像（转身）
    bool m_busy = false;     // 是否在 AI 处理中（空闲链不打断）
    qint64 m_lastInteractMs = 0; // 上次点击互动时间

    // ---- 番茄钟专注模式 ----
    bool m_focusActive = false;      // 是否在番茄钟流程中（专注或休息）
    bool m_focusPaused = false;      // 是否暂停
    int m_focusPhase = 0;            // 0=专注 1=休息
    int m_focusRemain = 0;           // 当前阶段剩余秒数
    int m_focusTodaySeconds = 0;     // 今日累计专注秒数（会话内存 + 持久化）
    static constexpr int kFocusSeconds = 25 * 60;  // 专注时长
    static constexpr int kBreakSeconds = 5 * 60;   // 休息时长

    // ---- 长文档分段总结 ----
    QStringList m_chunkParts;    // 原文按字数分块
    QStringList m_chunkNotes;    // 各块 AI 小结
    int m_chunkIndex = -1;       // 当前处理到的块序号（-1 = 未分块）
    bool m_chunkMode = false;    // 是否处于分段总结流程

    // ---- 气泡文本保护：超长文本截断 + 完整版存档 ----
    QString m_lastFullText;      // 最近一次被截断的完整文本（查看最近总结用）
    QString m_lastSummaryFile;   // 完整总结落盘路径（查看最近总结用）
    QScrollArea *bubbleScroll;   // 气泡滚动容器（长总结在气泡内滚动查看全文）
    bool m_summaryLock = false;  // 总结成果锁定：点击互动/空闲链不覆盖气泡内容

    void updateFocusBubble();  // 刷新气泡里的倒计时
    void saveFocusToday();     // 今日累计写回配置（focus/daily/yyyyMMdd）
    int loadFocusToday() const; // 读取今日累计（跨天自动清零）
    void notifyStage(const QString &title, const QString &msg); // 右下角通知 + 提醒音
    void playDing();           // 播放内嵌提醒音（叮咚）

    void changePetState(const QString &gifPath, const QString &message);
    // 气泡文本统一入口：超过 140 字自动截断显示，完整文本存档，气泡保持小巧
    void setBubbleText(const QString &text);
    // 把完整长文本落盘到用户目录（供“查看最近总结”读取）
    void saveFullSummary(const QString &text);
    void askAI(const QString &fileContent);   // 资料总结（自动判断短/长文档）
    void sendAI(const QString &systemPrompt, const QString &userContent, AiAction action); // 通用请求
    // 长文档分块（每块 ≤maxChars 字，尽量在段落/句子边界切）
    QStringList chunkText(const QString &content, int maxChars = 2000) const;
    // 逐块请求 AI 小结（思考脸）
    void processNextChunk();
    // 所有块小结完成后，合并成全文总结（开心脸）
    void finishChunkSummary();
    // 从长文档中提取“骨架”（标题 + 考点句），替代粗暴截断
    QString buildSummarySkeleton(const QString &content) const;
    // 用内置轻量解析器提取 PDF 文本
    QString extractPdfText(const QString &path) const;
    // AI 不可用时的本地速览兜底
    QString localFallbackSummary(const QString &content) const;
    // 从候选表情里随机挑一个（避开正在播的）
    QString pickRandomGif(const QStringList &candidates, const QString &current) const;
    // 当天不重复的学习向碎碎念
    QString pickTodayPhrase();
    // 拖拽松手后的滑行动画（抛物线减速 + 屏幕边缘反弹）
    void doSwipeAnim();
    // 贴边收缩：slideIn=true 滑到只露半边脸，false 滑回正常位置
    void applyDockSlide(bool slideIn);
    // 学习统计：喂资料/抽背计数（持久化到 [stats]）
    void addStat(const QString &key, int delta = 1);
    // 判定并解锁成就（达成时气泡反馈）；返回是否刚解锁新成就
    bool checkAchievements();
    // 组装导出用的 Markdown 全文
    QString buildNotesMarkdown();
    // 保存/读取任务清单
    void saveTasks(const QStringList &tasks);
    QStringList loadTasks() const;
    // 保存/读取课表
    void saveSchedule(const QStringList &items);
    QStringList loadSchedule() const;
};

#endif // DESKTOPPET_H
