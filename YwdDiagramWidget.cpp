#include "YwdDiagramWidget.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QEvent>
#include <math.h>

// ============================================================================
// Static helpers
// ============================================================================
const QString &YwdDiagramWidget::signalTypeName(YwdSignalType t)
{
    static const QString names[] = {
        QStringLiteral("Position"),  QStringLiteral("Velocity"),
        QStringLiteral("Torque"),    QStringLiteral("Voltage"),
        QStringLiteral("MOS Temp"),  QStringLiteral("Motor Temp")
    };
    return names[static_cast<int>(t)];
}

const QString &YwdDiagramWidget::signalTypeUnit(YwdSignalType t)
{
    static const QString units[] = {
        QStringLiteral("rad"),        QStringLiteral("rad/s"),
        QStringLiteral("N\u00B7m"),   QStringLiteral("V"),
        QStringLiteral("\u2103"),     QStringLiteral("\u2103")
    };
    return units[static_cast<int>(t)];
}

// ============================================================================
// Construction
// ============================================================================
YwdDiagramWidget::YwdDiagramWidget(QWidget *parent)
    : QWidget(parent)
{
    buildUi();
}

// ============================================================================
// UI construction
// ============================================================================
void YwdDiagramWidget::buildUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(2, 2, 2, 2);
    root->setSpacing(2);

    // Top bar
    auto *topRow = new QHBoxLayout;
    topRow->setSpacing(4);

    m_typeCombo_ = new QComboBox(this);
    for (int i = 0; i < YST_COUNT; ++i) {
        auto t = static_cast<YwdSignalType>(i);
        m_typeCombo_->addItem(signalTypeName(t) + " (" + signalTypeUnit(t) + ")", i);
    }
    connect(m_typeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &YwdDiagramWidget::onSignalTypeChanged);
    topRow->addWidget(m_typeCombo_);

    topRow->addSpacing(6);

    m_prevBtn_ = new QPushButton(QStringLiteral("\u25C0"), this);
    m_prevBtn_->setFixedWidth(28);
    m_prevBtn_->setToolTip(QStringLiteral("Scroll back"));
    m_prevBtn_->setVisible(false);
    connect(m_prevBtn_, &QPushButton::clicked,
            this, &YwdDiagramWidget::onPrevClicked);
    topRow->addWidget(m_prevBtn_);

    m_nextBtn_ = new QPushButton(QStringLiteral("\u25B6"), this);
    m_nextBtn_->setFixedWidth(28);
    m_nextBtn_->setToolTip(QStringLiteral("Scroll forward"));
    m_nextBtn_->setVisible(false);
    connect(m_nextBtn_, &QPushButton::clicked,
            this, &YwdDiagramWidget::onNextClicked);
    topRow->addWidget(m_nextBtn_);


    m_resetZoomBtn_ = new QPushButton(QStringLiteral("Res.Zoom"), this);
    m_resetZoomBtn_->setFixedWidth(64);
    connect(m_resetZoomBtn_, &QPushButton::clicked,
            this, &YwdDiagramWidget::onResetZoom);
    topRow->addWidget(m_resetZoomBtn_);

    topRow->addStretch();

    for (uint8_t mId = 1; mId <= 3; mId ++)
    {
        m_motors_[mId].motorId = mId;
        m_motors_[mId].checkBox = new QCheckBox(
            QStringLiteral("Motor %1").arg(m_motors_[mId].motorId, 2, 16, QChar('0')));
        m_motors_[mId].checkBox->setChecked(false);
        connect(m_motors_[mId].checkBox, &QCheckBox::toggled,
                this, &YwdDiagramWidget::onMotorToggled);
        topRow->addWidget(m_motors_[mId].checkBox);
    }

    m_title_ = new QLabel(QStringLiteral("  "), this);
    m_title_->setStyleSheet("font-weight:bold; color:#00b4ff;");
    topRow->addWidget(m_title_);

    m_closeBtn_ = new QPushButton(QString::fromUtf8("\u00D7"), this);
    m_closeBtn_->setFixedSize(20, 20);
    connect(m_closeBtn_, &QPushButton::clicked, this, [this]() {
        emit requestClose(this);
    });
    topRow->addWidget(m_closeBtn_);

    root->addLayout(topRow);

    // Chart — styled to match the application-wide dark theme
    m_chart_ = new QChart;
    m_chart_->setAnimationOptions(QChart::NoAnimation);
    m_chart_->legend()->setVisible(true);
    m_chart_->legend()->setAlignment(Qt::AlignTop);
    m_chart_->legend()->setBackgroundVisible(false);
    m_chart_->legend()->setLabelColor(QColor("#dddddd"));
    m_chart_->setBackgroundBrush(QBrush(QColor("#232323")));
    m_chart_->setPlotAreaBackgroundBrush(QBrush(QColor("#1b1b1b")));
    m_chart_->setPlotAreaBackgroundVisible(true);
    m_chart_->setMargins(QMargins(0, 0, 0, 0));
    m_chart_->setContentsMargins(-8, -8, -8, -8);

    auto styleAxis = [](QValueAxis *ax) {
        ax->setLabelsColor(QColor("#cccccc"));
        ax->setTitleBrush(QBrush(QColor("#cccccc")));
        ax->setGridLineColor(QColor("#3a3a3a"));
        ax->setLinePenColor(QColor("#666666"));
    };

    m_axisX_ = new QValueAxis;
    m_axisX_->setTitleText(QStringLiteral("Time (s)"));
    m_axisX_->setLabelFormat("%.1f");
    m_axisX_->setRange(0, 1);
    m_axisX_->setTickType(QValueAxis::TicksDynamic);
    m_axisX_->setMinorTickCount(0);
    m_axisX_->setTickInterval(1.0);
    styleAxis(m_axisX_);
    m_chart_->addAxis(m_axisX_, Qt::AlignBottom);

    m_axisY_ = new QValueAxis;
    m_axisY_->setTitleText(
        signalTypeName(YST_Position) + " (" + signalTypeUnit(YST_Position) + ")");
    m_axisY_->setLabelFormat("%.3f");
    m_axisY_->setRange(-1, 1);
    styleAxis(m_axisY_);
    m_chart_->addAxis(m_axisY_, Qt::AlignLeft);

    m_chartView_ = new QChartView(m_chart_, this);
    m_chartView_->setRenderHint(QPainter::Antialiasing, true);
    m_chartView_->setRubberBand(QChartView::RectangleRubberBand);
    root->addWidget(m_chartView_, 1);

    // Placeholder / empty state
    m_emptyLabel_ = new QLabel(QStringLiteral("No motor data yet.  Waiting for feedback frames..."),
                                this);
    m_emptyLabel_->setAlignment(Qt::AlignCenter);
    m_emptyLabel_->setStyleSheet("color:#888; font-size:14px;");
    // Overlay the empty label on top of the chart view
    m_emptyLabel_->setParent(m_chartView_);
    m_emptyLabel_->setGeometry(0, 0, m_chartView_->width(), m_chartView_->height());
    // Keep the overlay in sync when the chart view is resized
    m_chartView_->installEventFilter(this);
}

