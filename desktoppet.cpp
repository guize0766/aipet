#include "desktoppet.h"
#include "pdfextract.h"
#include "stargame.h"
#include "statsdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkRequest>
#include <QMenu>
#include <QAction>
#include <QActionGroup>
#include <QInputDialog>
#include <QSettings>
#include <QRegularExpression>
#include <QApplication>
#include <QDialog>
#include <QCloseEvent>
#include <QListWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QDateTime>
#include <QPropertyAnimation>
#include <QRandomGenerator>
#include <QDebug>
#include <QSystemTrayIcon>
#include <QIcon>
#include <QPixmap>
#include <QDir>
#include <QStandardPaths>
#include <QTextEdit>
#include <QSet>
#include <QScreen>
#include <QGuiApplication>
#include <QEasingCurve>
#include <QEnterEvent>
#include <QDate>
#include <QFileDialog>
#include <windows.h>
#include <mmsystem.h>

// 空闲/点击互动的俏皮话池
static const QStringList kIdlePhrases = {
    "今天也要好好学习呀！",
    "要不要把复习资料拖给我？",
    "我在看着你学习哦~",
    "学累了就摸摸我，嘿嘿~",
    "DDL 不等人，冲鸭！",
    "知识点都记住了吗？",
    "休息一下，喝口水吧~",
    "右键还能考考我哦！",
    // ---- 学习向碎碎念（当天不重复，陪伴感打磨）----
    "这道题我也不会，咱一起看看？",
    "坐久了起来走走，脖子会酸哦！",
    "还有多久下课呀？我帮你盯着时间！",
    "笔记做完了吗？没做完我可要催了！",
    "书翻到第几页了？我猜你在摸鱼！",
    "记得喝水！学习再忙也要补水！",
    "这一节看完，奖励自己玩一小会儿吧！",
    "代码敲不出来的时候，歇两分钟再战！",
    "今天也要比昨天多记住一个知识点！",
    "困了就趴一会儿，我在旁边守着你！",
    "把不会的题丢给我，我帮你捋捋思路！",
    "复习不是抄书，是闭上眼睛能讲出来！"
};
static const QStringList kClickPhrases = {
    "嘿嘿，干嘛呀~",
    "摸我一下，学得更快哦！",
    "痒痒的啦！",
    "再摸我就要收费啦！",
    "嘿嘿，喜欢我嘛？",
    "戳我干嘛，快去学习！"
};

DesktopPet::DesktopPet(QWidget *parent)
    : QWidget(parent)
{
    // 1. 设置窗口：无边框、背景透明、永远置顶
    this->setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    this->setAttribute(Qt::WA_TranslucentBackground, true);
    this->setAcceptDrops(true); // 允许拖拽文件进来！
    this->resize(300, 420);
    this->setStyleSheet("background: transparent;");
    m_lastInteractMs = QDateTime::currentMSecsSinceEpoch(); // 启动即视为“刚互动过”，从活跃档开始

    // 2. 初始化气泡框（带小尾巴的对话气泡；套上滚动容器，长内容可在气泡内滚动看全文）
    bubbleLabel = new QLabel(this);
    bubbleLabel->setWordWrap(true); // 自动换行
    bubbleLabel->setStyleSheet(
        "QLabel {"
        "background: transparent;"
        "border: none;"
        "padding: 10px 14px;"
        "font-size: 14px;"
        "color: #4a3f52;"
        "line-height: 1.5;"
        "}");
    bubbleLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    setBubbleText("Hello呀！同学！把复习资料(txt/md/代码/PDF)拖给我，我帮你总结考点哦~");

    bubbleScroll = new QScrollArea(this);
    bubbleScroll->setWidget(bubbleLabel);
    bubbleScroll->setWidgetResizable(true);
    bubbleScroll->setFrameShape(QFrame::NoFrame);
    bubbleScroll->setMaximumHeight(150); // 气泡高度上限，防止把整个窗口撑大；内容超高可滚动
    bubbleScroll->viewport()->setAutoFillBackground(false);
    bubbleScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    bubbleScroll->setStyleSheet(
        "QScrollArea {"
        "background-color: rgba(255, 255, 255, 242);"
        "border: 1.5px solid #f2b8d0;"
        "border-radius: 14px;"
        "}"
        "QScrollArea > QWidget > QWidget { background: transparent; }"
        "QScrollBar:vertical {"
        "width: 6px; background: transparent; margin: 2px 0;"
        "}"
        "QScrollBar::handle:vertical {"
        "background: #e5aac6; border-radius: 3px; min-height: 18px;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }"
        );

    // 气泡下方的小尾巴（CSS 三角），指向宠物，更像真正的对话框
    tailLabel = new QLabel(this);
    tailLabel->setObjectName("tailLabel");
    tailLabel->setFixedSize(22, 13);
    tailLabel->setStyleSheet(
        "#tailLabel {"
        "border-left: 11px solid transparent;"
        "border-right: 11px solid transparent;"
        "border-top: 13px solid rgba(255,255,255,242);"
        "background: transparent;"
        "}");

    // 3. 初始化桌宠画面（GIF 已内嵌到程序资源，且为透明背景）
    petLabel = new QLabel(this);
    petLabel->setFixedSize(220, 220);
    petLabel->setScaledContents(true);
    petMovie = new QMovie(this);
    changePetState(":/pet/eye.gif", "你好呀！把复习资料拖给我，我帮你总结考点哦~");

    // 4. 布局：气泡 → 尾巴 → 宠物（尾巴居中紧贴气泡，宠物紧跟其下）
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(0);
    layout->addWidget(bubbleScroll);
    layout->addWidget(tailLabel, 0, Qt::AlignHCenter | Qt::AlignTop);
    layout->addWidget(petLabel, 0, Qt::AlignHCenter | Qt::AlignTop);
    this->setLayout(layout);

    // 5. 帧定时器：手动逐帧取帧显示（支持转身镜像），40ms 轮询 QMovie 当前帧
    frameTimer = new QTimer(this);
    frameTimer->setInterval(40);
    connect(frameTimer, &QTimer::timeout, this, &DesktopPet::onFrameTick);
    frameTimer->start();

    // 5.5 初始化本地配置（提前：陪伴频率、贴边等需要读取；API Key 也存这里）
    m_settings = new QSettings(QSettings::IniFormat, QSettings::UserScope, "AIPet", "AIPet1", this);

    // 6. 陪伴感：空闲状态机（按频率分档检测间隔）+ 贴边收缩检测
    int freqMs = 8000; // 默认“适中”：每 8 秒检测一次闲置档位
    m_companionFreq = m_settings->value("companion/freq", "normal").toString();
    if (m_companionFreq == "calm") freqMs = 12000;        // 安静：12 秒
    else if (m_companionFreq == "lively") freqMs = 4000;  // 活泼：4 秒
    idleTimer = new QTimer(this);
    idleTimer->setInterval(freqMs);
    connect(idleTimer, &QTimer::timeout, this, &DesktopPet::onIdleTick);
    idleTimer->start();

    dockTimer = new QTimer(this);
    dockTimer->setInterval(1500);
    connect(dockTimer, &QTimer::timeout, this, &DesktopPet::onDockCheck);
    dockTimer->start();

    // 7. 课表提醒：每 30 秒检查一次
    scheduleTimer = new QTimer(this);
    scheduleTimer->setInterval(30000);
    connect(scheduleTimer, &QTimer::timeout, this, &DesktopPet::onScheduleTick);
    scheduleTimer->start();

    // 8. 点击互动后的恢复定时器（默认不启动，互动时才启动）
    talkTimer = new QTimer(this);
    talkTimer->setSingleShot(true);
    talkTimer->setInterval(4000);
    connect(talkTimer, &QTimer::timeout, this, &DesktopPet::onTalkDone);

    // 8.1 番茄钟：每秒倒计时（常驻，靠标志位判断是否活跃）
    m_focusTimer = new QTimer(this);
    m_focusTimer->setInterval(1000);
    connect(m_focusTimer, &QTimer::timeout, this, &DesktopPet::onFocusTick);
    m_focusTimer->start();

    // 9. 初始化网络模块
    netManager = new QNetworkAccessManager(this);
    connect(netManager, &QNetworkAccessManager::finished, this, &DesktopPet::onAIResponse);

    // 10. 首次使用：自动弹出 API Key 设置
    if (m_settings->value("api/key", "").toString().trimmed().isEmpty()) {
        setBubbleText("首次使用：请先右键我 → “设置 API Key”，把 DeepSeek 的 Key 喂给我~");
        onSetApiKey();
    }

    // 12. 读取今日专注累计（跨天自动归零）
    m_focusTodaySeconds = loadFocusToday();

    // 12.0 托盘图标（专注结束的右下角通知 / 快捷菜单）
    m_tray = new QSystemTrayIcon(this);
    {
        QMovie trayMovie(":/pet/normal.gif");
        trayMovie.jumpToFrame(0);
        QPixmap trayPix = trayMovie.currentPixmap();
        if (!trayPix.isNull())
            m_tray->setIcon(QIcon(trayPix));
        m_tray->setToolTip("AIPet 桌宠 · 番茄钟");
        QMenu *trayMenu = new QMenu();
        trayMenu->addAction("开始专注（25 分钟）", this, &DesktopPet::onStartFocus);
        trayMenu->addAction("学习看板…", this, &DesktopPet::showStatsDialog);
        trayMenu->addAction("摸鱼小游戏…", this, &DesktopPet::launchStarGame);
        trayMenu->addSeparator();
        trayMenu->addAction("退出", this, &DesktopPet::onQuit);
        m_tray->setContextMenu(trayMenu);
        m_tray->show();
    }

    // 12.1 自动化验证钩子：命令行带 --autofocus / --autofast 时自动开始番茄钟（供脚本/演示用，正常启动无影响）
    if (QApplication::arguments().contains("--autofocus")
        || QApplication::arguments().contains("--autofast")) {
        QTimer::singleShot(500, this, &DesktopPet::onStartFocus);
    }
    // 12.2 自动化验证钩子：--autosummary 时自动加载测试长文档走分段总结（供脚本/演示用）
    if (QApplication::arguments().contains("--autosummary")) {
        QTimer::singleShot(1000, this, [this]() {
            QFile f(QStringLiteral("C:/Users/35272/Desktop/aipet1/tests/long_notes.txt"));
            if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QTextStream in(&f);
                in.setEncoding(QStringConverter::Utf8);
                const QString content = in.readAll();
                f.close();
                if (!content.trimmed().isEmpty()) {
                    changePetState(":/pet/think.gif", "嚼嚼嚼...正在努力消化测试长文档，请稍等...");
                    m_busy = true;
                    askAI(content);
                }
            }
        });
    }
    // 12.4 演示钩子：--autoidle=N 模拟已闲置 N 秒（快速展示闲置状态机各档位：30s 眨眼/2min 换姿势/5min 打瞌睡/10min 睡觉）
    {
        QStringList args = QApplication::arguments();
        bool idleSet = false;
        for (const QString &a : args) {
            if (a.startsWith("--autoidle=")) {
                bool ok = false;
                int secs = a.mid(11).toInt(&ok);
                if (ok && secs > 0) {
                    m_lastInteractMs = QDateTime::currentMSecsSinceEpoch() - (qint64)secs * 1000;
                    idleSet = true;
                }
            }
        }
        if (!idleSet && args.contains("--autoidle")) {
            m_lastInteractMs = QDateTime::currentMSecsSinceEpoch() - 30000; // 无参数默认 30 秒（眨眼档）
        }
        // 12.5 演示钩子：--autodock 启动 3 秒后自动移到屏幕右边缘（展示贴边半透明收缩）
        if (args.contains("--autodock")) {
            QTimer::singleShot(3000, this, [this]() {
                QScreen *screen = QGuiApplication::primaryScreen();
                if (screen) move(screen->availableGeometry().right() + 1 - width(), pos().y());
            });
        }
    }
    // 12.6 自动化验证钩子：--autoquiz 时自动加载抽背资料并触发考考我（供脚本/演示用）
    if (QApplication::arguments().contains("--autoquiz")) {
        QTimer::singleShot(1200, this, [this]() {
            QFile f(QStringLiteral("C:/Users/35272/Desktop/aipet1/tests/demo_notes.txt"));
            if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QTextStream in(&f);
                in.setEncoding(QStringConverter::Utf8);
                m_lastContent = buildSummarySkeleton(in.readAll());
                f.close();
            }
            onQuiz();
        });
    }

    // 12.7 启动时恢复“最近总结”（重启后右键仍能查看上次的总结全文）
    {
        const QString base = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
        QDir dir(base.isEmpty() ? QString(QDir::homePath() + "/AIPet/summaries")
                                : QString(base + "/AIPet/summaries"));
        QFile fl(dir.filePath("latest_summary.txt"));
        if (fl.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&fl);
            in.setEncoding(QStringConverter::Utf8);
            m_lastFullText = in.readAll();
            fl.close();
            m_lastSummaryFile = dir.filePath("latest_summary.txt");
        }
    }

    // 12.8 启动欢迎语：每次启动（含开机自启）打个招呼 + 按当天任务/专注状态个性化鼓励
    QTimer::singleShot(2500, this, [this]() {
        changePetState(":/pet/smile.gif", buildWelcomeText());
        talkTimer->start(); // 4 秒后恢复
    });

    // 12.9 验证钩子：--autoboard 启动 3 秒后打开学习看板（截图验证用）
    if (QApplication::arguments().contains("--autoboard")) {
        QTimer::singleShot(3000, this, [this]() { showStatsDialog(); });
    }
    // 12.10 验证钩子：--autoexport 启动后直接把学习笔记导出到桌面测试文件（验证 Markdown 组装）
    if (QApplication::arguments().contains("--autoexport")) {
        QTimer::singleShot(4000, this, [this]() {
            QFile f(QDir::homePath() + "/Desktop/AIPet学习笔记_验证.md");
            if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream out(&f);
                out.setEncoding(QStringConverter::Utf8);
                out << buildNotesMarkdown();
                f.close();
            }
        });
    }
}

