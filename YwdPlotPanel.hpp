#pragma once

#include <QWidget>
#include <QVector>
#include <QMap>
#include <deque>
#include <cstdint>

#include "YwdDiagramWidget.hpp"
#include "ywd_protocol.h"

class QComboBox;
class QPushButton;
class QSplitter;
class QTimer;
class QToolButton;


// ============================================================================
// YwdPlotPanel – container for one or more YwdDiagramWidget panels
// Accumulates per-motor FeedbackFrames, builds periodic snapshots,
// manages shared history, and drives chart rendering.
// ============================================================================
class YwdPlotPanel : public QWidget
{
    Q_OBJECT
public:
    explicit YwdPlotPanel(QWidget *parent = nullptr);

    // Called by MainWindow::onFrameReceived for each incoming feedback
    void pushFeedback(const FeedbackFrame &fb);

    // Set external update timer (created/managed by MainWindow)
    void setUpdateTimer(QTimer *timer);

    // Clear all history
    void clearHistory();

public slots:
    void onTimerTick();

signals:
    void requestClose(YwdPlotPanel *self);

private slots:
    void onAddDiagram();
    void onFrequencyChanged(int index);
    void onLiveToggled(bool on);
    void onDiagramRequestClose(YwdDiagramWidget *which);

private:
    void updateDiagramConfigs();

    // ── UI ────────────────────────────────────────────────────
    QPushButton *m_addButton_  = nullptr;
    QComboBox   *m_freqCombo_  = nullptr;
    QToolButton *m_liveBtn_    = nullptr;
    QToolButton *m_clearBtn_   = nullptr;
    QSplitter   *m_splitter_   = nullptr;
    QVector<YwdDiagramWidget *> m_diagrams_;

    // ── Timer ─────────────────────────────────────────────────
    QTimer *m_timer_ = nullptr;

    // ── Data ──────────────────────────────────────────────────
    MotorSnapshot  m_latest_;           // latest state per motor
    std::deque<MotorSnapshot> history_;
    int historyLimit_    = 5000;
    int totalSamples_    = 0;
    double samplePeriod_ = 0.05;        // seconds

    int  m_maxPoints_    = 250;
    bool m_liveMode_     = true;
    bool m_hasNewData_   = false;
};
