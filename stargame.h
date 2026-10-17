#ifndef STARGAME_H
#define STARGAME_H

#include <QWidget>
#include <QTimer>
#include <QVector>

class QPushButton;

// 摸鱼小游戏：接住掉落的小星星（专注休息时放松用）
class StarGame : public QWidget
{
    Q_OBJECT
public:
    explicit StarGame(QWidget *parent = nullptr);
    void restart(); // 开始 / 重新开始一局

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void onLoop();    // 30ms 游戏循环
    void onSpawn();   // 定时生成星星
    void onSecond();  // 1s 倒计时
    void onRestart(); // 再玩一次

private:
    struct Star { double x = 0, y = 0, speed = 2.0; };
    QVector<Star> m_stars;
    double m_basketX = 260.0;   // 篮筐中心 x
    int m_score = 0;            // 接住数
    int m_miss = 0;             // 漏掉数
    int m_remainSec = 60;       // 剩余秒数
    bool m_over = false;        // 是否结束
    QTimer *m_loopTimer;
    QTimer *m_spawnTimer;
    QTimer *m_secTimer;
    QPushButton *m_againBtn = nullptr;
    QPushButton *m_closeBtn = nullptr;

    void drawStar(QPainter &p, double cx, double cy, double outer, const QColor &fill);
    void endGame();
};

#endif // STARGAME_H