DesktopPet::~DesktopPet() {

    saveFocusToday(); // 退出时把今日专注时长写回
}

// --- 窗口关闭事件（排查谁关掉了桌宠） ---
void DesktopPet::closeEvent(QCloseEvent *event) {

    QWidget::closeEvent(event);
}

// 切换宠物动画和气泡文字（路径一律使用内嵌资源 :/pet/xxx.gif）
void DesktopPet::changePetState(const QString &gifPath, const QString &message) {
    petMovie->stop();
    petMovie->setFileName(gifPath);
    petMovie->start();
    if (!message.isNull()) setBubbleText(message);
}

// --- 气泡文本统一入口：完整内容直接显示在气泡里，超高时在气泡内滚动查看（不再截断丢内容） ---
void DesktopPet::setBubbleText(const QString &text) {
    bubbleLabel->setText(text);
    // 超长文本同步存档备份（右键「查看最近总结」仍可弹窗看全文）
    if (text.length() > 140) {
        m_lastFullText = text;
        saveFullSummary(text);
    }
}

// --- 完整长文本落盘到用户目录（同时写“最近总结”固定文件，重启后仍可查看） ---
void DesktopPet::saveFullSummary(const QString &text) {
    const QString base = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    QDir dir(base.isEmpty() ? QString(QDir::homePath() + "/AIPet/summaries")
                            : QString(base + "/AIPet/summaries"));
    if (!dir.exists()) dir.mkpath(".");
    // 1) 带时间戳的历史存档（保留每次记录）
    const QString path = dir.filePath(
        "AIPet_summary_" + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + ".txt");
    QFile f(path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&f);
        out.setEncoding(QStringConverter::Utf8);
        out << text;
        f.close();
    }
    // 2) 固定名“最近总结”文件：覆盖式，重启后右键「查看最近总结…」仍能打开
    const QString latest = dir.filePath("latest_summary.txt");
    QFile fl(latest);
    if (fl.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&fl);
        out.setEncoding(QStringConverter::Utf8);
        out << text;
        fl.close();
        m_lastSummaryFile = latest; // 查看入口优先指向这份固定文件
    }
}

// --- 查看最近总结：只读弹窗展示完整全文（可滚动/可复制；重启后仍可用） ---
void DesktopPet::onViewSummary() {
    QString full = m_lastFullText;
    // 优先读磁盘上的“最近总结”固定文件（跨会话有效）
    const QString base = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    QDir dir(base.isEmpty() ? QString(QDir::homePath() + "/AIPet/summaries")
                            : QString(base + "/AIPet/summaries"));
    const QString latest = dir.filePath("latest_summary.txt");
    QFile fl(latest);
    if (fl.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&fl);
        in.setEncoding(QStringConverter::Utf8);
        const QString disk = in.readAll();
        fl.close();
        if (!disk.trimmed().isEmpty()) full = disk;
    }
    if (full.trimmed().isEmpty() && !m_lastSummaryFile.isEmpty()) {
        QFile f(m_lastSummaryFile);
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&f);
            in.setEncoding(QStringConverter::Utf8);
            full = in.readAll();
            f.close();
        }
    }
    if (full.trimmed().isEmpty()) {
        setBubbleText("我还没有完整的总结存档呢～先拖份资料给我吧！");
        return;
    }
    QDialog dlg(this);
    dlg.setWindowTitle("最近总结 · 全文");
    dlg.resize(560, 480);
    QVBoxLayout *lay = new QVBoxLayout(&dlg);
    QTextEdit *view = new QTextEdit(&dlg);
    view->setReadOnly(true);
    view->setPlainText(full);
    view->setLineWrapMode(QTextEdit::WidgetWidth);
    QPushButton *closeBtn = new QPushButton("关 闭", &dlg);
    lay->addWidget(view);
    lay->addWidget(closeBtn, 0, Qt::AlignHCenter);
    QObject::connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    dlg.exec();
}

// --- 帧驱动：取 QMovie 当前帧显示，支持镜像 ---
void DesktopPet::onFrameTick() {
    QImage img = petMovie->currentImage();
    if (img.isNull()) return;
    if (m_mirrored) img = img.mirrored(true, false);
    petLabel->setPixmap(QPixmap::fromImage(img));
}

// --- 鼠标：按下记录位置与分区；拖拽中记录速度；释放时小位移=分区互动，大位移=松手滑行 ---
void DesktopPet::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        dragPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
        pressPos = event->position().toPoint();
        m_lastMovePos = event->globalPosition().toPoint();
        m_lastMoveMs = QDateTime::currentMSecsSinceEpoch();
        m_dragging = false;
        // 命中宠物分区：头 / 身体 / 手（相对 petLabel 的二维分区）
        m_pressZone = -1;
        if (!m_busy) {
            QPoint rel = event->position().toPoint() - petLabel->geometry().topLeft();
            if (petLabel->geometry().contains(event->position().toPoint())) {
                double fy = (double)rel.y() / petLabel->height();
                double fx = (double)rel.x() / petLabel->width();
                if (fy < 0.40) m_pressZone = 0;                 // 头部：摸头
                else if (fx < 0.30 || fx > 0.70) m_pressZone = 2; // 左右手：碰手
                else m_pressZone = 1;                            // 身体/脸：戳脸
            }
        }
        // 用户按下即算互动：重置闲置档位
        m_idleStage = 0;
        event->accept();
    }
}

void DesktopPet::mouseMoveEvent(QMouseEvent *event) {
    if (event->buttons() & Qt::LeftButton) {
        qint64 now = QDateTime::currentMSecsSinceEpoch();
        // 移动距离 >6px 视为拖拽（用于松手滑行）
        if ((event->globalPosition().toPoint() - m_lastMovePos).manhattanLength() > 6) {
            m_dragging = true;
            m_lastMovePos = event->globalPosition().toPoint();
            m_lastMoveMs = now;
        }
        move(event->globalPosition().toPoint() - dragPosition);
        event->accept();
    }
}

