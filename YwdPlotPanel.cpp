#include "YwdPlotPanel.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QToolButton>
#include <QSplitter>
#include <QTimer>
#include <algorithm>


// ============================================================================
// Construction
// ============================================================================
YwdPlotPanel::YwdPlotPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(4, 4, 4, 4);
    root->setSpacing(4);

    // ── Top row: Add diagram, Update frequency, Live, Clear ─────
    auto *topRow = new QHBoxLayout;

    m_addButton_ = new QPushButton(QStringLiteral("+"), this);
    m_addButton_->setFixedWidth(28);
    m_addButton_->setToolTip(QStringLiteral("Add diagram panel"));
    connect(m_addButton_, &QPushButton::clicked,
            this, &YwdPlotPanel::onAddDiagram);
    topRow->addWidget(m_addButton_);

    auto *freqLabel = new QLabel(QStringLiteral("Update:"), this);
    m_freqCombo_ = new QComboBox(this);
    m_freqCombo_->addItem(QStringLiteral("20 ms"),  20);
    m_freqCombo_->addItem(QStringLiteral("50 ms"),  50);
    m_freqCombo_->addItem(QStringLiteral("100 ms"), 100);
    m_freqCombo_->addItem(QStringLiteral("200 ms"), 200);
    m_freqCombo_->setCurrentIndex(1);  // 50 ms default

    topRow->addSpacing(8);
    topRow->addWidget(freqLabel);
    topRow->addWidget(m_freqCombo_);

    m_liveBtn_ = new QToolButton(this);
    m_liveBtn_->setText(QStringLiteral("Pause"));
    m_liveBtn_->setCheckable(true);
    m_liveBtn_->setChecked(false);
    connect(m_liveBtn_, &QToolButton::toggled,
            this, &YwdPlotPanel::onLiveToggled);
    topRow->addSpacing(8);
    topRow->addWidget(m_liveBtn_);

    m_clearBtn_ = new QToolButton(this);
    m_clearBtn_->setText(QStringLiteral("Clear"));
    connect(m_clearBtn_, &QToolButton::clicked,
            this, &YwdPlotPanel::clearHistory);
    topRow->addSpacing(4);
    topRow->addWidget(m_clearBtn_);

    topRow->addStretch();
    root->addLayout(topRow);

    // ── Diagram area ────────────────────────────────────────────
    m_splitter_ = new QSplitter(Qt::Vertical, this);
    root->addWidget(m_splitter_, 1);

    // Wire frequency combo
    connect(m_freqCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &YwdPlotPanel::onFrequencyChanged);

    // Create first diagram
    onAddDiagram();
}

// ============================================================================
// Timer setup
// ============================================================================
void YwdPlotPanel::setUpdateTimer(QTimer *timer)
{
    m_timer_ = timer;
    if (!m_timer_ || !m_freqCombo_)
        return;

    int ms = m_freqCombo_->currentData().toInt();
    m_timer_->setInterval(ms);
    m_timer_->start();               // the timer must be running
    samplePeriod_ = ms / 1000.0;
    updateDiagramConfigs();
}

// ============================================================================
// Add / remove diagrams
// ============================================================================
void YwdPlotPanel::onAddDiagram()
{
    auto *diag = new YwdDiagramWidget(m_splitter_);
    diag->setMaxPoints(m_maxPoints_);
    diag->setSamplePeriod(samplePeriod_);

    m_splitter_->addWidget(diag);
    m_diagrams_.append(diag);

    connect(diag, &YwdDiagramWidget::requestClose,
            this, &YwdPlotPanel::onDiagramRequestClose);
}

void YwdPlotPanel::onDiagramRequestClose(YwdDiagramWidget *which)
{
    auto it = std::find(m_diagrams_.begin(), m_diagrams_.end(), which);
    if (it != m_diagrams_.end())
        m_diagrams_.erase(it);
    which->deleteLater();
}

