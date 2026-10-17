#ifndef STATSDIALOG_H
#define STATSDIALOG_H

#include <QDialog>
#include <QDate>
#include <QVector>
#include <QStringList>

class QSettings;
class QLabel;

// 学习看板：近 7 天专注柱状图（自绘）+ 学习统计 + 成就列表
class FocusStatsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit FocusStatsDialog(QSettings *settings,
                              int docsFed, int quizTaken, int quizCorrect,
                              const QStringList &unlocked,
                              QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    struct DayStat {
        QDate date;
        int seconds = 0;
    };
    QVector<DayStat> m_days; // 最近 7 天（含今天）
    int m_totalSeconds = 0;

    int m_docsFed = 0;      // 喂资料份数
    int m_quizTaken = 0;    // 抽背次数
    int m_quizCorrect = 0;  // 抽背答对数
    QStringList m_unlocked; // 已解锁成就 id
};

#endif // STATSDIALOG_H