void DesktopPet::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        QPoint delta = event->position().toPoint() - pressPos;
        if (delta.manhattanLength() >= 8) {
            // 拖拽松手 → 物理滑行 + 屏幕边缘反弹
            doSwipeAnim();
            m_dragging = false;
            event->accept();
            return;
        }
        m_dragging = false;
        // 几乎没移动 → 视为“点击互动”（双击会先触发两次单击，用时间间隔过滤）
        if (!m_busy) {
            qint64 now = QDateTime::currentMSecsSinceEpoch();
            if (now - m_lastInteractMs > 350) { // 过滤双击触发的第一次单击
                m_lastInteractMs = now;
                if (m_summaryLock) {
                    // 总结成果展示中：点击只换表情动作，不覆盖气泡里的总结内容
                    QString gif = pickRandomGif(
                        {":/pet/eye.gif", ":/pet/normal.gif", ":/pet/smile.gif", ":/pet/think.gif"},
                        petMovie->fileName());
                    petMovie->stop();
                    petMovie->setFileName(gif);
                    petMovie->start();
                    event->accept();
                    return;
                }
                // 分区互动：摸头 / 戳脸 / 碰手，各自表情+台词；非宠物区保持随机互动
                QString gif, phrase;
                if (m_pressZone == 0) {
                    static const QStringList headPhrases = {
                        "嘿嘿，最喜欢被摸头了~", "蹭蹭你手心！", "摸头摸头，学习更有劲！"};
                    gif = ":/pet/smile.gif";
                    phrase = headPhrases[QRandomGenerator::global()->bounded(headPhrases.size())];
                } else if (m_pressZone == 2) {
                    static const QStringList handPhrases = {
                        "挥挥～嘿！手手给你碰一下！", "碰手手？那我挥一挥！", "嘿嘿，拉钩上吊，一百年不许变！"};
                    gif = pickRandomGif({":/pet/eye.gif", ":/pet/smile.gif"}, petMovie->fileName());
                    phrase = handPhrases[QRandomGenerator::global()->bounded(handPhrases.size())];
                } else if (m_pressZone == 1) {
                    static const QStringList facePhrases = {
                        "哼！腮帮子鼓起来啦！", "戳我脸！我记仇了！", "呜哇！脸都被你戳扁啦！"};
                    gif = ":/pet/think.gif";
                    phrase = facePhrases[QRandomGenerator::global()->bounded(facePhrases.size())];
                } else {
                    gif = pickRandomGif(
                        {":/pet/eye.gif", ":/pet/normal.gif", ":/pet/smile.gif", ":/pet/think.gif"},
                        petMovie->fileName());
                    phrase = kClickPhrases[QRandomGenerator::global()->bounded(kClickPhrases.size())];
                }
                changePetState(gif, phrase);
                talkTimer->start(); // 4 秒后恢复
            }
        }
        event->accept();
    }
}

void DesktopPet::mouseDoubleClickEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_lastInteractMs = QDateTime::currentMSecsSinceEpoch();
        if (!m_summaryLock) {
            changePetState(":/pet/smile.gif", "嘿嘿！一起加油鸭！");
        }
        // 窗口 Q 弹一下（透明度脉冲）
        QPropertyAnimation *a = new QPropertyAnimation(this, "windowOpacity", this);
        a->setDuration(400);
        a->setStartValue(0.7);
        a->setKeyValueAt(0.5, 1.0);
        a->setEndValue(1.0);
        a->start(QPropertyAnimation::DeleteWhenStopped);
        talkTimer->start();
        event->accept();
    }
}

// --- 拖拽松手：抛物线减速滑行 + 屏幕边缘轻微反弹（经典桌宠物理感） ---
void DesktopPet::doSwipeAnim() {
    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen) return;
    QRect avail = screen->availableGeometry();
    QPoint cur = pos();
    // 速度向量：最后两次移动的位移，缩放 4 倍作为滑行距离
    QPoint v = cur - m_lastMovePos;
    QPoint target = cur + QPoint(v.x() * 4, v.y() * 4);

    // 越界则 clamp 到屏幕内，并标记撞边（用于反弹）
    bool hitEdge = false;
    QPoint clamped = target;
    if (clamped.x() < avail.left()) { clamped.setX(avail.left()); hitEdge = true; }
    if (clamped.x() + width() > avail.right() + 1) { clamped.setX(avail.right() + 1 - width()); hitEdge = true; }
    if (clamped.y() < avail.top()) { clamped.setY(avail.top()); hitEdge = true; }
    if (clamped.y() + height() > avail.bottom() + 1) { clamped.setY(avail.bottom() + 1 - height()); hitEdge = true; }

    if (clamped == cur && !hitEdge) return; // 几乎没速度，不滑行

    QPropertyAnimation *slide = new QPropertyAnimation(this, "pos", this);
    slide->setDuration(hitEdge ? 480 : 320);
    slide->setStartValue(cur);
    if (hitEdge) {
        // 撞边轻微反弹：滑到边界后向屏幕外轻弹 6px 再弹回
        QPoint overshoot = clamped;
        if (clamped.x() == avail.left()) overshoot.setX(avail.left() - 6);
        if (clamped.x() + width() > avail.right() - 1) overshoot.setX(avail.right() + 1 - width() + 6);
        if (clamped.y() == avail.top()) overshoot.setY(avail.top() - 6);
        if (clamped.y() + height() > avail.bottom() - 1) overshoot.setY(avail.bottom() + 1 - height() + 6);
        slide->setKeyValueAt(0.55, clamped);
        slide->setKeyValueAt(0.78, overshoot);
        slide->setEndValue(clamped);
    } else {
        slide->setEndValue(clamped);
    }
    slide->setEasingCurve(QEasingCurve::OutCubic);
    slide->start(QPropertyAnimation::DeleteWhenStopped);
}

// --- 贴边收缩：slideIn=true 半透明滑到只露半边脸，false 滑回正常位置 ---
void DesktopPet::applyDockSlide(bool slideIn) {
    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen) return;
    QRect avail = screen->availableGeometry();
    QPoint cur = pos();
    QPoint target;
    if (slideIn) {
        m_dockNormalPos = cur; // 记住正常位置，鼠标移过来时滑回
        int exposed = 60;      // 只露半边脸的宽度
        if (cur.x() <= avail.left() + 10) {
            target = QPoint(avail.left() - (width() - exposed), cur.y());
        } else if (cur.x() + width() >= avail.right() - 9) {
            target = QPoint(avail.right() + 1 - exposed, cur.y());
        } else {
            return; // 不在左右边缘，不收缩
        }
    } else {
        target = m_dockNormalPos;
    }
    if (target == cur) return;

    QPropertyAnimation *a = new QPropertyAnimation(this, "pos", this);
    a->setDuration(300);
    a->setStartValue(cur);
    a->setEndValue(target);
    a->setEasingCurve(QEasingCurve::InOutQuad);
    a->start(QPropertyAnimation::DeleteWhenStopped);

    // 同步半透明：贴边降到 0.55，滑出恢复 1.0
    QPropertyAnimation *op = new QPropertyAnimation(this, "windowOpacity", this);
    op->setDuration(300);
    op->setStartValue(windowOpacity());
    op->setEndValue(slideIn ? 0.55 : 1.0);
    op->start(QPropertyAnimation::DeleteWhenStopped);
}

// --- 贴边检测：窗口靠左右边缘时自动半透明收缩，只露半边脸，不挡课件 ---
void DesktopPet::onDockCheck() {
    if (m_busy || m_focusActive || m_summaryLock) return; // 忙碌/专注/总结展示中不收缩
    if (m_dragging) return; // 正在拖拽不收缩
    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen) return;
    QRect avail = screen->availableGeometry();
    QPoint cur = pos();
    bool nearEdge = (cur.x() <= avail.left() + 10) ||
                    (cur.x() + width() >= avail.right() - 9);
    if (nearEdge && !m_docked && !underMouse()) {
        m_docked = true;
        applyDockSlide(true);
    } else if (!nearEdge && m_docked) {
        m_docked = false;
        applyDockSlide(false);
    }
}

// --- 鼠标移上来：贴边时平滑滑回正常位置、恢复不透明 ---
void DesktopPet::enterEvent(QEnterEvent *event) {
    if (m_docked) {
        m_docked = false;
        applyDockSlide(false);
    }
    QWidget::enterEvent(event);
}

// --- 鼠标移开：若窗口仍贴着边缘，稍后自动缩回半边脸 ---
void DesktopPet::leaveEvent(QEvent *event) {
    if (!m_docked) {
        QScreen *screen = QGuiApplication::primaryScreen();
        if (screen) {
            QRect avail = screen->availableGeometry();
            QPoint cur = pos();
            if ((cur.x() <= avail.left() + 10) ||
                (cur.x() + width() >= avail.right() - 9)) {
                QTimer::singleShot(350, this, [this]() { onDockCheck(); });
            }
        }
    }
    QWidget::leaveEvent(event);
}

// --- 当天不重复的学习向碎碎念（跨天自动重置） ---
QString DesktopPet::pickTodayPhrase() {
    QString today = QDate::currentDate().toString("yyyy-MM-dd");
    if (m_saidDate != today) {
        m_saidDate = today;
        m_saidToday.clear();
    }
    QString picked;
    for (int i = 0; i < 20; ++i) {
        QString cand = kIdlePhrases[QRandomGenerator::global()->bounded(kIdlePhrases.size())];
        if (!m_saidToday.contains(cand)) { picked = cand; break; }
    }
    if (picked.isEmpty()) { // 池子今天说完了（20 条很难），重置再挑
        m_saidToday.clear();
        picked = kIdlePhrases[QRandomGenerator::global()->bounded(kIdlePhrases.size())];
    }
    m_saidToday.insert(picked);
    return picked;
}

// --- 启动欢迎语：时间问候 + 任务/专注个性化 + 随机鼓励 ---
QString DesktopPet::buildWelcomeText() {
    static const QStringList kWelcomePool = {
        "又回来啦！不要忘记今天的任务哦~",
        "欢迎回来！今天也要元气满满！",
        "回来啦～今天想先学点什么呢？",
        "又见面啦！要不要先把今天的任务安排上？",
        "嗨！我在呢，一起加油鸭！",
        "欢迎回来！记得先喝水、再开工~"
    };
    const int hour = QTime::currentTime().hour();
    QString base;
    if (hour >= 5 && hour < 11) base = "早上好呀！";
    else if (hour >= 11 && hour < 14) base = "中午好！";
    else if (hour >= 14 && hour < 19) base = "下午好~";
    else if (hour >= 19 && hour < 23) base = "晚上好！";
    else base = "夜深了，注意休息呀~";

    // 个性化：今天还有任务没完成？
    const QStringList tasks = loadTasks();
    QString msg = base + " " + kWelcomePool[QRandomGenerator::global()->bounded(kWelcomePool.size())];
    if (!tasks.isEmpty()) {
        msg = base + " 又回来啦！今天还有 " + QString::number(tasks.size())
              + " 个任务等着你，别偷懒哦~";
    } else if (loadFocusToday() == 0) {
        msg = base + " 欢迎回来！今天还没开始专注，要不要来一轮番茄钟？";
    }
    return msg;
}