// ============================================================================
// Keep the empty-state overlay fitted to the chart view
// ============================================================================
bool YwdDiagramWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_chartView_ && event->type() == QEvent::Resize
        && m_emptyLabel_) {
        m_emptyLabel_->setGeometry(m_chartView_->rect());
    }
    return QWidget::eventFilter(watched, event);
}

// ============================================================================
// Clear plotted data (series points) without touching the UI structure
// ============================================================================
void YwdDiagramWidget::clearPlotData()
{
    for (auto it = m_motors_.begin(); it != m_motors_.end(); ++it) {
        if (it->series)
            it->series->clear();
    }
    m_emptyLabel_->setVisible(m_knownMotors_.isEmpty());
}

// ============================================================================
// Update signal list from latest motor data
// ============================================================================
void YwdDiagramWidget::updateSignalListFromSnapshot(const MotorSnapshot &latest)
{
    QSet<uint8_t> now;
    for (auto it = latest.begin(); it != latest.end(); ++it) {
        if (it.value().valid)
            now.insert(it.key());
    }

    if (now == m_knownMotors_)
        return;

    m_knownMotors_ = now;
    m_rebuildPending_ = true;
}

// ============================================================================
// Individual motor toggle
// ============================================================================
void YwdDiagramWidget::onMotorToggled()
{
    for (auto it = m_motors_.begin(); it != m_motors_.end(); ++it) {
        if (!it->checkBox) continue;
        if (it->checkBox->isChecked())
            ensureSeries(it->motorId);
        else
            dropSeries(it->motorId);
    }
    updateTitle();
}

// ============================================================================
// Signal type changed
// ============================================================================
void YwdDiagramWidget::onSignalTypeChanged(int index)
{
    m_signalType_ = static_cast<YwdSignalType>(index);
    m_axisY_->setTitleText(
        signalTypeName(m_signalType_) + " (" + signalTypeUnit(m_signalType_) + ")");
    // Drop all series – they will be rebuilt with correct colour on next render
    for (auto it = m_motors_.begin(); it != m_motors_.end(); ++it) {
        if (it->series) {
            m_chart_->removeSeries(it->series);
            delete it->series;
            it->series = nullptr;
        }
    }
    // Re-create series for checked motors
    for (auto it = m_motors_.begin(); it != m_motors_.end(); ++it) {
        if (it->checkBox && it->checkBox->isChecked())
            ensureSeries(it->motorId);
    }
    updateTitle();
}

// ============================================================================
// Title update
// ============================================================================
void YwdDiagramWidget::updateTitle()
{
    int active = 0, total = 0;
    for (auto it = m_motors_.begin(); it != m_motors_.end(); ++it) {
        ++total;
        if (it->checkBox && it->checkBox->isChecked())
            ++active;
    }

    m_title_->setText(QStringLiteral("Motors %1/%2 | %3")
                          .arg(active)
                          .arg(total)
                          .arg(signalTypeName(m_signalType_)));
}

