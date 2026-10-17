#include "stargame.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFrame>
#include <QRandomGenerator>
#include <QApplication>
#include <QLinearGradient>
#include <cmath>

StarGame::StarGame(QWidget *parent)
    : QWidget(parent)
{
    setWindowTitle("接星星 · 摸鱼时间");
    setFixedSize(520, 640);
    setAttribute(Qt::WA_DeleteOnClose);

    // 底部按钮区：默认隐藏，游戏结束时显示
    m_againBtn = new QPushButton("再玩一次", this);
    m_againBtn->setFixedSize(110, 38);
    m_againBtn->setStyleSheet(
        "QPushButton { background: #ff8fb3; color: white; border: none; border-radius: 19px;"
        " font-size: 15px; font-weight: bold; }"
        "QPushButton:hover { background: #ff6f9e; }");
    m_closeBtn = new QPushButton("关闭", this);
    m_closeBtn->setFixedSize(110, 38);
    m_closeBtn->setStyleSheet(
        "QPushButton { background: #cfd6e4; color: #4a3f52; border: none; border-radius: 19px;"
        " font-size: 15px; }"
        "QPushButton:hover { background: #b9c2d4; }");
    connect(m_againBtn, &QPushButton::clicked, this, &StarGame::onRestart);
    connect(m_closeBtn, &QPushButton::clicked, this, &StarGame::close);
    m_againBtn->hide();
    m_closeBtn->hide();

    // 30ms 游戏循环
    m_loopTimer = new QTimer(this);
    m_loopTimer->setInterval(30);
    connect(m_loopTimer, &QTimer::timeout, this, &StarGame::onLoop);

    // 星星生成：初始 850ms 一颗，随剩余时间变快
    m_spawnTimer = new QTimer(this);
    m_spawnTimer->setInterval(850);
    connect(m_spawnTimer, &QTimer::timeout, this, &StarGame::onSpawn);

    // 60 秒倒计时
    m_secTimer = new QTimer(this);
    m_secTimer->setInterval(1000);
    connect(m_secTimer, &QTimer::timeout, this, &StarGame::onSecond);

    restart();
}

void StarGame::restart()
{
    m_stars.clear();
    m_basketX = width() / 2.0;
    m_score = 0;
    m_miss = 0;
    m_remainSec = 60;
    m_over = false;
    m_againBtn->hide();
    m_closeBtn->hide();
    m_spawnTimer->setInterval(850);
    m_spawnTimer->start();
    m_loopTimer->start();
    m_secTimer->start();
    update();
}

void StarGame::onRestart()
{
    restart();
}

void StarGame::onSpawn()
{
    if (m_over) return;
    Star s;
    s.x = 30 + QRandomGenerator::global()->bounded(width() - 60);
    s.y = -20;
    // 随游戏推进加速（剩余时间越少、接得越多，星越快）
    s.speed = 1.6 + m_score * 0.12 + (60 - m_remainSec) * 0.02;
    if (s.speed > 6.0) s.speed = 6.0;
    m_stars.append(s);

    // 生成节奏逐渐变快（最快 450ms 一颗）
    int iv = 850 - (60 - m_remainSec) * 8 - m_score * 5;
    if (iv < 450) iv = 450;
    m_spawnTimer->setInterval(iv);
}

void StarGame::onSecond()
{
    if (m_over) return;
    --m_remainSec;
    if (m_remainSec <= 0) {
        m_remainSec = 0;
        endGame();
    }
    update();
}

void StarGame::onLoop()
{
    if (m_over) return;
    const double basketY = height() - 46.0;
    bool changed = false;
    for (int i = m_stars.size() - 1; i >= 0; --i) {
        Star &s = m_stars[i];
        s.y += s.speed;
        // 接住判定：星星中心进入篮筐区域
        if (s.y > basketY - 30 && s.y < basketY + 10
            && qAbs(s.x - m_basketX) < 52) {
            m_stars.remove(i);
            ++m_score;
            QApplication::beep(); // 接住轻响一下
            changed = true;
            continue;
        }
        // 掉出屏幕底部 = 漏掉
        if (s.y > height() + 24) {
            m_stars.remove(i);
            ++m_miss;
            changed = true;
            if (m_miss >= 10) { // 漏 10 颗提前结束
                endGame();
                return;
            }
        }
    }
    if (changed) update();
}

void StarGame::endGame()
{
    m_over = true;
    m_loopTimer->stop();
    m_spawnTimer->stop();
    m_secTimer->stop();
    m_againBtn->show();
    m_closeBtn->show();
    update();
}

void StarGame::drawStar(QPainter &p, double cx, double cy, double outer, const QColor &fill)
{
    QPainterPath path;
    const double inner = outer * 0.42;
    const int points = 10;
    for (int i = 0; i < points; ++i) {
        double ang = -M_PI / 2 + i * M_PI / 5; // 五角星 10 个顶点
        double r = (i % 2 == 0) ? outer : inner;
        double px = cx + r * cos(ang);
        double py = cy + r * sin(ang);
        if (i == 0) path.moveTo(px, py);
        else path.lineTo(px, py);
    }
    path.closeSubpath();
    p.setPen(Qt::NoPen);
    p.setBrush(fill);
    p.drawPath(path);
}

