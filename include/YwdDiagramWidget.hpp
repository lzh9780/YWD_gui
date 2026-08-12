#pragma once

#include <QWidget>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <QMap>
#include <QSet>
#include <deque>
#include <cstdint>

#include "ywd_protocol.h"

QT_CHARTS_USE_NAMESPACE

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QScrollArea;
class QTimer;
class QVBoxLayout;


// ============================================================================
// Per-motor snapshot data at one time point
// ============================================================================
struct MotorSample {
    float    position  = 0;
    float    velocity  = 0;
    float    torque    = 0;
    float    voltage   = 0;
    int8_t   temp_mos  = 0;
    int8_t   temp_motor = 0;
    bool     valid     = false;
};


// A snapshot = all known motors' latest state at one time instant
using MotorSnapshot = QMap<uint8_t, MotorSample>;


// ============================================================================
// Signal types available for chart display
// ============================================================================
enum YwdSignalType {
    YST_Position   = 0,
    YST_Velocity,
    YST_Torque,
    YST_Voltage,
    YST_TempMos,
    YST_TempMotor,
    YST_COUNT
};


// ============================================================================
// YwdDiagramWidget – time-series chart + motor signal checkboxes
// ============================================================================
class YwdDiagramWidget : public QWidget
{
    Q_OBJECT
public:
    explicit YwdDiagramWidget(QWidget *parent = nullptr);

    // Render a window of history
    void renderWindow(const std::deque<MotorSnapshot> &history,
                      double samplePeriodSec,
                      int historyOffsetSamples,
                      int startDequeIdx,
                      int endDequeIdx,
                      double xMin, double xMax);

    // Let the diagram discover which motors exist (from the latest snapshot)
    void updateSignalListFromSnapshot(const MotorSnapshot &latest);

    // Configure display
    void setMaxPoints(int n)  { m_maxPoints_ = n; }
    void setSamplePeriod(double t) { m_samplePeriod_ = t; }
    void setLiveMode(bool on);

    // History-mode scroll offset (in samples, 0 = newest)
    int  scrollOffset() const { return m_scrollOffset_; }

    // Clear all plotted points (keeps series/checkboxes)
    void clearPlotData();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

signals:
    void requestClose(YwdDiagramWidget *self);

private slots:
    void onSignalTypeChanged(int index);
    void onPrevClicked();
    void onNextClicked();
    void onResetZoom();

private:
    void buildUi();
    void ensureSeries(uint8_t motorId);
    void dropSeries(uint8_t motorId);
    void updateTitle();
    void onMotorToggled();

    QColor colorForMotor(uint8_t id) const;

    static const QString &signalTypeName(YwdSignalType t);
    static const QString &signalTypeUnit(YwdSignalType t);

    // ── UI controls ──────────────────────────────────────────
    QComboBox     *m_typeCombo_   = nullptr;
    QPushButton   *m_prevBtn_     = nullptr;
    QPushButton   *m_nextBtn_     = nullptr;
    QPushButton   *m_liveBtn_     = nullptr;
    QPushButton   *m_resetZoomBtn_= nullptr;
    QPushButton   *m_closeBtn_    = nullptr;
    QLabel        *m_title_       = nullptr;
    QLabel        *m_emptyLabel_  = nullptr;

    // ── Chart ─────────────────────────────────────────────────
    QChart         *m_chart_     = nullptr;
    QChartView     *m_chartView_ = nullptr;
    QValueAxis     *m_axisX_     = nullptr;
    QValueAxis     *m_axisY_     = nullptr;

    // ── Data ──────────────────────────────────────────────────
    YwdSignalType   m_signalType_  = YST_Position;
    int             m_maxPoints_   = 250;
    double          m_samplePeriod_= 0.05;
    bool            m_liveMode_    = true;

    // ── Motor <-> series mapping ──────────────────────────────
    struct MotorEntry {
        uint8_t      motorId;
        QCheckBox   *checkBox = nullptr;
        QLineSeries *series   = nullptr;
    };
    QMap<uint8_t, MotorEntry> m_motors_;
    QSet<uint8_t> m_knownMotors_;
    bool m_rebuildPending_ = false;

    // ── Scroll position in history mode ──────────────────────
    int m_scrollOffset_       = 0;    // offset in deque indices
    int m_lastScrollOffset_   = -1;   // used to detect scroll change (pause zoom gate)
};
