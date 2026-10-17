#include "statsdialog.h"
#include <QSettings>
#include <QPainter>
#include <QPainterPath>
#include <QDate>
#include <QVBoxLayout>
#include <QLabel>
#include <QDateTime>
#include <QMap>
#include <QVector>
#include <QtMath>

// 成就定义（与 desktoppet.cpp 中 checkAchievements 保持一致）
static const QVector<QPair<QString, QString>> kBoardAchievements = {
    {"first_focus", "初尝甜头：完成第一次番茄钟专注"},
    {"focus_7d",    "连续专注 7 天：每天都有专注记录"},
    {"feed_10",     "喂饱我啦：累计喂给我 10 份学习资料"},
    {"quiz_5",      "小试牛刀：累计答对 5 道抽背题"},
    {"quiz_20",     "答题狂魔：累计完成 20 次突击抽背"},
};

FocusStatsDialog::FocusStatsDialog(QSettings *settings,
                                   int docsFed, int quizTaken, int quizCorrect,
                                   const QStringList &unlocked, QWidget *parent)
    : QDialog(parent)
    , m_docsFed(docsFed)
    , m_quizTaken(quizTaken)
    , m_quizCorrect(quizCorrect)
    , m_unlocked(unlocked)
{
    setWindowTitle("学习看板 · AIPet");
    setFixedSize(560, 660);
    setAttribute(Qt::WA_DeleteOnClose);

    // 读取 focus/daily 组内全部日期键（key = yyyyMMdd）
    QMap<QDate, int> all;
    settings->beginGroup("focus/daily");
    const QStringList keys = settings->childKeys();
    for (const QString &k : keys) {
        bool ok = false;
        const qint64 v = k.toLongLong(&ok);
        if (!ok || v < 20000101 || v > 20991231) continue;
        const QDate d(v / 10000, (v / 100) % 100, v % 100);
        if (d.isValid()) all.insert(d, settings->value(k, 0).toInt());
    }
    settings->endGroup();

    // 组装最近 7 天（含今天），缺失补 0
    const QDate today = QDate::currentDate();
    for (int i = 6; i >= 0; --i) {
        const QDate d = today.addDays(-i);
        DayStat st;
        st.date = d;
        st.seconds = all.value(d, 0);
        m_days.append(st);
        m_totalSeconds += st.seconds;
    }
}