// --- 开机自启动开关：写/删 HKCU Run 注册表项（勾选=开机启动，取消=随时可关） ---
void DesktopPet::onToggleAutostart(bool checked) {
    QSettings runKey(QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"),
                     QSettings::NativeFormat);
    if (checked) {
        QString exe = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
        runKey.setValue("AIPet1", QString("\"%1\"").arg(exe));
        setBubbleText("已开启开机自启动～下次开机我会准时等你！");
    } else {
        runKey.remove("AIPet1");
        setBubbleText("已关闭开机自启动～想开的时候随时叫我！");
    }
    m_settings->setValue("startup/autostart", checked);
}

// --- 拖拽文件投喂交互 ---
void DesktopPet::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction(); // 允许拖入
        setBubbleText("哇！是好吃的复习资料吗？快松手喂我！");
    }
}

void DesktopPet::dragMoveEvent(QDragMoveEvent *event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void DesktopPet::dropEvent(QDropEvent *event) {
    QList<QUrl> urls = event->mimeData()->urls();
    if (urls.isEmpty()) return;
    QString filePath = urls.first().toLocalFile();
    if (filePath.isEmpty()) return;

    m_quizState = QuizState::Idle; // 新资料来了，旧的抽背作废
    m_chunkMode = false;           // 新资料来了，中断旧的分段总结流程（防止残留回调继续）
    m_summaryLock = false;         // 新资料来了，解除旧的总结锁定

    QString content;
    QString suffix = QFileInfo(filePath).suffix().toLower();

    // PDF：走内置解析器提取文本；其他：按文本文件读取
    if (suffix == "pdf") {
        content = extractPdfText(filePath);
        if (content.trimmed().isEmpty()) {
            setBubbleText("这份 PDF 我啃不出字来…它可能是扫描件/图片版，暂时喂不了我呜呜~");
            return;
        }
    } else {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            setBubbleText("呜呜，这个文件我咬不动（读取失败）...");
            return;
        }
        QTextStream in(&file);
        in.setEncoding(QStringConverter::Utf8); // 防止中文乱码
        content = in.readAll();
        file.close();
    }

    // 切换成“思考/咀嚼”动画，按内容长度自动走“短文档单次总结”或“长文档分段总结”
    changePetState(":/pet/think.gif", "嚼嚼嚼...正在努力消化这份资料，请稍等...");
    m_busy = true;
    askAI(content); // 传全文；askAI 内部 >2200 字时自动分段处理
}

// --- 提取文档“骨架”：优先保留标题行和含考点关键词的句子 ---
QString DesktopPet::buildSummarySkeleton(const QString &content) const {
    QStringList lines = content.split(QRegularExpression("[\r\n]+"), Qt::SkipEmptyParts);
    QStringList parts;
    int kept = 0; // 普通行计数，防止只取开头
    for (const QString &line : lines) {
        QString t = line.trimmed();
        if (t.isEmpty()) continue;

        bool isHeading = t.startsWith("#")
                      || t.startsWith("第")
                      || (t.at(0).isDigit() && (t.contains(".") || t.contains("、") || t.contains("章") || t.contains("节")));
        bool isKeyword = t.contains("重点") || t.contains("考点") || t.contains("定义")
                      || t.contains("定理") || t.contains("公式") || t.contains("结论")
                      || t.contains("注意") || t.contains("要点") || t.contains("重要")
                      || t.contains("必须") || t.contains("核心") || t.contains("总结");

        if (isHeading) {
            parts.append("◆ " + t.left(80));
        } else if (isKeyword) {
            parts.append("▸ " + t.left(120));
        } else if (kept < 30) { // 其他段落取段首，避免漏掉无关键词的重要段落
            parts.append("· " + t.left(60));
        }
        ++kept;

        if (parts.join("\n").length() > 3600) break; // 控制喂给 AI 的总量
    }
    return parts.join("\n");
}

// --- 用内置轻量解析器提取 PDF 文本（无第三方依赖） ---
QString DesktopPet::extractPdfText(const QString &path) const {
    return extractPdfTextSimple(path);
}

// --- 资料总结入口（自动判断短/长文档） ---
void DesktopPet::askAI(const QString &fileContent) {
    addStat("docsFed", 1); // 学习统计：喂资料 +1（拖拽与演示钩子统一计入）
    checkAchievements();
    m_lastContent = buildSummarySkeleton(fileContent); // 记住骨架：断网兜底 + 抽背出题用
    m_summaryLock = false; // 开始新总结，解除旧的锁定

    // 长文档（>2200 字）：分块逐段总结，全部完成后合并
    if (fileContent.trimmed().length() > 2200) {
        m_chunkParts = chunkText(fileContent, 2000);
        m_chunkNotes.clear();
        m_chunkIndex = 0;
        m_chunkMode = true;
        setBubbleText(QString("这份资料有点长（%1 字）！我把它分成 %2 块慢慢啃，"
                                     "每啃完一块就眨一下眼，全部啃完给你出全文总结和考点清单~")
                                 .arg(fileContent.trimmed().length())
                                 .arg(m_chunkParts.size()));
        processNextChunk();
        return;
    }

    // 短文档：保持原有单次总结
    m_chunkMode = false;
    m_chunkIndex = -1;
    const QString systemPrompt =
        "你是一个可爱的大学生桌面陪伴助手。请用傲娇又俏皮的语气，帮同学总结以下资料的【3个核心考点】，"
        "字数控制在150字以内，方便在气泡框显示。";
    sendAI(systemPrompt, fileContent, AiAction::Summary);
}

// --- 长文档分块：每块 ≤maxChars 字，优先段落、其次句子边界 ---
QStringList DesktopPet::chunkText(const QString &content, int maxChars) const {
    QStringList blocks;
    QString rest = content.trimmed();
    const int minCut = maxChars * 3 / 5; // 切点下限，避免块过碎
    while (!rest.isEmpty()) {
        if (rest.length() <= maxChars) {
            blocks.append(rest);
            break;
        }
        int cut = -1;
        // 优先找 maxChars 内最近的段落边界（\n\n）
        int p = rest.lastIndexOf(QStringLiteral("\n\n"), maxChars);
        if (p > minCut) {
            cut = p;
        } else {
            // 其次句子边界（\n 或中文句读/逗号）
            for (int pos = maxChars; pos >= minCut; --pos) {
                const QChar c = rest.at(pos);
                const ushort u = c.unicode();
                if (c == '\n' || c == '.' || c == ','
                    || u == 0x3002 /*。*/ || u == 0xFF1B /*；*/ || u == 0xFF01 /*！*/
                    || u == 0xFF1F /*？*/ || u == 0xFF0C /*，*/) {
                    cut = pos + 1;
                    break;
                }
            }
        }
        if (cut <= minCut) cut = maxChars; // 实在没边界就硬切
        blocks.append(rest.left(cut).trimmed());
        rest = rest.mid(cut).trimmed();
        if (blocks.size() > 64) { // 安全上限，防止死循环/异常超长文本
            if (!rest.isEmpty()) blocks.append(rest);
            break;
        }
    }
    return blocks;
}

// --- 逐块请求 AI 小结（思考脸 + 进度气泡） ---
void DesktopPet::processNextChunk() {
    if (m_chunkIndex < 0 || m_chunkIndex >= m_chunkParts.size()) {
        finishChunkSummary();
        return;
    }
    changePetState(":/pet/think.gif",
                   QString("正在消化第 %1/%2 块…").arg(m_chunkIndex + 1).arg(m_chunkParts.size()));
    const QString chunkPrompt =
        "你是认真的大学生学习伙伴。下面是从一份长课件中切出的一段内容，"
        "请用不超过120字概括这一段的重点，只输出概括本身。内容如下：\n";
    sendAI(chunkPrompt, m_chunkParts[m_chunkIndex], AiAction::SummaryChunk);
}

// --- 所有块小结完成 → 合并成「全文核心总结 + 高频考点清单」 ---
void DesktopPet::finishChunkSummary() {
    if (m_chunkNotes.isEmpty()) {
        m_busy = false;
        m_chunkMode = false;
        m_chunkIndex = -1;
        changePetState(":/pet/normal.gif", "呜呜…每块都没啃出东西，可能要检查网络或 API Key 啦~");
        return;
    }
    changePetState(":/pet/think.gif", "所有块都啃完啦！正在拼装全文总结和考点清单…");
    QString merged = m_chunkNotes.join("\n\n---\n\n");
    const QString mergePrompt =
        "你是认真的大学生学习伙伴。下面是同一份长课件逐段总结出的小结，"
        "请把它们整合成给同学的最终成果，严格输出以下格式，不要输出其他内容：\n"
        "【全文核心总结】不超过180字\n"
        "【高频考点清单】5条以内，每条一行，用“考点X：”开头";
    sendAI(mergePrompt, merged, AiAction::SummaryMerge);
}

// --- 通用 AI 请求：在线 DeepSeek / 离线 Ollama（右键可切换） ---
void DesktopPet::sendAI(const QString &systemPrompt, const QString &userContent, AiAction action) {
    const bool offline = m_settings->value("api/mode", "online").toString() == "offline";

    if (!offline) {
        QString apiKey = m_settings->value("api/key", "").toString().trimmed();
        if (apiKey.isEmpty()) {
            m_busy = false;
            changePetState(":/pet/normal.gif", "还没配置 API Key 呢～右键我 → “设置 API Key”粘贴一下，我马上开工！");
            m_quizState = QuizState::Idle;
            return;
        }
    }

    // 在线：DeepSeek（OpenAI 兼容）；离线：本地 Ollama 的 OpenAI 兼容端点（演示可拔网线）
    QUrl url;
    if (offline) {
        const QString base = m_settings->value("api/ollamaUrl", "http://127.0.0.1:11434/v1").toString();
        url = QUrl(base + "/chat/completions");
    } else {
        url = QUrl("https://api.deepseek.com/chat/completions");
    }
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    if (!offline)
        request.setRawHeader("Authorization", ("Bearer " + m_settings->value("api/key", "").toString().trimmed()).toUtf8());
    request.setTransferTimeout(offline ? 120000 : 30000); // 本地大模型推理慢，给足 2 分钟

    QJsonObject systemMsg;
    systemMsg["role"] = "system";
    systemMsg["content"] = systemPrompt;

    QJsonObject userMsg;
    userMsg["role"] = "user";
    userMsg["content"] = userContent;

    QJsonArray messages;
    messages.append(systemMsg);
    messages.append(userMsg);

    QJsonObject jsonBody;
    jsonBody["model"] = offline ? m_settings->value("api/ollamaModel", "qwen2.5:7b").toString()
                                : "deepseek-chat";
    jsonBody["messages"] = messages;

    QJsonDocument doc(jsonBody);
    // 把请求类型绑定到 reply 上：并发请求时各回各的，避免全局状态错配
    QNetworkReply *rep = netManager->post(request, doc.toJson());
    rep->setProperty("aiAction", int(action));
}