// ============================================================================
// Frequency changed
// ============================================================================
void YwdPlotPanel::onFrequencyChanged(int)
{
    if (!m_timer_ || !m_freqCombo_)
        return;

    int ms = m_freqCombo_->currentData().toInt();
    m_timer_->setInterval(ms);
    samplePeriod_ = ms / 1000.0;
    m_maxPoints_ = static_cast<int>(5.0 / samplePeriod_);  // show ~5s window
    history_.clear();
    totalSamples_ = 0;
    updateDiagramConfigs();
}

// ============================================================================
// Push per-motor feedback data
// ============================================================================
void YwdPlotPanel::pushFeedback(const FeedbackFrame &fb)
{
    MotorSample &s = m_latest_[fb.motor_id];
    s.position   = fb.position;
    s.velocity   = fb.velocity;
    s.torque     = fb.torque;
    s.voltage    = fb.voltage;
    s.temp_mos   = fb.temp_mos;
    s.temp_motor = fb.temp_motor;
    s.valid      = true;
    m_hasNewData_ = true;
}

// ============================================================================
// Timer tick – snapshot latest state and render
// ============================================================================
void YwdPlotPanel::onTimerTick()
{
    if (!m_hasNewData_)
        return;

    // Build snapshot (copy latest state for all known motors)
    MotorSnapshot snap;
    for (auto it = m_latest_.constBegin(); it != m_latest_.constEnd(); ++it)
        snap[it.key()] = it.value();

    m_hasNewData_ = false;

    // Push into history
    if (m_liveMode_) {
        history_.push_back(snap);
        ++totalSamples_;
        if (static_cast<int>(history_.size()) > historyLimit_)
            history_.pop_front();
    }

    // Update signal lists on each diagram
    for (auto *d : m_diagrams_)
        d->updateSignalListFromSnapshot(snap);

    if (history_.empty())
        return;

    // ── Compute render window ──────────────────────────────────
    const int hSize = static_cast<int>(history_.size());
    const int lastIdx = hSize - 1;
    const int offset  = totalSamples_ - hSize;  // global index offset

    // Render all diagrams — each diagram may have its own scroll offset
    // in history mode, so the window is computed per diagram.
    for (auto *d : m_diagrams_) {
        int startIdx = 0, endIdx = lastIdx;

        if (m_liveMode_) {
            if (hSize > m_maxPoints_)
                startIdx = hSize - m_maxPoints_;
        } else {
            // History mode: honour per-diagram scroll offset (0 = newest)
            endIdx   = std::max(0, lastIdx - d->scrollOffset());
            startIdx = std::max(0, endIdx - m_maxPoints_ + 1);
        }

        const double xMin = (offset + startIdx + 1) * samplePeriod_;
        const double xMax = (offset + endIdx   + 1) * samplePeriod_;

        d->renderWindow(history_, samplePeriod_, offset,
                        startIdx, endIdx, xMin, xMax);
    }
}

// ============================================================================
// Live / history toggle
// ============================================================================
void YwdPlotPanel::onLiveToggled(bool paused)
{
    m_liveMode_ = !paused;
    m_liveBtn_->setText(paused ? QStringLiteral("Live") : QStringLiteral("Pause"));
    for (auto *d : m_diagrams_)
        d->setLiveMode(m_liveMode_);
}

// ============================================================================
// Clear all history
// ============================================================================
void YwdPlotPanel::clearHistory()
{
    history_.clear();
    totalSamples_ = 0;
    m_latest_.clear();
    m_hasNewData_ = false;

    for (auto *d : m_diagrams_)
        d->clearPlotData();
}

// ============================================================================
// Push config to all diagrams
// ============================================================================
void YwdPlotPanel::updateDiagramConfigs()
{
    for (auto *d : m_diagrams_) {
        d->setMaxPoints(m_maxPoints_);
        d->setSamplePeriod(samplePeriod_);
    }
}