// ============================================================================
// Series management
// ============================================================================
void YwdDiagramWidget::ensureSeries(uint8_t motorId)
{
    auto it = m_motors_.find(motorId);
    if (it == m_motors_.end())
        return;

    if (it->series)
        return;  // already exists

    auto *s = new QLineSeries;
    s->setName(QStringLiteral("M%1").arg(motorId, 2, 16, QChar('0')));
    QColor c = colorForMotor(motorId);
    QPen pen(c, 1.5);
    s->setPen(pen);
    m_chart_->addSeries(s);
    s->attachAxis(m_axisX_);
    s->attachAxis(m_axisY_);

    it->series = s;
}

void YwdDiagramWidget::dropSeries(uint8_t motorId)
{
    auto it = m_motors_.find(motorId);
    if (it == m_motors_.end() || !it->series)
        return;

    m_chart_->removeSeries(it->series);
    delete it->series;
    it->series = nullptr;
}

// ============================================================================
// Color mapping
// ============================================================================
QColor YwdDiagramWidget::colorForMotor(uint8_t id) const
{
    static const QColor palette[] = {
        QColor("#e6194b"), QColor("#3cb44b"), QColor("#ffe119"),
        QColor("#4363d8"), QColor("#f58231"), QColor("#911eb4"),
        QColor("#42d4f4"), QColor("#f032e6"), QColor("#bfef45"),
        QColor("#fabebe"), QColor("#469990"), QColor("#e6beff"),
        QColor("#9a6324"), QColor("#fffac8"), QColor("#800000"),
        QColor("#aaffc3"),
    };
    constexpr int N = sizeof(palette) / sizeof(palette[0]);
    return palette[static_cast<int>(id) % N];
}

// ============================================================================
// renderWindow – draw one pass over the history window
// ============================================================================
static float extractValue(const MotorSample &s, YwdSignalType t)
{
    switch (t) {
    case YST_Position:   return s.position;
    case YST_Velocity:   return s.velocity;
    case YST_Torque:     return s.torque;
    case YST_Voltage:    return s.voltage;
    case YST_TempMos:    return static_cast<float>(s.temp_mos);
    case YST_TempMotor:  return static_cast<float>(s.temp_motor);
    default:             return 0;
    }
}

void YwdDiagramWidget::renderWindow(const std::deque<MotorSnapshot> &history,
                                     double samplePeriodSec,
                                     int historyOffsetSamples,
                                     int startDequeIdx,
                                     int endDequeIdx,
                                     double xMin, double xMax)
{

    const int count = endDequeIdx - startDequeIdx + 1;
    if (count <= 0) return;

    m_emptyLabel_->setVisible(false);

    // Bounds
    double yMinVal =  1e15;
    double yMaxVal = -1e15;

    // Update each motor series
    for (auto motorIt = m_motors_.begin(); motorIt != m_motors_.end(); ++motorIt) {
        if (!motorIt->series)
            continue;

        QVector<QPointF> pts;
        pts.reserve(count);

        uint8_t mid = motorIt->motorId;
        for (int di = startDequeIdx; di <= endDequeIdx; ++di) {
            int gi = historyOffsetSamples + di;  // global idx, 0-based
            const MotorSnapshot &snap = history[static_cast<size_t>(di)];
            auto sIt = snap.find(mid);
            if (sIt == snap.end() || !sIt->valid)
                continue;
            float val = extractValue(sIt.value(), m_signalType_);
            double t = (gi + 1) * samplePeriodSec;   // 1-based
            pts.append(QPointF(t, val));

            if (val < yMinVal) yMinVal = val * 1.2;
            if (val > yMaxVal) yMaxVal = val * 1.2;
        }

        motorIt->series->replace(pts);
    }

    // X axis — ensure visible window even for single point
    double xSpan = xMax - xMin;
    if (xSpan < 1e-6) {
        double pad = std::max(0.5, samplePeriodSec * 2.0);
        m_axisX_->setRange(xMin - pad, xMax + pad);
    } else {
        m_axisX_->setRange(xMin, xMax);
    }

    // Y axis — add 10% margin
    if (yMinVal < yMaxVal) {
        double ym = (yMaxVal - yMinVal) * 0.1;
        if (ym < 1e-6) ym = 0.5;
        m_axisY_->setRange(yMinVal - ym, yMaxVal + ym);
    } else {
        m_axisY_->setRange(yMinVal - 0.5, yMinVal + 0.5);
    }

    // Live / history button states
    if (m_liveMode_) m_scrollOffset_ = 0;
    m_prevBtn_->setVisible(!m_liveMode_);
    m_nextBtn_->setVisible(!m_liveMode_);

    updateTitle();
}

// ============================================================================
// Scroll / live mode
// ============================================================================
void YwdDiagramWidget::onPrevClicked()
{
    m_scrollOffset_ += 20;
}

void YwdDiagramWidget::onNextClicked()
{
    m_scrollOffset_ = std::max(0, m_scrollOffset_ - 20);
}

void YwdDiagramWidget::onResetZoom()
{
    m_chart_->zoomReset();
}