// --- 接收 AI 的回复；按请求类型分发 ---
void DesktopPet::onAIResponse(QNetworkReply *reply) {
    const AiAction action = static_cast<AiAction>(reply->property("aiAction").toInt());
    const bool ok = (reply->error() == QNetworkReply::NoError);
    QString aiText;
    if (ok) {
        QByteArray responseData = reply->readAll();
        QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData);
        QJsonObject jsonObj = jsonDoc.object();
        QJsonArray choices = jsonObj["choices"].toArray();
        if (!choices.isEmpty())
            aiText = choices[0].toObject()["message"].toObject()["content"].toString().trimmed();
    }
    reply->deleteLater();

    if (action == AiAction::Summary) {
        m_busy = false;
        if (ok && !aiText.isEmpty()) {
            changePetState(":/pet/smile.gif", "消化完毕！考点如下：\n" + aiText);
            m_summaryLock = true; // 总结成果锁定，点击/空闲不覆盖
        } else {
            changePetState(":/pet/normal.gif",
                "AI 打了个哈欠没接上话，先给你一份本地速览顶着：\n" + localFallbackSummary(m_lastContent)
                + "\n（网络恢复后再拖一次就有完整总结啦）");
            m_summaryLock = true;
        }
        return;
    }

    // 长文档：某一块小结完成 → 眨一下眼，接着啃下一块
    if (action == AiAction::SummaryChunk) {
        const QString note = (ok && !aiText.isEmpty()) ? aiText
                             : ("（本块本地速览）" + localFallbackSummary(m_chunkParts.value(m_chunkIndex)));
        m_chunkNotes.append(note);
        ++m_chunkIndex;
        changePetState(":/pet/eye.gif",
                       QString("第 %1/%2 块啃完啦！眨眨眼继续…")
                           .arg(m_chunkIndex).arg(m_chunkParts.size()));
        // 眨眼留 700ms 再继续下一块（或全部完成进入合并）
        QTimer::singleShot(700, this, [this]() {
            if (!m_chunkMode) return; // 已被中断（新拖拽/退出等）
            if (m_chunkIndex >= m_chunkParts.size())
                finishChunkSummary();
            else
                processNextChunk();
        });
        return;
    }

    // 长文档：合并总结完成 → 开心脸 + 全文成果
    if (action == AiAction::SummaryMerge) {
        m_busy = false;
        m_chunkMode = false;
        m_chunkIndex = -1;
        if (ok && !aiText.isEmpty()) {
            changePetState(":/pet/smile.gif", "全文消化完毕！\n" + aiText);
        } else {
            changePetState(":/pet/normal.gif",
                "合并时网络开小差了呜呜…先给你逐块小结顶着：\n" + m_chunkNotes.join("\n---\n"));
        }
        m_summaryLock = true; // 总结成果锁定，点击/空闲不覆盖
        return;
    }

    if (action == AiAction::QuizQuestion) {
        if (!ok || aiText.isEmpty()) {
            m_quizState = QuizState::Idle;
            m_busy = false;
            changePetState(":/pet/normal.gif", "出题网络开小差了呜呜…再右键我试试？");
            return;
        }
        // 解析【题目】：取【题目】到【答案】之间的内容；没有标签则用整段
        QString question = aiText;
        QRegularExpression reQ("【题目】\\s*(.*?)(?:【答案】|$)",
                               QRegularExpression::DotMatchesEverythingOption);
        auto m = reQ.match(aiText);
        if (m.hasMatch() && !m.captured(1).trimmed().isEmpty())
            question = m.captured(1).trimmed();
        else if (aiText.contains("【答案】"))
            question = aiText.section("【答案】", 0, 0).trimmed();
        m_lastQuizText = question;
        // 记录标准答案（错题本用）
        m_lastQuizAnswer.clear();
        if (aiText.contains("【答案】")) {
            QString ansPart = aiText.section("【答案】", 1, 1).trimmed();
            ansPart = ansPart.section("【解析】", 0, 0).trimmed(); // 去掉【解析】尾巴
            if (ansPart.length() > 120) ansPart = ansPart.left(120) + "…";
            m_lastQuizAnswer = ansPart;
        }
        addStat("quizTaken", 1); // 学习统计：抽背次数 +1
        checkAchievements();

        // 非模态输入框（堆分配 + deleteLater，避免 exec 阻塞与栈对象析构崩溃）
        QDialog *dlg = new QDialog(this);
        dlg->setWindowTitle("突击抽背");
        dlg->setMinimumWidth(440);
        QVBoxLayout *lay = new QVBoxLayout(dlg);
        QLabel *qLabel = new QLabel("请作答（按你学到的来，大胆写）：\n\n" + question, dlg);
        qLabel->setWordWrap(true);
        qLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        QTextEdit *answerEdit = new QTextEdit(dlg);
        answerEdit->setPlaceholderText("输入你的答案…");
        answerEdit->setFixedHeight(120);
        QHBoxLayout *btnRow = new QHBoxLayout;
        QPushButton *cancelBtn = new QPushButton("放弃", dlg);
        QPushButton *okBtn = new QPushButton("提交答案", dlg);
        btnRow->addStretch();
        btnRow->addWidget(cancelBtn);
        btnRow->addWidget(okBtn);
        lay->addWidget(qLabel);
        lay->addWidget(answerEdit);
        lay->addLayout(btnRow);
        QObject::connect(okBtn, &QPushButton::clicked, dlg, &QDialog::accept);
        QObject::connect(cancelBtn, &QPushButton::clicked, dlg, &QDialog::reject);

        // 提交答案：读取输入 → 发判定请求 → 释放对话框（此时 dlg 与 answerEdit 必然存活）
        QObject::connect(dlg, &QDialog::accepted, this, [this, dlg, answerEdit, question]() {
           
            const QString userAnswer = answerEdit->toPlainText().trimmed();
            if (userAnswer.isEmpty()) {
                m_quizState = QuizState::Idle;
                m_busy = false;
                changePetState(":/pet/smile.gif", "答案空空如也，是还没想好吗？右键我再考你一道~");
                dlg->deleteLater();
                return;
            }
            // 把题目 + 同学的答案 + 资料骨架一起交给 AI 判定
            m_quizState = QuizState::Judging;
            m_lastUserAnswer = userAnswer; // 记录本次答案（答错时进错题本）
            changePetState(":/pet/think.gif", "让我看看你答得怎么样…");
            const QString judgePrompt =
                "你是严格的复习考官。以下是我刚才吃掉的复习资料：\n" + m_lastContent
                + "\n\n我出的题目：\n" + question
                + "\n\n同学的答案：\n" + userAnswer
                + "\n\n请判定对错（意思对、沾边就算对），输出固定格式，不要输出其他内容：\n"
                + "【判定】对 或 【判定】错\n【解析】不超过80字的简短讲解，夸或鼓励都可以。";
            sendAI("你是一个说话俏皮可爱的学习考官。", judgePrompt, AiAction::QuizJudge);
            dlg->deleteLater();
        });
        // 放弃作答
        QObject::connect(dlg, &QDialog::rejected, this, [this, dlg]() {
            m_quizState = QuizState::Idle;
            m_busy = false;
            changePetState(":/pet/smile.gif", "嘿嘿，答不出来没关系，右键我再考你一道~");
            dlg->deleteLater();
        });

        // 自动化测试钩子：--autoquiz 时自动填答案并提交（供脚本/演示用）
        if (QApplication::arguments().contains("--autoquiz")) {
            QTimer::singleShot(1500, dlg, [dlg, answerEdit]() {
                answerEdit->setPlainText("测试答案：二叉树中序遍历可以得到有序序列，左根右");
                dlg->accept();
            });
        }

       
        dlg->show();
        return;
    }

    if (action == AiAction::QuizJudge) {
        m_quizState = QuizState::Idle;
        m_busy = false;
        if (!ok || aiText.isEmpty()) {
            changePetState(":/pet/normal.gif", "判定时网络开小差了呜呜…你的答案我记下了，下次再判~");
            return;
        }
        // 解析【判定】对/错
        QRegularExpression reJ("【判定】\\s*(对|错|正确|错误|答对|答错|满分)");
        auto m = reJ.match(aiText);
        const bool correct = m.hasMatch() && (m.captured(1) == "对" || m.captured(1) == "正确"
                                              || m.captured(1) == "答对" || m.captured(1) == "满分");
        if (correct) {
            addStat("quizCorrect", 1); // 学习统计：答对 +1
            checkAchievements();
            changePetState(":/pet/smile.gif", "太棒啦！答对啦！\n" + aiText);
        } else {
            // 答错：记入错题本（导出学习笔记用）
            if (!m_lastQuizText.isEmpty() && !m_lastUserAnswer.isEmpty()) {
                QStringList wrong = m_settings->value("stats/wrongList").toStringList();
                wrong << QString("题目：%1\n我的答案：%2\n参考答案：%3")
                             .arg(m_lastQuizText, m_lastUserAnswer,
                                  m_lastQuizAnswer.isEmpty() ? "（未获取到标准答案）" : m_lastQuizAnswer);
                if (wrong.size() > 50) wrong = wrong.mid(wrong.size() - 50); // 最多留 50 条
                m_settings->setValue("stats/wrongList", wrong);
            }
            changePetState(":/pet/normal.gif", "答错了哦，再接再厉～\n" + aiText);
        }
    }
}