void StarGame::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // 夜空渐变背景
    QLinearGradient sky(0, 0, 0, height());
    sky.setColorAt(0.0, QColor("#1b2a5e"));
    sky.setColorAt(0.6, QColor("#4a3f7a"));
    sky.setColorAt(1.0, QColor("#ffd3e0"));
    p.fillRect(rect(), sky);

    // 点缀小星星（固定伪随机，用分数种子保持稳定感）
    for (int i = 0; i < 40; ++i) {
        double sx = (i * 137.0) - int(i * 137.0 / width()) * width();
        double sy = (i * 83.0) - int(i * 83.0 / height() * 0.7) * (height() * 0.7);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 255, 255, 90 + (i % 3) * 40));
        p.drawEllipse(QPointF(sx, sy), 1.6, 1.6);
    }

    // 顶部信息栏：剩余时间 / 得分 / 漏掉
    QFont f = p.font();
    f.setPointSize(15);
    f.setBold(true);
    p.setFont(f);
    p.setPen(QColor(255, 255, 255, 235));
    p.drawText(QRect(18, 14, 200, 30), Qt::AlignLeft | Qt::AlignVCenter,
               QString("⏱ %1s").arg(m_remainSec));
    p.drawText(QRect(width() / 2 - 100, 14, 200, 30), Qt::AlignCenter,
               QString("⭐ %1").arg(m_score));
    p.setPen(QColor(255, 200, 210, 235));
    p.drawText(QRect(width() - 158, 14, 140, 30), Qt::AlignRight | Qt::AlignVCenter,
               QString("漏掉 %1 / 10").arg(m_miss));

    // 掉落中的星星（金色 + 微微光晕）
    for (const Star &s : m_stars) {
        p.setBrush(QColor(255, 235, 140, 70));
        p.setPen(Qt::NoPen);
        p.drawEllipse(QPointF(s.x, s.y), 22, 22);
        drawStar(p, s.x, s.y, 15.0, QColor("#ffd24a"));
    }

    // 底部篮筐（粉色圆角筐 + 提手）
    const double by = height() - 46.0;
    p.setPen(QPen(QColor("#e06a95"), 3));
    p.setBrush(QColor("#ff9cbe"));
    p.drawRoundedRect(QRectF(m_basketX - 48, by - 24, 96, 30), 14, 14);
    // 筐内网格
    p.setPen(QPen(QColor(255, 255, 255, 150), 2));
    for (int i = 1; i < 3; ++i) {
        double gx = m_basketX - 48 + i * 96 / 3.0;
        p.drawLine(QPointF(gx, by - 24), QPointF(gx, by + 6));
    }
    p.drawLine(QPointF(m_basketX - 48, by - 24), QPointF(m_basketX - 34, by - 40));
    p.drawLine(QPointF(m_basketX + 48, by - 24), QPointF(m_basketX + 34, by - 40));
    p.setPen(QPen(QColor("#e06a95"), 3));
    p.setBrush(Qt::NoBrush);
    p.drawArc(QRectF(m_basketX - 34, by - 52, 68, 28), 0, 180 * 16);

    // 游戏结束面板
    if (m_over) {
        p.fillRect(rect(), QColor(0, 0, 0, 130));
        QFont big = p.font();
        big.setPointSize(26);
        big.setBold(true);
        p.setFont(big);
        p.setPen(QColor("#ffd24a"));
        p.drawText(QRect(0, height() / 2 - 90, width(), 60), Qt::AlignCenter, "摸鱼结束！");
        QFont mid = p.font();
        mid.setPointSize(17);
        p.setFont(mid);
        p.setPen(QColor(255, 255, 255, 240));
        p.drawText(QRect(0, height() / 2 - 20, width(), 40), Qt::AlignCenter,
                   QString("接住了 %1 颗星星 · 漏掉 %2 颗").arg(m_score).arg(m_miss));
        // 按钮位置
        int bw = width() / 2 - 130;
        m_againBtn->move(bw, height() / 2 + 30);
        m_closeBtn->move(bw + 150, height() / 2 + 30);
    }
}

void StarGame::mouseMoveEvent(QMouseEvent *event)
{
    double nx = event->pos().x();
    m_basketX = qBound(50.0, nx, (double)width() - 50);
    update();
}

void StarGame::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Left) m_basketX = qBound(50.0, m_basketX - 42, (double)width() - 50);
    else if (event->key() == Qt::Key_Right) m_basketX = qBound(50.0, m_basketX + 42, (double)width() - 50);
    else { QWidget::keyPressEvent(event); return; }
    update();
}