void FocusStatsDialog::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), QColor("#fff7fb"));

    // 标题
    QFont tf = p.font();
    tf.setPointSize(17);
    tf.setBold(true);
    p.setFont(tf);
    p.setPen(QColor("#4a3f52"));
    p.drawText(QRect(0, 14, width(), 30), Qt::AlignCenter, "近 7 天专注时长（分钟）");

    // 图表区（上半部分）
    const int left = 64, right = width() - 20;
    const int top = 62, bottom = 300;
    const double plotW = right - left;
    const double slotW = plotW / 7.0;

    // Y 轴最大值：向上取整到 5 的倍数（至少 10 分钟）
    int maxMin = 10;
    for (const DayStat &d : m_days) maxMin = qMax(maxMin, d.seconds / 60);
    maxMin = ((maxMin + 4) / 5) * 5;

    // 网格 + Y 轴刻度
    QFont g = p.font();
    g.setPointSize(9);
    p.setFont(g);
    for (int i = 0; i <= 4; ++i) {
        const int min = maxMin * i / 4;
        const double gy = bottom - (bottom - top) * i / 4.0;
        p.setPen(QPen(QColor(233, 216, 228), 1));
        p.drawLine(QPointF(left, gy), QPointF(right, gy));
        p.setPen(QColor("#9a8ba6"));
        p.drawText(QRect(4, gy - 10, 54, 20), Qt::AlignRight | Qt::AlignVCenter,
                   QString::number(min));
    }
    // X 轴
    p.setPen(QPen(QColor("#c9a8c0"), 2));
    p.drawLine(QPointF(left, bottom), QPointF(right, bottom));

    // 柱子
    for (int i = 0; i < 7; ++i) {
        const DayStat &d = m_days[i];
        const double cx = left + slotW * (i + 0.5);
        const double bw = slotW * 0.5;
        const double minutes = d.seconds / 60.0;
        const double barH = (bottom - top) * (minutes / maxMin);
        const bool today = (d.date == QDate::currentDate());

        QLinearGradient grad(0, bottom, 0, bottom - barH);
        if (today) {
            grad.setColorAt(0, QColor("#ff8fb3"));
            grad.setColorAt(1, QColor("#ff6f9e"));
        } else {
            grad.setColorAt(0, QColor("#ffd0e0"));
            grad.setColorAt(1, QColor("#ffb3cd"));
        }
        p.setPen(Qt::NoPen);
        p.setBrush(grad);
        if (barH > 0.5)
            p.drawRoundedRect(QRectF(cx - bw / 2, bottom - barH, bw, barH), 6, 6);

        if (minutes >= 1.0) {
            QFont vf = p.font();
            vf.setPointSize(9);
            vf.setBold(true);
            p.setFont(vf);
            p.setPen(today ? QColor("#e0487d") : QColor("#b06a92"));
            p.drawText(QRectF(cx - bw / 2, bottom - barH - 20, bw, 18),
                       Qt::AlignCenter, QString::number((int)qCeil(minutes)));
        }

        QFont df = p.font();
        df.setPointSize(9);
        df.setBold(today);
        p.setFont(df);
        p.setPen(today ? QColor("#e0487d") : QColor("#7c6a80"));
        p.drawText(QRectF(cx - slotW / 2, bottom + 8, slotW, 18),
                   Qt::AlignCenter, d.date.toString("MM-dd"));
    }

    // 7 天累计
    QFont sf = p.font();
    sf.setPointSize(11);
    sf.setBold(true);
    p.setFont(sf);
    p.setPen(QColor("#7c6a80"));
    const int totalMin = m_totalSeconds / 60;
    p.drawText(QRect(0, bottom + 30, width(), 24), Qt::AlignCenter,
               QString("7 天累计 %1 小时 %2 分钟 · 平均 %3 分钟/天")
                   .arg(totalMin / 60).arg(totalMin % 60).arg(qCeil(totalMin / 7.0)));

    // ---- 学习统计 ----
    p.setPen(QColor("#4a3f52"));
    QFont h = p.font();
    h.setPointSize(13);
    h.setBold(true);
    p.setFont(h);
    p.drawText(QRect(20, 372, width() - 40, 24), Qt::AlignLeft, "学习统计");

    QFont st = p.font();
    st.setPointSize(11);
    st.setBold(false);
    p.setFont(st);
    p.setPen(QColor("#7c6a80"));
    const int quizRate = m_quizTaken > 0 ? qRound(m_quizCorrect * 100.0 / m_quizTaken) : 0;
    p.drawText(QRect(20, 400, width() - 40, 22), Qt::AlignLeft,
               QString("喂资料 %1 份    ·    抽背 %2 次    ·    答对 %3 次（正确率 %4%）")
                   .arg(m_docsFed).arg(m_quizTaken).arg(m_quizCorrect).arg(quizRate));

    // ---- 成就列表 ----
    p.setPen(QColor("#4a3f52"));
    p.setFont(h);
    p.drawText(QRect(20, 438, width() - 40, 24), Qt::AlignLeft, "成就");

    QFont a = p.font();
    a.setPointSize(10);
    p.setFont(a);
    int ay = 468;
    for (const auto &ach : kBoardAchievements) {
        const bool got = m_unlocked.contains(ach.first);
        p.setPen(got ? QColor("#e0487d") : QColor("#b9a7b0"));
        QFont af = p.font();
        af.setBold(got);
        p.setFont(af);
        p.drawText(QRect(24, ay, width() - 48, 22), Qt::AlignLeft,
                   QString("%1 %2").arg(got ? "✔" : "○").arg(ach.second));
        ay += 24;
    }
}