// --- 本地速览兜底：直接从骨架里取最重要的几行 ---
QString DesktopPet::localFallbackSummary(const QString &content) const {
    if (content.trimmed().isEmpty())
        return "暂时没有可用的内容…";
    QStringList lines = content.split(QRegularExpression("[\r\n]+"), Qt::SkipEmptyParts);
    QString out;
    for (const QString &line : lines) {
        QString t = line.trimmed();
        if (t.isEmpty()) continue;
        out += t;
        if (out.length() >= 140) break;
    }
    return out.left(150);
}

// --- 从候选表情里随机挑一个（避开当前） ---
QString DesktopPet::pickRandomGif(const QStringList &candidates, const QString &current) const {
    if (candidates.size() <= 1) return candidates.isEmpty() ? QString() : candidates.first();
    QString picked = current;
    int guard = 0;
    while (picked == current && guard < 10) {
        picked = candidates[QRandomGenerator::global()->bounded(candidates.size())];
        ++guard;
    }
    return picked;
}

// --- 陪伴感状态机：闲置时长分级触发（眨眼→换姿势→打瞌睡→趴桌睡觉）+ 当天不重复碎碎念 ---
void DesktopPet::onIdleTick() {
    if (m_busy || m_quizState != QuizState::Idle) return;
    if (m_summaryLock) return; // 总结成果展示中，不随机换话覆盖
    if (m_focusActive) return; // 番茄钟进行中不闲聊打扰
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    qint64 idleMs = now - m_lastInteractMs;

    // 陪伴频率缩放档位阈值：活泼更快进入闲聊档，安静更慢
    double scale = 1.0;
    if (m_companionFreq == "lively") scale = 0.6;
    else if (m_companionFreq == "calm") scale = 1.6;

    int target = 0;
    if (idleMs >= (qint64)(30000 * scale)) target = 1;  // 30s：眨眨眼
    if (idleMs >= (qint64)(120000 * scale)) target = 2; // 2min：换姿势 + 碎碎念
    if (idleMs >= (qint64)(300000 * scale)) target = 3; // 5min：打瞌睡
    if (idleMs >= (qint64)(600000 * scale)) target = 4; // 10min：趴桌睡觉
    if (target <= m_idleStage) return; // 只有档位提升才做动作

    m_idleStage = target;
    switch (target) {
    case 1: // 眨眨眼：只换表情，不说话（润物细无声）
        petMovie->stop();
        petMovie->setFileName(":/pet/eye.gif");
        petMovie->start();
        break;
    case 2: // 换姿势 + 一条今天没说过的话
        changePetState(pickRandomGif({":/pet/normal.gif", ":/pet/smile.gif"}, petMovie->fileName()),
                       pickTodayPhrase());
        break;
    case 3: // 打瞌睡
        changePetState(":/pet/think.gif", "哈欠…有点困了，你也记得歇一歇哦~");
        break;
    case 4: // 趴桌睡觉
        changePetState(":/pet/normal.gif", "zzz… 我趴一会儿，你学累了就叫我~");
        break;
    }
}

// --- 点击互动后恢复日常 ---
void DesktopPet::onTalkDone() {
    if (m_busy || m_quizState != QuizState::Idle) return;
    if (m_summaryLock) return; // 总结成果展示中，不恢复成普通气泡
    // 番茄钟进行中：恢复倒计时气泡，不打扰专注氛围
    if (m_focusActive) {
        updateFocusBubble();
        return;
    }
    changePetState(":/pet/eye.gif", "继续学习吧！记得把资料拖给我~");
}

// ===================== 番茄钟专注模式 =====================
// 每秒倒计时 + 阶段切换（专注 → 休息 → 结束）
void DesktopPet::onFocusTick() {
    if (!m_focusActive || m_focusPaused) return;
    --m_focusRemain;
    if (m_focusPhase == 0) ++m_focusTodaySeconds; // 只累计专注阶段

    if (m_focusRemain <= 0) {
        if (m_focusPhase == 0) {
            // 专注完成 → 自动进入休息
            saveFocusToday();
            m_settings->setValue("stats/firstFocusDone", true); // 成就：完成第一次番茄钟
            checkAchievements();
            m_focusPhase = 1;
            // --autofast 模式下休息 2 秒（自动化验证/演示用）
            m_focusRemain = QApplication::arguments().contains("--autofast") ? 2 : kBreakSeconds;
            changePetState(":/pet/smile.gif", "专注完成！太棒啦！休息 5 分钟，玩会小星星放松一下~");
            notifyStage("番茄钟 · 专注完成", "25 分钟专注达成！休息 5 分钟，来玩接星星放松一下~");
            launchStarGame(); // 休息时间自动弹出摸鱼小游戏
        } else {
            // 休息结束 → 番茄钟一轮完成
            m_focusActive = false;
            m_focusPaused = false;
            changePetState(":/pet/eye.gif", "休息结束！回来继续加油鸭！");
            notifyStage("番茄钟 · 休息结束", "休息结束，回来继续学习吧！");
        }
        return;
    }
    updateFocusBubble();
}

void DesktopPet::onStartFocus() {
    if (m_focusActive) return;
    m_focusActive = true;
    m_focusPaused = false;
    m_focusPhase = 0;
    // --autofast：自动化验证/演示用，专注 3 秒（正常启动无影响）
    if (QApplication::arguments().contains("--autofast")) {
        m_focusRemain = 3;
    } else {
        m_focusRemain = kFocusSeconds;
    }
    changePetState(":/pet/normal.gif", "专注开始！25 分钟倒计时，一起加油鸭！");
}

void DesktopPet::onTogglePauseFocus() {
    if (!m_focusActive) return;
    m_focusPaused = !m_focusPaused;
    if (m_focusPaused) {
        setBubbleText("番茄钟已暂停～右键我 → “继续专注”再出发！");
    } else {
        updateFocusBubble();
    }
}

void DesktopPet::onStopFocus() {
    if (!m_focusActive) return;
    saveFocusToday(); // 已经过的专注秒数计入今日累计
    m_focusActive = false;
    m_focusPaused = false;
    changePetState(":/pet/eye.gif", "专注结束啦～今天也要加油鸭！");
}

void DesktopPet::updateFocusBubble() {
    if (!m_focusActive) return;
    int r = m_focusRemain;
    QString mm = QString("%1:%2")
                     .arg(r / 60, 2, 10, QChar('0'))
                     .arg(r % 60, 2, 10, QChar('0'));
    setBubbleText(m_focusPhase == 0
        ? QString("专注中 %1 · 学完摸我一下~").arg(mm)
        : QString("休息中 %1 · 放松一下~").arg(mm));
}

void DesktopPet::saveFocusToday() {
    // 按天持久化：focus/daily/yyyyMMdd = 今日累计秒数（供近 7 天统计页使用）
    const QString today = QDateTime::currentDateTime().date().toString("yyyyMMdd");
    m_settings->setValue("focus/date", today);
    m_settings->setValue("focus/todaySeconds", m_focusTodaySeconds);
    m_settings->setValue("focus/daily/" + today, m_focusTodaySeconds);
    m_settings->sync();
}

int DesktopPet::loadFocusToday() const {
    if (m_settings->value("focus/date").toString()
            != QDateTime::currentDateTime().date().toString("yyyyMMdd")) {
        return 0; // 跨天，今日累计归零
    }
    return m_settings->value("focus/todaySeconds", 0).toInt();
}

// 右下角通知 + 提醒音
void DesktopPet::notifyStage(const QString &title, const QString &msg) {
    if (m_tray && QSystemTrayIcon::isSystemTrayAvailable())
        m_tray->showMessage(title, msg, QSystemTrayIcon::Information, 8000);
    playDing();
}

// 播放内嵌“叮咚”提醒音（解到临时文件后用系统 API 播放，无额外依赖）
void DesktopPet::playDing() {
    static QString wavPath;
    if (wavPath.isEmpty()) {
        const QString dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
        wavPath = dir + "/aipet_ding.wav";
        if (!QFile::exists(wavPath)) {
            QFile res(":/sound/ding.wav");
            if (res.open(QIODevice::ReadOnly)) {
                QFile out(wavPath);
                if (out.open(QIODevice::WriteOnly)) {
                    out.write(res.readAll());
                    out.close();
                }
                res.close();
            }
        }
    }
    PlaySoundW(reinterpret_cast<const WCHAR *>(wavPath.utf16()),
               nullptr, SND_FILENAME | SND_ASYNC);
}

// 专注统计：近 7 天柱状图
// 学习看板：专注柱状图 + 学习统计 + 成就（打开时顺带检查成就解锁）
void DesktopPet::showLearnBoard() { showStatsDialog(); }

void DesktopPet::showStatsDialog() {
    checkAchievements();
    const int docs = m_settings->value("stats/docsFed", 0).toInt();
    const int taken = m_settings->value("stats/quizTaken", 0).toInt();
    const int correct = m_settings->value("stats/quizCorrect", 0).toInt();
    const QStringList unlocked = m_settings->value("achievements/list", QString()).toString()
                                     .split(',', Qt::SkipEmptyParts);
    FocusStatsDialog *dlg = new FocusStatsDialog(m_settings, docs, taken, correct, unlocked, this);
    dlg->show();
}

// --- 学习统计计数（持久化到 [stats]） ---
void DesktopPet::addStat(const QString &key, int delta) {
    const QString full = QString("stats/%1").arg(key);
    m_settings->setValue(full, m_settings->value(full, 0).toInt() + delta);
}

// --- 成就判定：达成时解锁并气泡反馈；返回是否刚解锁 ---
bool DesktopPet::checkAchievements() {
    QStringList unlocked = m_settings->value("achievements/list", QString()).toString()
                               .split(',', Qt::SkipEmptyParts);
    QStringList news;
    auto unlock = [&](const QString &id) {
        if (!unlocked.contains(id)) { unlocked << id; news << id; }
    };

    // 初尝甜头：完成过至少一次番茄钟
    if (m_settings->value("stats/firstFocusDone", false).toBool()) unlock("first_focus");
    // 连续专注 7 天：最近 7 天每天都有专注记录
    {
        int streak = 0;
        for (int i = 0; i < 7; ++i) {
            const QString k = QDate::currentDate().addDays(-i).toString("yyyyMMdd");
            if (m_settings->value(QString("focus/daily/%1").arg(k), 0).toInt() > 0) ++streak;
            else break;
        }
        if (streak >= 7) unlock("focus_7d");
    }
    // 喂饱我啦：喂资料 ≥10 份
    if (m_settings->value("stats/docsFed", 0).toInt() >= 10) unlock("feed_10");
    // 小试牛刀：答对 ≥5 题
    if (m_settings->value("stats/quizCorrect", 0).toInt() >= 5) unlock("quiz_5");
    // 答题狂魔：抽背 ≥20 次
    if (m_settings->value("stats/quizTaken", 0).toInt() >= 20) unlock("quiz_20");

    if (!news.isEmpty()) {
        m_settings->setValue("achievements/list", unlocked.join(','));
        static const QStringList kIds = {"first_focus", "focus_7d", "feed_10", "quiz_5", "quiz_20"};
        static const QStringList kNames = {"初尝甜头", "连续专注 7 天", "喂饱我啦", "小试牛刀", "答题狂魔"};
        QStringList got;
        for (const QString &n : news) {
            const int idx = kIds.indexOf(n);
            if (idx >= 0) got << kNames[idx];
        }
        changePetState(":/pet/smile.gif", "🎉 成就达成：" + got.join("、") + "！");
        return true;
    }
    return false;
}

// --- 组装导出用的 Markdown 学习笔记 ---
QString DesktopPet::buildNotesMarkdown() {
    const int docs = m_settings->value("stats/docsFed", 0).toInt();
    const int taken = m_settings->value("stats/quizTaken", 0).toInt();
    const int correct = m_settings->value("stats/quizCorrect", 0).toInt();
    const int quizRate = taken > 0 ? qRound(correct * 100.0 / taken) : 0;
    const int todaySec = m_settings->value(
        QString("focus/daily/%1").arg(QDate::currentDate().toString("yyyyMMdd")), 0).toInt();
    const QStringList unlocked = m_settings->value("achievements/list", QString()).toString()
                                     .split(',', Qt::SkipEmptyParts);
    static const QStringList kIds = {"first_focus", "focus_7d", "feed_10", "quiz_5", "quiz_20"};
    static const QStringList kNames = {"初尝甜头", "连续专注 7 天", "喂饱我啦", "小试牛刀", "答题狂魔"};
    QStringList achGot;
    for (const QString &id : unlocked) {
        const int idx = kIds.indexOf(id);
        if (idx >= 0) achGot << kNames[idx];
    }

    QString md;
    md += "# AIPet 学习笔记\n\n";
    md += "> 导出时间：" + QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm") + "\n\n";
    md += "## 学习统计\n\n";
    md += QString("- 今日专注：%1 分钟\n").arg(todaySec / 60);
    md += QString("- 喂资料：%1 份\n").arg(docs);
    md += QString("- 突击抽背：%1 次，答对 %2 次（正确率 %3%）\n").arg(taken).arg(correct).arg(quizRate);
    md += achGot.isEmpty() ? "- 成就：暂无\n" : "- 成就：" + achGot.join("、") + "\n";
    md += "\n## 资料总结\n\n";

    // 汇总目录下所有总结（最新的在前）
    const QString base = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    QDir dir(base.isEmpty() ? QString(QDir::homePath() + "/AIPet/summaries")
                            : QString(base + "/AIPet/summaries"));
    QStringList files = dir.entryList({"AIPet_summary_*.txt"}, QDir::Files, QDir::Time | QDir::Reversed);
    if (files.isEmpty()) {
        md += "（还没有导出过总结～拖一份资料给我就有了）\n";
    } else {
        int n = 0;
        for (const QString &fn : files) {
            QFile f(dir.filePath(fn));
            if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
            QTextStream in(&f);
            in.setEncoding(QStringConverter::Utf8);
            const QString content = in.readAll().trimmed();
            f.close();
            if (content.isEmpty()) continue;
            ++n;
            md += QString("### %1\n\n%2\n\n").arg(fn.section("_", 2, 2).section(".txt", 0, 0), content);
        }
        if (n == 0) md += "（还没有导出过总结～拖一份资料给我就有了）\n";
    }

    // 错题本
    md += "## 错题本\n\n";
    const QStringList wrong = m_settings->value("stats/wrongList").toStringList();
    if (wrong.isEmpty()) {
        md += "（暂无错题，继续保持！）\n";
    } else {
        int i = 1;
        for (const QString &w : wrong) {
            md += QString("### 错题 %1\n\n%2\n\n").arg(i++).arg(w);
        }
    }
    return md;
}

// --- 一键导出学习笔记（Markdown，可直接贴进 Notion/有道云） ---
void DesktopPet::onExportNotes() {
    checkAchievements();
    const QString md = buildNotesMarkdown();
    const QString def = QDir::homePath() + "/Desktop/AIPet学习笔记_"
                        + QDate::currentDate().toString("yyyyMMdd") + ".md";
    QString path = QFileDialog::getSaveFileName(this, "导出学习笔记", def, "Markdown (*.md)");
    if (path.isEmpty()) return;
    QFile f(path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&f);
        out.setEncoding(QStringConverter::Utf8);
        out << md;
        f.close();
        setBubbleText("学习笔记已导出：" + QDir::toNativeSeparators(path));
    } else {
        setBubbleText("导出失败了呜呜…换个路径再试试？");
    }
}

// --- 切换在线 AI（DeepSeek）/ 离线 AI（Ollama） ---
void DesktopPet::onToggleAiMode(bool offline) {
    m_settings->setValue("api/mode", offline ? "offline" : "online");
    if (offline) {
        setBubbleText("已切换离线 AI（Ollama）～演示可拔网线！"
                      "先启动本地 Ollama 并拉好模型（默认 http://127.0.0.1:11434，模型 qwen2.5:7b，"
                      "可在配置文件 [api] ollamaUrl/ollamaModel 修改）");
    } else {
        setBubbleText("已切换回在线 AI（DeepSeek）～网络畅通，随时开工！");
    }
}// 摸鱼小游戏：接星星
void DesktopPet::launchStarGame() {
    StarGame *game = new StarGame();
    game->show();
    game->raise();
    game->activateWindow();
}

// --- 转身（左右镜像） ---
void DesktopPet::onTurnAround() {
    m_mirrored = !m_mirrored;
    setBubbleText(m_mirrored ? "我转过身去啦~" : "我转回来啦~");
}

// --- 右键菜单 ---
void DesktopPet::contextMenuEvent(QContextMenuEvent *event) {
    QMenu menu(this);

    QMenu *stateMenu = menu.addMenu("切换状态");
    stateMenu->addAction("日常", this, [this]() { m_summaryLock = false; changePetState(":/pet/eye.gif", "你好呀！把复习资料拖给我~"); });
    stateMenu->addAction("思考", this, [this]() { m_summaryLock = false; changePetState(":/pet/think.gif", "思考中…"); });
    stateMenu->addAction("开心", this, [this]() { m_summaryLock = false; changePetState(":/pet/smile.gif", "嘿嘿，今天也要一起加油！"); });
    stateMenu->addAction("可爱", this, [this]() { m_summaryLock = false; changePetState(":/pet/normal.gif", "咕噜咕噜~"); });

    // ---- 陪伴感打磨：碎碎念频率三档（影响闲置动作/说话的快慢）----
    QMenu *companionMenu = menu.addMenu("陪伴频率");
    auto setCompanionFreq = [this, companionMenu](const QString &key, const QString &label, int ms) {
        QAction *act = companionMenu->addAction(label);
        act->setCheckable(true);
        act->setChecked(m_companionFreq == key);
        connect(act, &QAction::triggered, this, [this, key, ms]() {
            m_companionFreq = key;
            m_settings->setValue("companion/freq", key);
            idleTimer->setInterval(ms);
            m_idleStage = 0; // 频率变更后从活跃档重新计时
            setBubbleText(QString("陪伴频率已切换为“%1”~").arg(
                key == "calm" ? "安静" : (key == "lively" ? "活泼" : "适中")));
        });
    };
    setCompanionFreq("calm", "安静（少打扰）", 12000);
    setCompanionFreq("normal", "适中（默认）", 8000);
    setCompanionFreq("lively", "活泼（常互动）", 4000);

    // ---- 番茄钟专注模式 ----
    if (m_focusActive) {
        menu.addAction(m_focusPaused ? "继续专注" : "暂停专注",
                       this, &DesktopPet::onTogglePauseFocus);
        menu.addAction("结束专注", this, &DesktopPet::onStopFocus);
    } else {
        menu.addAction("开始专注（25 分钟）", this, &DesktopPet::onStartFocus);
    }
    QAction *focusStat = menu.addAction(QString("今日专注：%1 分钟").arg(m_focusTodaySeconds / 60));
    focusStat->setEnabled(false); // 纯展示项
    menu.addAction("学习看板…", this, &DesktopPet::showStatsDialog);
    // 专注进行中不提供摸鱼入口（避免分心），休息/空闲时可玩
    if (!m_focusActive || m_focusPhase == 1)
        menu.addAction("摸鱼小游戏…", this, &DesktopPet::launchStarGame);

    menu.addAction("考考我…", this, &DesktopPet::onQuiz);
    menu.addAction("查看最近总结…", this, &DesktopPet::onViewSummary);
    menu.addAction("导出学习笔记…", this, &DesktopPet::onExportNotes);
    menu.addAction("任务清单…", this, &DesktopPet::onTodoList);
    menu.addAction("设置课表…", this, &DesktopPet::onEditSchedule);
    menu.addAction("转身", this, &DesktopPet::onTurnAround);

    QAction *topAct = menu.addAction("窗口置顶");
    topAct->setCheckable(true);
    topAct->setChecked(windowFlags() & Qt::WindowStaysOnTopHint);
    connect(topAct, &QAction::toggled, this, &DesktopPet::onToggleTopmost);

    // 开机自启动（写 HKCU Run 注册表，勾选即开机启动；随时可取消关闭）
    QSettings runKey(QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"),
                     QSettings::NativeFormat);
    QAction *autoAct = menu.addAction("开机自启动");
    autoAct->setCheckable(true);
    autoAct->setChecked(runKey.contains("AIPet1"));
    connect(autoAct, &QAction::toggled, this, &DesktopPet::onToggleAutostart);

    // AI 模式切换：在线 DeepSeek / 离线 Ollama（演示可拔网线，互斥单选）
    QMenu *aiMenu = menu.addMenu("AI 模式");
    QActionGroup *aiGroup = new QActionGroup(aiMenu);
    aiGroup->setExclusive(true);
    const bool offlineNow = m_settings->value("api/mode", "online").toString() == "offline";
    QAction *aiOnline = aiMenu->addAction("在线（DeepSeek）");
    aiOnline->setCheckable(true);
    aiOnline->setChecked(!offlineNow);
    aiGroup->addAction(aiOnline);
    connect(aiOnline, &QAction::triggered, this, [this]() { onToggleAiMode(false); });
    QAction *aiOffline = aiMenu->addAction("离线（Ollama）");
    aiOffline->setCheckable(true);
    aiOffline->setChecked(offlineNow);
    aiGroup->addAction(aiOffline);
    connect(aiOffline, &QAction::triggered, this, [this]() { onToggleAiMode(true); });

    menu.addAction("设置 API Key…", this, &DesktopPet::onSetApiKey);
    menu.addSeparator();
    menu.addAction("退出", this, &DesktopPet::onQuit);

    menu.exec(event->globalPos());
}

// --- 突击抽背：基于刚吃掉的资料出题 ---
void DesktopPet::onQuiz() {
    if (m_lastContent.trimmed().isEmpty()) {
        changePetState(":/pet/normal.gif", "我肚子里还没东西呢～先拖一份复习资料给我，再考考我！");
        return;
    }
    if (m_quizState != QuizState::Idle) {
        setBubbleText("等一等，我正在忙这道题呢…");
        return;
    }
    if (m_busy) { // 总结/其他 AI 任务进行中不重入，避免状态错乱
        setBubbleText("我还在忙呢～等我消化完手头的资料，再考考你！");
        return;
    }
    m_summaryLock = false; // 考考我会切换气泡内容，解除旧的总结锁定
    // 仅在线模式要求 API Key；离线模式（Ollama）不需要
    const bool offlineQuiz = m_settings->value("api/mode", "online").toString() == "offline";
    if (!offlineQuiz && m_settings->value("api/key", "").toString().trimmed().isEmpty()) {
        changePetState(":/pet/normal.gif", "还没配置 API Key 呢～右键我 → “设置 API Key”粘贴一下，我马上开工！");
        return;
    }
    m_quizState = QuizState::Asking;
    m_busy = true;
    changePetState(":/pet/think.gif", "让我从刚才的资料里挑个考点考考你…");
    const QString quizPrompt =
        "根据下面这份复习资料，出一道题考考同学。要求：\n"
        "1. 只出 1 道单选题或简答题，考查最核心的考点；\n"
        "2. 题目不超过100字，选项不超过4个；\n"
        "3. 必须严格输出以下格式，不要输出其他内容：\n"
        "【题目】...\n【答案】...\n【解析】...";
    sendAI("你是一个俏皮可爱的大学生学习考官。", quizPrompt + "\n\n资料内容：\n" + m_lastContent, AiAction::QuizQuestion);
}

// --- 设置 API Key（保存到本地配置文件） ---
void DesktopPet::onSetApiKey() {
    bool ok = false;
    QString key = QInputDialog::getText(
        this, "设置 API Key",
        "粘贴你的 DeepSeek API Key：",
        QLineEdit::Password,
        m_settings->value("api/key").toString(),
        &ok);
    if (ok && !key.trimmed().isEmpty()) {
        m_settings->setValue("api/key", key.trimmed());
        setBubbleText("收到！Key 已保存，把资料拖给我试试吧~");
    }
}

// --- 置顶开关 ---
void DesktopPet::onToggleTopmost(bool checked) {
    setWindowFlag(Qt::WindowStaysOnTopHint, checked);
    show();
}

// --- 退出程序 ---
void DesktopPet::onQuit() {

    QApplication::quit();
}

// ===================== 任务清单 =====================
QStringList DesktopPet::loadTasks() const {
    return m_settings->value("task/items", QStringList()).toStringList();
}

void DesktopPet::saveTasks(const QStringList &tasks) {
    m_settings->setValue("task/items", tasks);
}

void DesktopPet::onTodoList() {
    QDialog dlg(this);
    dlg.setWindowTitle("任务清单");
    dlg.resize(320, 380);

    QListWidget *list = new QListWidget(&dlg);
    QLineEdit *edit = new QLineEdit(&dlg);
    edit->setPlaceholderText("输入新任务，回车添加…");
    QPushButton *addBtn = new QPushButton("添加", &dlg);
    QPushButton *delBtn = new QPushButton("删除", &dlg);

    QHBoxLayout *row = new QHBoxLayout;
    row->addWidget(edit, 1);
    row->addWidget(addBtn);

    QHBoxLayout *row2 = new QHBoxLayout;
    row2->addStretch(1);
    row2->addWidget(delBtn);

    QVBoxLayout *lay = new QVBoxLayout(&dlg);
    lay->addWidget(list, 1);
    lay->addLayout(row);
    lay->addLayout(row2);

    // 载入
    int completed = 0;
    for (const QString &s : loadTasks()) {
        int sep = s.lastIndexOf('|');
        QListWidgetItem *it = new QListWidgetItem(s.left(sep), list);
        bool done = (s.mid(sep + 1) == "1");
        it->setFlags(it->flags() | Qt::ItemIsUserCheckable);
        it->setCheckState(done ? Qt::Checked : Qt::Unchecked);
        if (done) ++completed;
    }

    auto persist = [&]() {
        QStringList out;
        for (int i = 0; i < list->count(); ++i) {
            QListWidgetItem *it = list->item(i);
            out << it->text() + "|" + (it->checkState() == Qt::Checked ? "1" : "0");
        }
        saveTasks(out);
    };

    auto addTask = [&]() {
        QString t = edit->text().trimmed();
        if (t.isEmpty()) return;
        QListWidgetItem *it = new QListWidgetItem(t, list);
        it->setFlags(it->flags() | Qt::ItemIsUserCheckable);
        it->setCheckState(Qt::Unchecked);
        edit->clear();
        persist();
        changePetState(":/pet/smile.gif", "记下啦！学完记得打勾哦~");
    };

    connect(addBtn, &QPushButton::clicked, addTask);
    connect(edit, &QLineEdit::returnPressed, addTask);
    connect(delBtn, &QPushButton::clicked, [&]() {
        int row = list->currentRow();
        if (row >= 0) {
            delete list->takeItem(row);
            persist();
        }
    });
    // 勾选变化 → 保存 + 完成时夸奖
    connect(list, &QListWidget::itemChanged, this, [&](QListWidgetItem *it) {
        bool wasChecked = (it->checkState() == Qt::Checked);
        persist();
        if (wasChecked && !m_busy) {
            changePetState(":/pet/smile.gif", "任务完成！超棒的！奖励你摸我一下~");
        }
    });

    dlg.exec();
}

// ===================== 课表提醒 =====================
QStringList DesktopPet::loadSchedule() const {
    return m_settings->value("schedule/items", QStringList()).toStringList();
}

void DesktopPet::saveSchedule(const QStringList &items) {
    m_settings->setValue("schedule/items", items);
}

void DesktopPet::onEditSchedule() {
    bool ok = false;
    QString text = QInputDialog::getMultiLineText(
        this, "设置课表",
        "每行一条课程，格式：周几 时间 课程名 @地点\n例如：周一 8:00 高等数学 @三教101\n（到点助手会提醒你上课）",
        loadSchedule().join("\n"), &ok);
    if (ok) {
        QStringList items;
        for (const QString &line : text.split(QRegularExpression("[\r\n]+"), Qt::SkipEmptyParts)) {
            if (!line.trimmed().isEmpty()) items << line.trimmed();
        }
        saveSchedule(items);
        changePetState(":/pet/smile.gif", "课表记好啦！我会准时叫你上课的！");
    }
}

void DesktopPet::onScheduleTick() {
    QStringList items = loadSchedule();
    if (items.isEmpty()) return;
    if (m_busy || m_quizState != QuizState::Idle) return; // AI 忙时不打扰

    QDateTime now = QDateTime::currentDateTime();
    int dow = now.date().dayOfWeek(); // 1=周一 ... 7=周日
    QString nowHm = now.time().toString("H:mm");
    int nowMin = now.time().hour() * 60 + now.time().minute();

    static const QStringList weekNames = {"", "周一", "周二", "周三", "周四", "周五", "周六", "周日"};

    for (const QString &line : items) {
        // 解析星期
        int wd = 0;
        for (int i = 1; i <= 7; ++i) {
            if (line.contains(weekNames[i]) || line.contains(QString("星期") + weekNames[i].mid(1, 1))) {
                wd = i;
                break;
            }
        }
        if (wd == 0 || wd != dow) continue;
        // 解析时间 HH:MM
        QRegularExpression re("([0-2]?\\d):([0-5]\\d)");
        auto m = re.match(line);
        if (!m.hasMatch()) continue;
        int hh = m.captured(1).toInt();
        int mm = m.captured(2).toInt();
        int classMin = hh * 60 + mm;
        int diff = classMin - nowMin;
        if (diff < 0 || diff > 5) continue; // 上课前 5 分钟内提醒

        // 课程名 + 地点
        QString rest = line.mid(m.capturedStart(0)).trimmed();
        int atIdx = rest.indexOf('@');
        QString course = (atIdx >= 0 ? rest.left(atIdx) : rest).trimmed();
        QString place = atIdx >= 0 ? rest.mid(atIdx + 1).trimmed() : QString();

        // 去重：同一天同一课程只提醒一次
        QString key = now.date().toString("yyyyMMdd") + "|" + course;
        if (m_settings->value("schedule/lastRemind").toStringList().contains(key)) continue;
        QStringList last = m_settings->value("schedule/lastRemind").toStringList();
        last << key;
        if (last.size() > 50) last = last.mid(last.size() - 50);
        m_settings->setValue("schedule/lastRemind", last);

        QString msg = diff <= 1
            ? QString("上课啦！%1%2，快去教室！").arg(course, place.isEmpty() ? QString() : " @" + place)
            : QString("还有 %1 分钟上课：%2%3，准备一下~").arg(diff).arg(course, place.isEmpty() ? QString() : " @" + place);
        changePetState(":/pet/think.gif", msg);
        break;
    }
}
