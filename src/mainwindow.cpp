#include "mainwindow.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QMessageBox>
#include <QFileDialog>
#include <QDateTime>
#include <QStatusBar>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QHeaderView>
#include <QScrollBar>
#include <QStringList>
#include <cstring>
#include <cmath>
#include <climits>

// ============================================================================
// Helpers
// ============================================================================
// Render a list of node IDs as "0x01, 0x02, ..." (used by the node scan log).
static QString formatNodeList(const QVector<uint8_t> &nodes)
{
    QStringList parts;
    parts.reserve(nodes.size());
    for (uint8_t n : nodes)
        parts << QString("0x%1").arg(n, 2, 16, QChar('0')).toUpper();
    return parts.join(", ");
}

QString MainWindow::hexStr(uint32_t val, int digits)
{
    return QString("%1").arg(val, digits, 16, QChar('0')).toUpper();
}

QLineEdit *MainWindow::makeLineEdit(const QString &def, int width)
{
    auto *le = new QLineEdit(def);
    le->setFixedWidth(width);
    le->setAlignment(Qt::AlignRight);
    le->setFont(QFont("monospace", 9));
    le->setMaxLength(16);
    le->setValidator(new QRegularExpressionValidator(
        QRegularExpression("[+-]?[0-9]*\\.?[0-9]*([eE][+-]?[0-9]+)?"), le));
    le->setToolTip("Decimal value (e.g. 12.5, -0.25, 1e-3)");
    return le;
}

bool MainWindow::parseLineEdit(QLineEdit *le, double &out)
{
    const QString s = le->text().trimmed();
    if (s.isEmpty())
        return false;
    bool ok = false;
    out = s.toDouble(&ok);
    return ok;
}

double MainWindow::lineEditValue(QLineEdit *le, double fallback)
{
    double v;
    return parseLineEdit(le, v) ? v : fallback;
}

bool MainWindow::validateSendInputs(QLineEdit *const edits[], int n, int motorIdx) const
{
    for (int i = 0; i < n; ++i) {
        double v;
        if (!parseLineEdit(edits[i], v)) {
            QMessageBox::warning(
                const_cast<MainWindow *>(this), "Invalid Input",
                QString("Motor %1: all parameter fields must be valid decimal numbers "
                        "(field \"%2\" is empty or invalid).")
                    .arg(motorIdx + 1)
                    .arg(edits[i]->toolTip()));
            return false;
        }
    }
    return true;
}

QWidget *MainWindow::makeLineRow(const QString &label,
                                 QLineEdit *const *edits, int n)
{
    auto *w = new QWidget();
    auto *h = new QHBoxLayout(w);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(3);
    auto *lbl = new QLabel(label);
    lbl->setFixedWidth(90);
    lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    h->addWidget(lbl);
    for (int i = 0; i < n; ++i)
        h->addWidget(edits[i]);
    h->addStretch();
    return w;
}

// ============================================================================
// Constructor / destructor
// ============================================================================
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_device(new CanFdDevice(this))
    , m_connected(false)
    , m_pollTimer(new QTimer(this))
    , m_fbValid(false)
    , m_diagramTimer_(new QTimer(this))
    , m_mitTimer(new QTimer(this))
    , m_pvTimer(new QTimer(this))
    , m_cvTimer(new QTimer(this))
    , m_regBatchIdx(0)
    , m_batchReadTimer(new QTimer(this))
{
    setWindowTitle("YWD Smart Motor CAN-FD Control");
    resize(1280, 850);

    setupUi();

    // Status bar: RX statistics
    m_lblRxStats = new QLabel("RX: 0  |  Feedback: 0.0 Hz", this);
    m_lblRxStats->setStyleSheet("color: #888; font-family: monospace;");
    statusBar()->addPermanentWidget(m_lblRxStats);

    // Device callbacks
    connect(m_device, &CanFdDevice::frameReceived,
            this,     &MainWindow::onFrameReceived);

    // Poll timer (50 ms) for stats refresh
    connect(m_pollTimer, &QTimer::timeout, this, &MainWindow::onPollTimer);
    m_pollTimer->start(50);

    // Per-tab periodic send timers
    connect(m_mitTimer, &QTimer::timeout, this, &MainWindow::onMitTick);
    connect(m_pvTimer,  &QTimer::timeout, this, &MainWindow::onPvTick);
    connect(m_cvTimer,  &QTimer::timeout, this, &MainWindow::onCvTick);

    // Clear per-motor register value cache
    memset(m_regValues, 0, sizeof(m_regValues));
    memset(m_regValid,  0, sizeof(m_regValid));
    memset(m_regRstat,  0, sizeof(m_regRstat));

    // Batch register read timer
    connect(m_batchReadTimer, &QTimer::timeout, this, &MainWindow::onBatchReadNext);

    // Load full register catalogue
    m_allRegs = YwdProtocol::getAllRegisters();

    // Populate each motor page's register table — 4 cols: Addr|Name|Value|RSTAT
    for (int mi = 0; mi < kMaxMotors; ++mi) {
        m_regTables[mi]->setRowCount(m_allRegs.size());
        for (int i = 0; i < m_allRegs.size(); ++i) {
            const RegInfo &r = m_allRegs[i];
            m_regTables[mi]->setItem(i, 0, new QTableWidgetItem(QString("0x%1").arg(r.addr, 2, 16, QChar('0'))));
            m_regTables[mi]->setItem(i, 1, new QTableWidgetItem(r.name));
            m_regTables[mi]->setItem(i, 2, new QTableWidgetItem("—"));
            m_regTables[mi]->setItem(i, 3, new QTableWidgetItem("—"));
        }
        m_regTables[mi]->resizeColumnsToContents();
    }
}

// ============================================================================
// UI construction
// ============================================================================
void MainWindow::setupUi()
{
    QWidget *central = new QWidget(this);
    setCentralWidget(central);

    QVBoxLayout *mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(4);

    // ---- Row 0: Device connection (compact) ----
    QHBoxLayout *devRow = new QHBoxLayout();
    devRow->addWidget(new QLabel("Device:"));
    m_cmbDevice = new QComboBox();
    m_cmbDevice->setMinimumWidth(150);
    m_cmbDevice->addItem("ZCAN1");
    devRow->addWidget(m_cmbDevice, 1);
    m_lblDevStatus = new QLabel("Disconnected");
    m_lblDevStatus->setStyleSheet("color: #888;");
    devRow->addWidget(m_lblDevStatus);
    m_btnConnect = new QPushButton("Connect");
    m_btnConnect->setFixedWidth(90);
    connect(m_btnConnect, &QPushButton::clicked, this, &MainWindow::onDeviceToggle);
    devRow->addWidget(m_btnConnect);
    m_btnOta = new QPushButton("OTA");
    m_btnOta->setFixedWidth(70);
    m_btnOta->setToolTip("Open the firmware upgrade (OTA) window");
    connect(m_btnOta, &QPushButton::clicked, this, &MainWindow::onOpenOtaWindow);
    devRow->addWidget(m_btnOta);
    mainLayout->addLayout(devRow);

    // ---- Row 1: Two-column splitter (left controls | right diagrams) ----
    QSplitter *hSplit = new QSplitter(Qt::Horizontal);

    // ============ LEFT PANEL ============
    QWidget   *leftPanel = new QWidget();
    QVBoxLayout *leftV = new QVBoxLayout(leftPanel);
    leftV->setContentsMargins(0, 0, 0, 0);
    leftV->setSpacing(3);

    // -- Command tabs --
    m_cmdTabs = new QTabWidget();
    m_cmdTabs->setTabPosition(QTabWidget::North);
    m_cmdTabs->setDocumentMode(true);

    buildMitTab();
    buildPosVelTab();
    buildConstVelTab();
    buildSystemTab();
    setupModeCheckboxExclusion();

    leftV->addWidget(m_cmdTabs);

    // -- Register access --
    {
        QGroupBox *grp = new QGroupBox("Register Access");
        QVBoxLayout *vl = new QVBoxLayout(grp);
        vl->setContentsMargins(4, 2, 4, 2);
        vl->setSpacing(2);

        // Row 1: Single register read/write controls
        QHBoxLayout *row1 = new QHBoxLayout();
        row1->addWidget(new QLabel("Motor:"));
        m_regMotorId = new QSpinBox();
        m_regMotorId->setRange(0, 0x7F);
        m_regMotorId->setPrefix("0x");
        m_regMotorId->setDisplayIntegerBase(16);
        m_regMotorId->setValue(0x01);
        m_regMotorId->setFixedWidth(70);
        row1->addWidget(m_regMotorId);
        row1->addWidget(new QLabel("Addr:"));
        m_spinRegAddr = new QSpinBox();
        m_spinRegAddr->setRange(0, 0xFF);
        m_spinRegAddr->setPrefix("0x");
        m_spinRegAddr->setDisplayIntegerBase(16);
        m_spinRegAddr->setValue(0x11);
        m_spinRegAddr->setFixedWidth(80);
        row1->addWidget(m_spinRegAddr);
        row1->addStretch();
        vl->addLayout(row1);

        QHBoxLayout *row2 = new QHBoxLayout();
        row2->addWidget(new QLabel("Value:"));
        m_editRegValue = new QLineEdit("0.0000");
        m_editRegValue->setFont(QFont("monospace", 10));
        m_editRegValue->setMaxLength(16);
        m_editRegValue->setMinimumWidth(120);
        m_editRegValue->setValidator(new QRegularExpressionValidator(
            QRegularExpression("[+-]?[0-9]*\\.?[0-9]*([eE][+-]?[0-9]+)?"),
            m_editRegValue));
        m_editRegValue->setToolTip(
            "Decimal value matching the register data type:\n"
            "float32 → float (e.g. 12.5)\n"
            "uint32 / int32 → integer (e.g. 42)");
        connect(m_editRegValue, &QLineEdit::returnPressed, this, &MainWindow::onRegWrite);
        row2->addWidget(m_editRegValue, 1);
        m_btnRegRead  = new QPushButton("Read");
        m_btnRegWrite = new QPushButton("Write");
        connect(m_btnRegRead,  &QPushButton::clicked, this, &MainWindow::onRegRead);
        connect(m_btnRegWrite, &QPushButton::clicked, this, &MainWindow::onRegWrite);
        row2->addWidget(m_btnRegRead);
        row2->addWidget(m_btnRegWrite);
        vl->addLayout(row2);

        // Row 3: Read result label
        QHBoxLayout *row3 = new QHBoxLayout();
        m_lblRegResult = new QLabel("");
        m_lblRegResult->setStyleSheet("color: green; font-family: monospace;");
        row3->addWidget(m_lblRegResult, 1);
        vl->addLayout(row3);

        // Motor pages: each motor has its own table + Read All button
        m_regMotorTabs = new QTabWidget();
        m_regMotorTabs->setDocumentMode(true);
        for (int mi = 0; mi < kMaxMotors; ++mi) {
            QWidget *page = new QWidget();
            QVBoxLayout *pl = new QVBoxLayout(page);
            pl->setContentsMargins(2, 2, 2, 2);
            pl->setSpacing(2);

            // Table: Addr | Name | Value | RSTAT
            m_regTables[mi] = new QTableWidget(0, 4);
            m_regTables[mi]->setHorizontalHeaderLabels({"Addr", "Name", "Value", "RSTAT"});
            m_regTables[mi]->setSelectionBehavior(QAbstractItemView::SelectRows);
            m_regTables[mi]->setEditTriggers(QAbstractItemView::NoEditTriggers);
            m_regTables[mi]->verticalHeader()->setVisible(false);
            // m_regTables[mi]->setMaximumHeight(300);
            m_regTables[mi]->horizontalHeader()->setStretchLastSection(true);
            pl->addWidget(m_regTables[mi]);

            // Bottom row: Read All button + status
            QHBoxLayout *br = new QHBoxLayout();
            m_btnReadAllMotor[mi] = new QPushButton("Read All");
            m_btnReadAllMotor[mi]->setFixedWidth(80);
            // Bind to the page index; the actual node ID is resolved at click
            // time via the scan-derived mapping.
            connect(m_btnReadAllMotor[mi], &QPushButton::clicked, this,
                    [this, mi]{ onReadAllRegs(static_cast<uint8_t>(mi + 1)); });
            br->addWidget(m_btnReadAllMotor[mi]);
            m_lblReadAllMotor[mi] = new QLabel("");
            m_lblReadAllMotor[mi]->setStyleSheet("color: #aaa; font-family: monospace;");
            br->addWidget(m_lblReadAllMotor[mi], 1);
            pl->addLayout(br);

            m_regMotorTabs->addTab(page, QString("Motor %1").arg(mi + 1));
        }
        vl->addWidget(m_regMotorTabs);

        leftV->addWidget(grp);
    }

    leftV->addStretch();
    hSplit->addWidget(leftPanel);

    // ============ RIGHT PANEL: Diagrams ============
    m_plotPanel_ = new YwdPlotPanel();
    m_plotPanel_->setUpdateTimer(m_diagramTimer_);
    connect(m_diagramTimer_, &QTimer::timeout, m_plotPanel_, &YwdPlotPanel::onTimerTick);
    hSplit->addWidget(m_plotPanel_);

    hSplit->setStretchFactor(0, 0);
    hSplit->setStretchFactor(1, 1);
    hSplit->setSizes({380, 900});

    mainLayout->addWidget(hSplit, 1);

    // ---- Row 2: Feedback / Log (switchable bottom panel) ----
    m_bottomTabs = new QTabWidget();
    m_bottomTabs->setDocumentMode(true);

    // -- Tab 1: Motor Feedback table --
    {
        QWidget *fbPage = new QWidget();
        QVBoxLayout *vl = new QVBoxLayout(fbPage);
        vl->setContentsMargins(2, 2, 2, 2);
        m_feedbackTable = new QTableWidget(0, 12);
        m_feedbackTable->setHorizontalHeaderLabels(
            {"Motor", "State", "Fault", "Mode", "Position", "Velocity",
             "Torque", "Voltage", "MOS T", "Mot T", "Recs", "SEQ"});
        m_feedbackTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_feedbackTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_feedbackTable->verticalHeader()->setVisible(false);
        m_feedbackTable->horizontalHeader()->setStretchLastSection(true);
        vl->addWidget(m_feedbackTable);

        QHBoxLayout *fbBtnRow = new QHBoxLayout();
        fbBtnRow->addStretch();
        QPushButton *btnClearFb = new QPushButton("Clear Feedback");
        btnClearFb->setFixedWidth(120);
        connect(btnClearFb, &QPushButton::clicked, this, [this] {
            m_feedbackTable->setRowCount(0);
            m_lastFeedback.clear();
        });
        fbBtnRow->addWidget(btnClearFb);
        vl->addLayout(fbBtnRow);

        m_bottomTabs->addTab(fbPage, "Feedback");
    }

    // -- Tab 2: Communication Log --
    {
        QWidget *logPage = new QWidget();
        QVBoxLayout *logLay = new QVBoxLayout(logPage);
        logLay->setContentsMargins(2, 2, 2, 2);

        m_logView = new QTextEdit();
        m_logView->setReadOnly(true);
        m_logView->setFont(QFont("monospace", 9));
        m_logView->setStyleSheet("background: #1e1e1e; color: #d4d4d4;");
        m_logView->document()->setMaximumBlockCount(2000);

        QHBoxLayout *logBtnRow = new QHBoxLayout();
        m_chkShowRaw = new QCheckBox("Show Raw Data");
        m_chkShowRaw->setChecked(false); // Default to hiding raw CAN-FD frames in the log
        m_chkShowRaw->setToolTip("Show/hide raw CAN-FD frame dumps in the log");
        logBtnRow->addWidget(m_chkShowRaw);
        logBtnRow->addStretch();
        QPushButton *btnClearLog = new QPushButton("Clear Log");
        btnClearLog->setFixedWidth(80);
        connect(btnClearLog, &QPushButton::clicked, m_logView, &QTextEdit::clear);
        logBtnRow->addWidget(btnClearLog);

        logLay->addWidget(m_logView, 1);
        logLay->addLayout(logBtnRow);

        m_bottomTabs->addTab(logPage, "Log");
    }

    m_bottomTabs->setCurrentIndex(0);
    m_bottomTabs->setFixedHeight(180);
    mainLayout->addWidget(m_bottomTabs);
}

// ============================================================================
// Tab builders
// ============================================================================
void MainWindow::buildMitTab()
{
    QWidget *w = new QWidget();
    QVBoxLayout *vl = new QVBoxLayout(w);
    vl->setContentsMargins(4, 4, 4, 4);
    vl->setSpacing(2);

    // Motor enable checkboxes row — label width matches the parameter rows
    // below so each checkbox column lines up with its line-edit column.
    {
        auto *row = new QHBoxLayout();
        auto *lbl = new QLabel("Motor:");
        lbl->setFixedWidth(90);
        lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        row->addWidget(lbl);
        for (int i = 0; i < kMaxMotors; ++i) {
            m_mitEn[i] = new QCheckBox(QString("0x0%1").arg(i + 1));
            m_mitEn[i]->setChecked(true);
            row->addWidget(m_mitEn[i]);
        }
        m_mitSync = new QCheckBox("Sync");
        m_mitSync->setToolTip(
            "Send identical parameters to every ticked motor.\n"
            "Motor 1 is the master: its values are replicated, the others are read-only.");
        connect(m_mitSync, &QCheckBox::toggled, this, [this](bool on) {
            if (on) m_mitEn[0]->setChecked(true);   // motor 1 must be in the group
            for (int i = 1; i < kMaxMotors; ++i) {
                m_mitPos[i]->setEnabled(!on);
                m_mitVel[i]->setEnabled(!on);
                m_mitKp[i]->setEnabled(!on);
                m_mitKd[i]->setEnabled(!on);
                m_mitTff[i]->setEnabled(!on);
            }
        });
        row->addWidget(m_mitSync);
        row->addStretch();
        vl->addLayout(row);
    }

    // Parameter rows — one per parameter, one line edit per motor (aligned with the checkbox columns)
    for (int i = 0; i < kMaxMotors; ++i) {
        m_mitPos[i] = makeLineEdit("0");
        m_mitVel[i] = makeLineEdit("0");
        m_mitKp[i]  = makeLineEdit("0");
        m_mitKd[i]  = makeLineEdit("0");
        m_mitTff[i] = makeLineEdit("0");
    }
    vl->addWidget(makeLineRow("Pos (rad):",        m_mitPos, kMaxMotors));
    vl->addWidget(makeLineRow("Vel (rad/s):",      m_mitVel, kMaxMotors));
    vl->addWidget(makeLineRow("Kp (Nm/rad):",      m_mitKp,  kMaxMotors));
    vl->addWidget(makeLineRow("Kd (Nm/(rad/s)):",  m_mitKd,  kMaxMotors));
    vl->addWidget(makeLineRow("Tff (Nm):",         m_mitTff, kMaxMotors));

    // Interval + toggle button
    {
        auto *row = new QHBoxLayout();
        row->addWidget(new QLabel("Interval:"));
        m_spinMitInterval = new QSpinBox();
        m_spinMitInterval->setRange(1, 5000);
        m_spinMitInterval->setValue(10);
        m_spinMitInterval->setSuffix(" ms");
        m_spinMitInterval->setFixedWidth(90);
        row->addWidget(m_spinMitInterval);
        row->addStretch();
        m_lblMitStatus = new QLabel("");
        m_lblMitStatus->setStyleSheet("color: #888; font-size: 9px;");
        row->addWidget(m_lblMitStatus);
        m_btnMitToggle = new QPushButton("Start Sending");
        m_btnMitToggle->setFixedWidth(110);
        m_btnMitToggle->setStyleSheet(
            "QPushButton { background: #2e7d32; color: white; font-weight: bold; }");
        connect(m_btnMitToggle, &QPushButton::clicked, this, &MainWindow::onMitToggle);
        row->addWidget(m_btnMitToggle);
        vl->addLayout(row);
    }

    vl->addStretch();
    m_cmdTabs->addTab(w, "MIT");
}

void MainWindow::buildPosVelTab()
{
    QWidget *w = new QWidget();
    QVBoxLayout *vl = new QVBoxLayout(w);
    vl->setContentsMargins(4, 4, 4, 4);
    vl->setSpacing(2);

    // Motor enable checkboxes — label width matches the parameter rows below
    // so each checkbox column lines up with its line-edit column.
    {
        auto *row = new QHBoxLayout();
        auto *lbl = new QLabel("Motor:");
        lbl->setFixedWidth(90);
        lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        row->addWidget(lbl);
        for (int i = 0; i < kMaxMotors; ++i) {
            m_pvEn[i] = new QCheckBox(QString("0x0%1").arg(i + 1));
            m_pvEn[i]->setChecked(false);   // default control mode = MIT
            row->addWidget(m_pvEn[i]);
        }
        m_pvSync = new QCheckBox("Sync");
        m_pvSync->setToolTip(
            "Send identical parameters to every ticked motor.\n"
            "Motor 1 is the master: its values are replicated, the others are read-only.");
        connect(m_pvSync, &QCheckBox::toggled, this, [this](bool on) {
            if (on) m_pvEn[0]->setChecked(true);   // motor 1 must be in the group
            for (int i = 1; i < kMaxMotors; ++i) {
                m_pvPos[i]->setEnabled(!on);
                m_pvVelLim[i]->setEnabled(!on);
                m_pvAcc[i]->setEnabled(!on);
                m_pvDec[i]->setEnabled(!on);
            }
        });
        row->addWidget(m_pvSync);
        row->addStretch();
        vl->addLayout(row);
    }

    // Pos | Vel limit | Acc | Dec — one line edit per motor (aligned with the checkbox columns)
    for (int i = 0; i < kMaxMotors; ++i) {
        m_pvPos[i]    = makeLineEdit("0");
        m_pvVelLim[i] = makeLineEdit("0");
        m_pvAcc[i]    = makeLineEdit("0");
        m_pvAcc[i]->setToolTip("Acceleration (rad/s²). 0 = use register default.");
        m_pvDec[i]    = makeLineEdit("0");
        m_pvDec[i]->setToolTip("Deceleration (rad/s²). 0 = use register default.");
    }
    vl->addWidget(makeLineRow("Pos (rad):",         m_pvPos,    kMaxMotors));
    vl->addWidget(makeLineRow("Vel limit (rad/s):", m_pvVelLim, kMaxMotors));
    vl->addWidget(makeLineRow("Acc (rad/s²):",      m_pvAcc,    kMaxMotors));
    vl->addWidget(makeLineRow("Dec (rad/s²):",      m_pvDec,    kMaxMotors));

    // Interval + toggle
    {
        auto *row = new QHBoxLayout();
        row->addWidget(new QLabel("Interval:"));
        m_spinPvInterval = new QSpinBox();
        m_spinPvInterval->setRange(10, 5000);
        m_spinPvInterval->setValue(10);
        m_spinPvInterval->setSuffix(" ms");
        m_spinPvInterval->setFixedWidth(90);
        row->addWidget(m_spinPvInterval);
        row->addStretch();
        m_lblPvStatus = new QLabel("");
        m_lblPvStatus->setStyleSheet("color: #888; font-size: 9px;");
        row->addWidget(m_lblPvStatus);
        m_btnPvToggle = new QPushButton("Start Sending");
        m_btnPvToggle->setFixedWidth(110);
        m_btnPvToggle->setStyleSheet(
            "QPushButton { background: #2e7d32; color: white; font-weight: bold; }");
        connect(m_btnPvToggle, &QPushButton::clicked, this, &MainWindow::onPvToggle);
        row->addWidget(m_btnPvToggle);
        vl->addLayout(row);
    }

    vl->addStretch();
    m_cmdTabs->addTab(w, "Pos-Vel");
}

void MainWindow::buildConstVelTab()
{
    QWidget *w = new QWidget();
    QVBoxLayout *vl = new QVBoxLayout(w);
    vl->setContentsMargins(4, 4, 4, 4);
    vl->setSpacing(2);

    // Motor enable checkboxes — label width matches the parameter rows below
    // so each checkbox column lines up with its line-edit column.
    {
        auto *row = new QHBoxLayout();
        auto *lbl = new QLabel("Motor:");
        lbl->setFixedWidth(90);
        lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        row->addWidget(lbl);
        for (int i = 0; i < kMaxMotors; ++i) {
            m_cvEn[i] = new QCheckBox(QString("0x0%1").arg(i + 1));
            m_cvEn[i]->setChecked(false);   // default control mode = MIT
            row->addWidget(m_cvEn[i]);
        }
        m_cvSync = new QCheckBox("Sync");
        m_cvSync->setToolTip(
            "Send identical parameters to every ticked motor.\n"
            "Motor 1 is the master: its values are replicated, the others are read-only.");
        connect(m_cvSync, &QCheckBox::toggled, this, [this](bool on) {
            if (on) m_cvEn[0]->setChecked(true);   // motor 1 must be in the group
            for (int i = 1; i < kMaxMotors; ++i) {
                m_cvVel[i]->setEnabled(!on);
                m_cvAcc[i]->setEnabled(!on);
                m_cvDec[i]->setEnabled(!on);
            }
        });
        row->addWidget(m_cvSync);
        row->addStretch();
        vl->addLayout(row);
    }

    // Vel | Acc | Dec — one line edit per motor (aligned with the checkbox columns)
    for (int i = 0; i < kMaxMotors; ++i) {
        m_cvVel[i] = makeLineEdit("0");
        m_cvAcc[i] = makeLineEdit("0");
        m_cvAcc[i]->setToolTip("Acceleration (rad/s²). 0 = use register default.");
        m_cvDec[i] = makeLineEdit("0");
        m_cvDec[i]->setToolTip("Deceleration (rad/s²). 0 = use register default.");
    }
    vl->addWidget(makeLineRow("Vel (rad/s):", m_cvVel, kMaxMotors));
    vl->addWidget(makeLineRow("Acc (rad/s²):", m_cvAcc, kMaxMotors));
    vl->addWidget(makeLineRow("Dec (rad/s²):", m_cvDec, kMaxMotors));

    // Interval + toggle
    {
        auto *row = new QHBoxLayout();
        row->addWidget(new QLabel("Interval:"));
        m_spinCvInterval = new QSpinBox();
        m_spinCvInterval->setRange(10, 5000);
        m_spinCvInterval->setValue(10);
        m_spinCvInterval->setSuffix(" ms");
        m_spinCvInterval->setFixedWidth(90);
        row->addWidget(m_spinCvInterval);
        row->addStretch();
        m_lblCvStatus = new QLabel("");
        m_lblCvStatus->setStyleSheet("color: #888; font-size: 9px;");
        row->addWidget(m_lblCvStatus);
        m_btnCvToggle = new QPushButton("Start Sending");
        m_btnCvToggle->setFixedWidth(110);
        m_btnCvToggle->setStyleSheet(
            "QPushButton { background: #2e7d32; color: white; font-weight: bold; }");
        connect(m_btnCvToggle, &QPushButton::clicked, this, &MainWindow::onCvToggle);
        row->addWidget(m_btnCvToggle);
        vl->addLayout(row);
    }

    vl->addStretch();
    m_cmdTabs->addTab(w, "Const Vel");
}

void MainWindow::buildSystemTab()
{
    QWidget *w = new QWidget();
    QVBoxLayout *vl = new QVBoxLayout(w);
    vl->setContentsMargins(4, 4, 4, 4);
    vl->setSpacing(3);

    // Motor enable checkboxes
    {
        auto *row = new QHBoxLayout();
        row->addWidget(new QLabel("Motor:"));
        for (int i = 0; i < kMaxMotors; ++i) {
            m_sysEn[i] = new QCheckBox(QString("0x0%1").arg(i + 1));
            m_sysEn[i]->setChecked(true);
            row->addWidget(m_sysEn[i]);
        }
        row->addStretch();
        vl->addLayout(row);
    }

    auto makeSysBtn = [&](const QString &text, int cmd) {
        auto *b = new QPushButton(text);
        connect(b, &QPushButton::clicked, this, [this, cmd]{ onSendSystemCmd(cmd); });
        return b;
    };

    QGridLayout *grid = new QGridLayout();
    m_btnSysEnable       = makeSysBtn("Enable",      SYS_CMD_ENABLE);
    m_btnSysDisable      = makeSysBtn("Disable",     SYS_CMD_DISABLE);
    m_btnSysSetZero      = makeSysBtn("Set Zero",    SYS_CMD_SET_ZERO);
    m_btnSysClearFault   = makeSysBtn("Clear Fault", SYS_CMD_CLEAR_FAULT);
    m_btnSysSave         = makeSysBtn("Save",        SYS_CMD_SAVE);
    m_btnSysReset        = makeSysBtn("Reset",       SYS_CMD_RESET);
    m_btnSysLoadDefaults = makeSysBtn("Load Default",SYS_CMD_LOAD_DEFAULTS);

    grid->addWidget(m_btnSysEnable,       0, 0);
    grid->addWidget(m_btnSysDisable,      0, 1);
    grid->addWidget(m_btnSysSetZero,      1, 0);
    grid->addWidget(m_btnSysClearFault,   1, 1);
    grid->addWidget(m_btnSysSave,         2, 0);
    grid->addWidget(m_btnSysReset,        2, 1);
    grid->addWidget(m_btnSysLoadDefaults, 3, 0, 1, 2);
    vl->addLayout(grid);
    vl->addStretch();
    m_cmdTabs->addTab(w, "System");
}

// ============================================================================
// OTA / IAP — open the standalone firmware-upgrade window (protocol v2)
// ============================================================================
void MainWindow::onOpenOtaWindow()
{
    if (!m_otaWindow) {
        m_otaWindow = new OtaWindow(this);

        // OTA frames are transmitted through the shared CAN device.
        connect(m_otaWindow, &OtaWindow::sendFrameRequested,
                this, [this](const CanFdFrame &f) {
                    m_device->sendFrame(f);
                    logRawTx(f, "TX ota");
                });
        // OTA log lines are forwarded to the main window's communication log.
        connect(m_otaWindow, &OtaWindow::logMessage,
                this, &MainWindow::logMessage);
    }

    // Target node defaults to the register-tab motor selection.
    m_otaWindow->setTargetNode(static_cast<quint8>(m_regMotorId->value()));
    m_otaWindow->setConnected(m_connected);
    m_otaWindow->setAnchorGeometry(frameGeometry());

    // Stop the periodic control senders — OTA owns the bus during a session.
    if (m_otaWindow->isVisible())
        stopAllTimers();

    m_otaWindow->show();
}

// ============================================================================
// Same-motor checkbox mutual exclusion across the control-mode tabs.
// Checking a motor in one mode unchecks it in the other two, so every motor
// has at most one control mode. System-tab checkboxes are excluded.
// ============================================================================
void MainWindow::setupModeCheckboxExclusion()
{
    for (int i = 0; i < kMaxMotors; ++i) {
        connect(m_mitEn[i], &QCheckBox::toggled, this, [this, i](bool checked) {
            if (checked) {
                m_pvEn[i]->setChecked(false);
                m_cvEn[i]->setChecked(false);
            }
        });
        connect(m_pvEn[i], &QCheckBox::toggled, this, [this, i](bool checked) {
            if (checked) {
                m_mitEn[i]->setChecked(false);
                m_cvEn[i]->setChecked(false);
            }
        });
        connect(m_cvEn[i], &QCheckBox::toggled, this, [this, i](bool checked) {
            if (checked) {
                m_mitEn[i]->setChecked(false);
                m_pvEn[i]->setChecked(false);
            }
        });
    }
}

// ============================================================================
// Logging
// ============================================================================
void MainWindow::logMessage(const QString &msg)
{
    QString ts = QDateTime::currentDateTime().toString("HH:mm:ss.zzz");
    m_logView->append("[" + ts + "] " + msg);
}

void MainWindow::logRawTx(const CanFdFrame &frame, const QString &dir)
{
    // Raw frame dumps are optional — toggled by the "Show Raw Data" switch.
    if (m_chkShowRaw && !m_chkShowRaw->isChecked())
        return;

    QString hex;
    for (int i = 0; i < frame.len; ++i)
        hex += QString(" %1").arg(frame.data[i], 2, 16, QChar('0')).toUpper();
    logMessage(QString("%1  ID=0x%2  DLC=%3 [%4]")
                   .arg(dir)
                   .arg(frame.id, 3, 16, QChar('0')).toUpper()
                   .arg(frame.len)
                   .arg(hex.trimmed()));
}

// ============================================================================
// Device connection
// ============================================================================
void MainWindow::onDeviceToggle()
{
    if (m_connected) {
        stopNodeScan();
        stopAllTimers();
        m_device->stopReceive();
        m_device->close();
        m_connected = false;
        m_btnConnect->setText("Connect");
        m_lblDevStatus->setText("Disconnected");
        m_lblDevStatus->setStyleSheet("color: #888;");
        if (m_otaWindow) m_otaWindow->setConnected(false);
        logMessage("Device disconnected.");
    } else {
        bool ok = m_device->open();
        if (!ok) {
            QMessageBox::warning(this, "Open Failed",
                "Failed to open device.\n" + m_device->lastError());
            return;
        }
        m_device->startReceive();
        m_connected = true;
        m_btnConnect->setText("Disconnect");
        m_lblDevStatus->setText("Connected");
        m_lblDevStatus->setStyleSheet("color: #4caf50; font-weight: bold;");
        if (m_otaWindow) m_otaWindow->setConnected(true);
        logMessage("Device opened successfully.");

        // Discover active nodes (node IDs may be reassigned, not fixed 1..4).
        startNodeScan();
    }
}

void MainWindow::stopAllTimers()
{
    // MIT
    if (m_mitRunning) onMitToggle();
    // Pos-Vel
    if (m_pvRunning)  onPvToggle();
    // Const Vel
    if (m_cvRunning)  onCvToggle();
}

// ============================================================================
// Node-ID scan — discover active nodes by reading REG_NODE_ID (0x00) on every
// candidate CAN node ID. A device only answers when the CAN ID matches its own
// node ID, so a response proves the node exists. 0.05 s timeout per probe;
// the scan ends early once kScanTargetCount (4) nodes have responded.
// ============================================================================
void MainWindow::startNodeScan()
{
    if (m_scanning) stopNodeScan();

    m_scanning = true;
    m_scanNode = 0x01;
    m_scanFound.clear();

    // Invalidate all pages until the scan binds them to a discovered node.
    for (int i = 0; i < kMaxMotors; ++i) {
        m_nodeIds[i]   = 0;
        m_nodeValid[i] = false;
        setMotorPageEnabled(i, false);
    }

    if (!m_scanTimer) {
        m_scanTimer = new QTimer(this);
        connect(m_scanTimer, &QTimer::timeout, this, &MainWindow::onNodeScanTick);
    }
    logMessage(QString("[Scan] Node-ID scan started (probe reg 0x%1, timeout %2 ms)")
                   .arg(REG_NODE_ID, 2, 16, QChar('0'))
                   .arg(kScanProbeTimeoutMs));
    onNodeScanTick();   // send the first probe immediately
}

void MainWindow::stopNodeScan()
{
    if (m_scanTimer) m_scanTimer->stop();
    m_scanning = false;
}

// Push the scan result into the motor-page → node-ID map, then reflect it in
// each register table's "Node ID" (RID 0x00) row. Pages keep their names
// ("Motor 1".."Motor 4"); only the underlying CAN node ID changes.
void MainWindow::applyNodeMap()
{
    // Assign discovered nodes to pages in order. Pages without a discovered
    // node (index >= found count) are marked unknown (id 0, invalid) and all
    // of their related controls are disabled.
    const int found = m_scanFound.size();
    for (int i = 0; i < kMaxMotors; ++i) {
        if (i < found) {
            m_nodeIds[i]   = m_scanFound[i];
            m_nodeValid[i] = true;
        } else {
            m_nodeIds[i]   = 0;
            m_nodeValid[i] = false;
        }
    }

    // Reflect the mapping in each page: node ID + RSTAT row, table state.
    for (int mi = 0; mi < kMaxMotors; ++mi) {
        const bool valid = m_nodeValid[mi];
        setMotorPageEnabled(mi, valid);
        m_plotPanel_->setMotorAvailable(static_cast<uint8_t>(mi + 1), valid);

        QTableWidget *tbl = m_regTables[mi];
        if (!tbl) continue;
        for (int r = 0; r < tbl->rowCount(); ++r) {
            auto *it = tbl->item(r, 0);
            if (!it) continue;
            bool ok = false;
            const uint16_t rowAddr = it->text().toUInt(&ok, 16);
            if (ok && rowAddr == REG_NODE_ID) {
                auto *vi = tbl->item(r, 2);
                if (!vi) { vi = new QTableWidgetItem(); tbl->setItem(r, 2, vi); }
                auto *si = tbl->item(r, 3);
                if (!si) { si = new QTableWidgetItem(); tbl->setItem(r, 3, si); }

                if (valid) {
                    vi->setText(QString("0x%1")
                                    .arg(m_nodeIds[mi], 2, 16, QChar('0')).toUpper());
                    vi->setForeground(QColor("#1F6FB2"));
                    si->setText("OK (0x00)");
                    si->setForeground(QColor("#4caf50"));
                } else {
                    vi->setText("unknown");
                    vi->setForeground(QColor("#c62828"));
                    si->setText("--");
                    si->setForeground(QColor("#888"));
                }
                break;
            }
        }
    }

    if (found == 0) {
        logMessage("[Scan] No nodes found — all motor pages disabled.");
    } else if (found < kMaxMotors) {
        QStringList avail, missing;
        for (int i = 0; i < kMaxMotors; ++i)
            (m_nodeValid[i] ? avail : missing)
                << QString("Motor %1").arg(i + 1);
        logMessage(QString("[Scan] Only %1/%2 nodes found (available: %3). "
                           "Disabled: %4")
                       .arg(found).arg(kMaxMotors)
                       .arg(avail.join(", "), missing.join(", ")));
    }

    logMessage(QString("[Scan] Motor mapping: Motor 1->%1  Motor 2->%2  "
                       "Motor 3->%3  Motor 4->%4")
                   .arg(m_nodeValid[0] ? QString("0x%1").arg(m_nodeIds[0], 2, 16, QChar('0')).toUpper() : "unknown")
                   .arg(m_nodeValid[1] ? QString("0x%1").arg(m_nodeIds[1], 2, 16, QChar('0')).toUpper() : "unknown")
                   .arg(m_nodeValid[2] ? QString("0x%1").arg(m_nodeIds[2], 2, 16, QChar('0')).toUpper() : "unknown")
                   .arg(m_nodeValid[3] ? QString("0x%1").arg(m_nodeIds[3], 2, 16, QChar('0')).toUpper() : "unknown"));
}

void MainWindow::onNodeScanTick()
{
    if (!m_scanning || !m_connected) {
        stopNodeScan();
        return;
    }

    // Wait for the response window of the probe just sent before advancing.
    if (m_scanTimer->isActive()
        && m_scanElapsed.elapsed() < kScanProbeTimeoutMs) {
        return;
    }

    if (m_scanNode > kScanMaxNode) {
        logMessage(QString("[Scan] Done — %1 node(s) found: %2")
                       .arg(m_scanFound.size())
                       .arg(m_scanFound.isEmpty() ? QString("none")
                                                  : formatNodeList(m_scanFound)));
        stopNodeScan();
        applyNodeMap();
        return;
    }

    // Send a single-register read of REG_NODE_ID to the candidate CAN node.
    CanFdFrame f = m_proto.encodeRegRead(static_cast<uint8_t>(m_scanNode),
                                         REG_NODE_ID);
    m_device->sendFrame(f);
    logRawTx(f, "TX scan");

    m_scanElapsed.restart();
    m_scanTimer->start(kScanProbeTimeoutMs);
    ++m_scanNode;
}

// ============================================================================
// MIT Tab — per-motor periodic send
// ============================================================================
void MainWindow::onMitToggle()
{
    if (m_mitRunning) {
        m_mitTimer->stop();
        m_mitRunning = false;
        // Release motors owned by MIT so another mode can take them.
        for (int i = 0; i < kMaxMotors; ++i)
            if (m_motorMode[i] == MODE_MIT) m_motorMode[i] = MODE_NONE;
        m_btnMitToggle->setText("Start Sending");
        m_btnMitToggle->setStyleSheet(
            "QPushButton { background: #2e7d32; color: white; font-weight: bold; }");
        m_spinMitInterval->setEnabled(true);
    } else {
        // Take over every checked motor — each motor keeps exactly one
        // control mode (System commands are exempt).
        QString taken;
        for (int i = 0; i < kMaxMotors; ++i) {
            if (!m_mitEn[i]->isChecked()) continue;
            if (m_motorMode[i] != MODE_NONE && m_motorMode[i] != MODE_MIT)
                taken += QString("0x0%1 ").arg(i + 1);
            m_motorMode[i] = MODE_MIT;
        }
        if (!taken.isEmpty())
            logMessage(QString("MIT takes over motor(s): %1").arg(taken));

        // Validate the send inputs of every checked motor before starting.
        for (int i = 0; i < kMaxMotors; ++i) {
            if (!m_mitEn[i]->isChecked()) continue;
            QLineEdit *edits[] = {m_mitPos[i], m_mitVel[i], m_mitKp[i], m_mitKd[i], m_mitTff[i]};
            if (!validateSendInputs(edits, 5, i)) return;
        }

        int ms = m_spinMitInterval->value();
        m_mitTimer->start(ms);
        m_mitRunning = true;
        m_plotPanel_->clearHistory();
        m_btnMitToggle->setText("Stop Sending");
        m_btnMitToggle->setStyleSheet(
            "QPushButton { background: #c62828; color: white; font-weight: bold; }");
        m_spinMitInterval->setEnabled(false);

        // Tabs that no longer control any motor stop automatically.
        if (m_pvRunning && !modeHasMotors(MODE_PV)) onPvToggle();
        if (m_cvRunning && !modeHasMotors(MODE_CV)) onCvToggle();
    }
    refreshModeLabels();
    updateDiagramTiming();
}

void MainWindow::onMitTick()
{
    if (!m_connected) return;

    // Collect motors that are ticked and owned by this mode.
    QVector<int> active;
    for (int i = 0; i < kMaxMotors; ++i) {
        if (!m_nodeValid[i]) continue;              // node unknown → never command
        if (!m_mitEn[i]->isChecked()) {
            // Motor deselected mid-run → release it for other modes.
            if (m_motorMode[i] == MODE_MIT) m_motorMode[i] = MODE_NONE;
            continue;
        }
        if (m_motorMode[i] != MODE_MIT) continue;   // taken over by another mode
        active.append(i);
    }

    // All motors released/taken over → auto-stop this tab.
    if (active.isEmpty()) {
        if (m_mitRunning) onMitToggle();
        refreshModeLabels();
        return;
    }

    const bool sync = m_mitSync && m_mitSync->isChecked();
    if (active.size() > 1) {
        // Aggregated frame (V0.2 §6) — one frame for all.
        std::vector<YwdProtocol::AggMitRecord> recs;
        recs.reserve(active.size());
        for (int idx : active) {
            const int src = sync ? 0 : idx;   // sync → replicate motor 1 data
            recs.push_back({nodeIdForIndex(idx),
                static_cast<float>(lineEditValue(m_mitPos[src], 0.0)),
                static_cast<float>(lineEditValue(m_mitVel[src], 0.0)),
                static_cast<float>(lineEditValue(m_mitKp[src], 0.0)),
                static_cast<float>(lineEditValue(m_mitKd[src], 0.0)),
                static_cast<float>(lineEditValue(m_mitTff[src], 0.0))});
        }
        CanFdFrame f = m_proto.encodeAggMit(recs);
        m_device->sendFrame(f);
        logRawTx(f, "TX agg");
    } else {
        const int i = active.first();
        uint8_t mid = nodeIdForIndex(i);
        CanFdFrame f = m_proto.encodeMitCtrl(mid,
            static_cast<float>(lineEditValue(m_mitPos[i], 0.0)),
            static_cast<float>(lineEditValue(m_mitVel[i], 0.0)),
            static_cast<float>(lineEditValue(m_mitKp[i], 0.0)),
            static_cast<float>(lineEditValue(m_mitKd[i], 0.0)),
            static_cast<float>(lineEditValue(m_mitTff[i], 0.0)));
        m_device->sendFrame(f);
        logRawTx(f, "TX");
    }
    refreshModeLabels();
}

// ============================================================================
// Pos-Vel Tab — per-motor periodic send
// ============================================================================
void MainWindow::onPvToggle()
{
    if (m_pvRunning) {
        m_pvTimer->stop();
        m_pvRunning = false;
        // Release motors owned by Pos-Vel so another mode can take them.
        for (int i = 0; i < kMaxMotors; ++i)
            if (m_motorMode[i] == MODE_PV) m_motorMode[i] = MODE_NONE;
        m_btnPvToggle->setText("Start Sending");
        m_btnPvToggle->setStyleSheet(
            "QPushButton { background: #2e7d32; color: white; font-weight: bold; }");
        m_spinPvInterval->setEnabled(true);
    } else {
        // Take over every checked motor — each motor keeps exactly one
        // control mode (System commands are exempt).
        QString taken;
        for (int i = 0; i < kMaxMotors; ++i) {
            if (!m_pvEn[i]->isChecked()) continue;
            if (m_motorMode[i] != MODE_NONE && m_motorMode[i] != MODE_PV)
                taken += QString("0x0%1 ").arg(i + 1);
            m_motorMode[i] = MODE_PV;
        }
        if (!taken.isEmpty())
            logMessage(QString("Pos-Vel takes over motor(s): %1").arg(taken));

        // Validate the send inputs of every checked motor before starting.
        for (int i = 0; i < kMaxMotors; ++i) {
            if (!m_pvEn[i]->isChecked()) continue;
            QLineEdit *edits[] = {m_pvPos[i], m_pvVelLim[i], m_pvAcc[i], m_pvDec[i]};
            if (!validateSendInputs(edits, 4, i)) return;
        }

        int ms = m_spinPvInterval->value();
        m_pvTimer->start(ms);
        m_pvRunning = true;
        m_plotPanel_->clearHistory();
        m_btnPvToggle->setText("Stop Sending");
        m_btnPvToggle->setStyleSheet(
            "QPushButton { background: #c62828; color: white; font-weight: bold; }");
        m_spinPvInterval->setEnabled(false);

        // Tabs that no longer control any motor stop automatically.
        if (m_mitRunning && !modeHasMotors(MODE_MIT)) onMitToggle();
        if (m_cvRunning && !modeHasMotors(MODE_CV)) onCvToggle();
    }
    refreshModeLabels();
    updateDiagramTiming();
}

void MainWindow::onPvTick()
{
    if (!m_connected) return;

    // Collect motors that are ticked and owned by this mode.
    QVector<int> active;
    for (int i = 0; i < kMaxMotors; ++i) {
        if (!m_nodeValid[i]) continue;              // node unknown → never command
        if (!m_pvEn[i]->isChecked()) {
            // Motor deselected mid-run → release it for other modes.
            if (m_motorMode[i] == MODE_PV) m_motorMode[i] = MODE_NONE;
            continue;
        }
        if (m_motorMode[i] != MODE_PV) continue;   // taken over by another mode
        active.append(i);
    }

    // All motors released/taken over → auto-stop this tab.
    if (active.isEmpty()) {
        if (m_pvRunning) onPvToggle();
        refreshModeLabels();
        return;
    }

    const bool sync = m_pvSync && m_pvSync->isChecked();
    if (active.size() > 1) {
        // Aggregated frame (V0.2 §6) — one frame for all.
        std::vector<YwdProtocol::AggPosVelRecord> recs;
        recs.reserve(active.size());
        for (int idx : active) {
            const int src = sync ? 0 : idx;   // sync → replicate motor 1 data
            recs.push_back({nodeIdForIndex(idx),
                static_cast<float>(lineEditValue(m_pvPos[src], 0.0)),
                static_cast<float>(lineEditValue(m_pvVelLim[src], 0.0)),
                static_cast<float>(lineEditValue(m_pvAcc[src], 0.0)),
                static_cast<float>(lineEditValue(m_pvDec[src], 0.0))});
        }
        CanFdFrame f = m_proto.encodeAggPosVel(recs);
        m_device->sendFrame(f);
        logRawTx(f, "TX agg");
    } else {
        const int i = active.first();
        uint8_t mid = nodeIdForIndex(i);
        CanFdFrame f = m_proto.encodePosVel(mid,
            static_cast<float>(lineEditValue(m_pvPos[i], 0.0)),
            static_cast<float>(lineEditValue(m_pvVelLim[i], 0.0)),
            static_cast<float>(lineEditValue(m_pvAcc[i], 0.0)),
            static_cast<float>(lineEditValue(m_pvDec[i], 0.0)));
        m_device->sendFrame(f);
        logRawTx(f, "TX");
    }
    refreshModeLabels();
}

// ============================================================================
// Const Vel Tab — per-motor periodic send
// ============================================================================
void MainWindow::onCvToggle()
{
    if (m_cvRunning) {
        m_cvTimer->stop();
        m_cvRunning = false;
        // Release motors owned by Const Vel so another mode can take them.
        for (int i = 0; i < kMaxMotors; ++i)
            if (m_motorMode[i] == MODE_CV) m_motorMode[i] = MODE_NONE;
        m_btnCvToggle->setText("Start Sending");
        m_btnCvToggle->setStyleSheet(
            "QPushButton { background: #2e7d32; color: white; font-weight: bold; }");
        m_spinCvInterval->setEnabled(true);
    } else {
        // Take over every checked motor — each motor keeps exactly one
        // control mode (System commands are exempt).
        QString taken;
        for (int i = 0; i < kMaxMotors; ++i) {
            if (!m_cvEn[i]->isChecked()) continue;
            if (m_motorMode[i] != MODE_NONE && m_motorMode[i] != MODE_CV)
                taken += QString("0x0%1 ").arg(i + 1);
            m_motorMode[i] = MODE_CV;
        }
        if (!taken.isEmpty())
            logMessage(QString("Const Vel takes over motor(s): %1").arg(taken));

        // Validate the send inputs of every checked motor before starting.
        for (int i = 0; i < kMaxMotors; ++i) {
            if (!m_cvEn[i]->isChecked()) continue;
            QLineEdit *edits[] = {m_cvVel[i], m_cvAcc[i], m_cvDec[i]};
            if (!validateSendInputs(edits, 3, i)) return;
        }

        int ms = m_spinCvInterval->value();
        m_cvTimer->start(ms);
        m_cvRunning = true;
        m_plotPanel_->clearHistory();
        m_btnCvToggle->setText("Stop Sending");
        m_btnCvToggle->setStyleSheet(
            "QPushButton { background: #c62828; color: white; font-weight: bold; }");
        m_spinCvInterval->setEnabled(false);

        // Tabs that no longer control any motor stop automatically.
        if (m_mitRunning && !modeHasMotors(MODE_MIT)) onMitToggle();
        if (m_pvRunning && !modeHasMotors(MODE_PV)) onPvToggle();
    }
    refreshModeLabels();
    updateDiagramTiming();
}

void MainWindow::onCvTick()
{
    if (!m_connected) return;

    // Collect motors that are ticked and owned by this mode.
    QVector<int> active;
    for (int i = 0; i < kMaxMotors; ++i) {
        if (!m_nodeValid[i]) continue;              // node unknown → never command
        if (!m_cvEn[i]->isChecked()) {
            // Motor deselected mid-run → release it for other modes.
            if (m_motorMode[i] == MODE_CV) m_motorMode[i] = MODE_NONE;
            continue;
        }
        if (m_motorMode[i] != MODE_CV) continue;   // taken over by another mode
        active.append(i);
    }

    // All motors released/taken over → auto-stop this tab.
    if (active.isEmpty()) {
        if (m_cvRunning) onCvToggle();
        refreshModeLabels();
        return;
    }

    const bool sync = m_cvSync && m_cvSync->isChecked();
    if (active.size() > 1) {
        // Aggregated frame (V0.2 §6) — one frame for all.
        std::vector<YwdProtocol::AggConstVelRecord> recs;
        recs.reserve(active.size());
        for (int idx : active) {
            const int src = sync ? 0 : idx;   // sync → replicate motor 1 data
            recs.push_back({nodeIdForIndex(idx),
                static_cast<float>(lineEditValue(m_cvVel[src], 0.0)),
                static_cast<float>(lineEditValue(m_cvAcc[src], 0.0)),
                static_cast<float>(lineEditValue(m_cvDec[src], 0.0))});
        }
        CanFdFrame f = m_proto.encodeAggConstVel(recs);
        m_device->sendFrame(f);
        logRawTx(f, "TX agg");
    } else {
        const int i = active.first();
        uint8_t mid = nodeIdForIndex(i);
        CanFdFrame f = m_proto.encodeConstVel(mid,
            static_cast<float>(lineEditValue(m_cvVel[i], 0.0)),
            static_cast<float>(lineEditValue(m_cvAcc[i], 0.0)),
            static_cast<float>(lineEditValue(m_cvDec[i], 0.0)));
        m_device->sendFrame(f);
        logRawTx(f, "TX");
    }
    refreshModeLabels();
}

// ============================================================================
// Derive diagram update interval from active send interval(s).
// Diagram update = send_interval × 10  (e.g. send 100 ms → diagram 1000 ms).
// Uses the minimum of currently active send intervals if more than one tab is running.
// ============================================================================
void MainWindow::updateDiagramTiming()
{
    int minMs = INT_MAX;
    if (m_mitRunning)  minMs = std::min(minMs, m_spinMitInterval->value());
    if (m_pvRunning)   minMs = std::min(minMs, m_spinPvInterval->value());
    if (m_cvRunning)   minMs = std::min(minMs, m_spinCvInterval->value());

    if (minMs == INT_MAX)
        return;   // no send active — keep current interval

    m_plotPanel_->setUpdateInterval(minMs * 10);
}

// ============================================================================
// One-control-mode-per-motor helpers
// ============================================================================
bool MainWindow::modeHasMotors(int mode) const
{
    for (int i = 0; i < kMaxMotors; ++i)
        if (m_motorMode[i] == mode) return true;
    return false;
}

QString MainWindow::modeMotorList(int mode) const
{
    // Show the actual CAN node ID each page talks to (may differ from the page
    // number once the scan has remapped nodes).
    QStringList ids;
    for (int i = 0; i < kMaxMotors; ++i)
        if (m_motorMode[i] == mode)
            ids << QString("0x%1").arg(m_nodeIds[i], 2, 16, QChar('0')).toUpper();
    return ids.join(" ");
}

void MainWindow::refreshModeLabels()
{
    m_lblMitStatus->setText(m_mitRunning ? "Running: " + modeMotorList(MODE_MIT) : "");
    m_lblPvStatus->setText(m_pvRunning  ? "Running: " + modeMotorList(MODE_PV)  : "");
    m_lblCvStatus->setText(m_cvRunning  ? "Running: " + modeMotorList(MODE_CV)  : "");
}

// ============================================================================
// System Tab — send to all checked motors
// ============================================================================
void MainWindow::onSendSystemCmd(int cmdCode)
{
    if (!m_connected) return;

    for (int i = 0; i < kMaxMotors; ++i) {
        if (!m_nodeValid[i]) continue;              // node unknown → never command
        if (!m_sysEn[i]->isChecked()) continue;
        uint8_t mid = nodeIdForIndex(i);
        CanFdFrame f = m_proto.encodeSystemCmd(mid, cmdCode);
        m_device->sendFrame(f);
        logRawTx(f, "TX");
    }
}

// ============================================================================
// Register access
// ============================================================================
void MainWindow::onRegRead()
{
    if (!m_connected) return;
    uint8_t  mid  = static_cast<uint8_t>(m_regMotorId->value());
    uint16_t addr = static_cast<uint16_t>(m_spinRegAddr->value());
    CanFdFrame f = m_proto.encodeRegRead(mid, addr);
    m_device->sendFrame(f);
    logRawTx(f, "TX");
}

void MainWindow::onRegWrite()
{
    if (!m_connected) return;

    uint8_t  mid  = static_cast<uint8_t>(m_regMotorId->value());
    uint16_t addr = static_cast<uint16_t>(m_spinRegAddr->value());

    uint32_t value = 0;
    QString  err;
    if (!parseRegValue(static_cast<uint8_t>(addr), m_editRegValue->text(), value, err)) {
        m_lblRegResult->setText("Invalid value: " + err);
        m_lblRegResult->setStyleSheet("color: red; font-family: monospace;");
        return;
    }

    CanFdFrame f = m_proto.encodeRegWrite(mid, addr, value);
    m_device->sendFrame(f);
    logRawTx(f, "TX");
}

// ============================================================================
// Typed register value helpers — format/parse per RegInfo::data_type.
// float32 → IEEE-754 float, uint32/int32 → decimal integer. No hex input.
// ============================================================================
QString MainWindow::formatRegValue(uint8_t addr, uint32_t value) const
{
    QString type;
    for (const auto &reg : m_allRegs)
        if (reg.addr == addr) { type = reg.data_type; break; }

    if (type == QStringLiteral("float32")) {
        float f;
        memcpy(&f, &value, sizeof(f));
        if (std::isfinite(f))
            return QString::number(f, 'f', 4);
        return QString("0x%1").arg(value, 8, 16, QChar('0')).toUpper();
    }
    if (type == QStringLiteral("int32"))
        return QString::number(static_cast<int32_t>(value));
    return QString::number(value);   // uint32 (default)
}

bool MainWindow::parseRegValue(uint8_t addr, const QString &text,
                               uint32_t &value, QString &err) const
{
    QString type;
    for (const auto &reg : m_allRegs)
        if (reg.addr == addr) { type = reg.data_type; break; }

    const QString s = text.trimmed();
    if (s.isEmpty()) { err = "empty value"; return false; }

    if (type == QStringLiteral("float32")) {
        bool ok = false;
        const float f = s.toFloat(&ok);
        if (!ok) { err = "not a number: '" + text + "'"; return false; }
        memcpy(&value, &f, sizeof(f));
        return true;
    }

    bool ok = false;
    const qlonglong v = s.toLongLong(&ok);
    if (!ok) { err = "not an integer: '" + text + "'"; return false; }
    if (type == QStringLiteral("int32")) {
        if (v < INT32_MIN || v > INT32_MAX) {
            err = "out of int32 range (-2147483648..2147483647)";
            return false;
        }
        value = static_cast<uint32_t>(static_cast<int32_t>(v));
    } else {
        if (v < 0 || v > static_cast<qlonglong>(UINT32_MAX)) {
            err = "out of uint32 range (0..4294967295)";
            return false;
        }
        value = static_cast<uint32_t>(v);
    }
    return true;
}

void MainWindow::onReadAllRegs(uint8_t motor_page)
{
    if (!m_connected) return;
    if (motor_page < 0x01 || motor_page > kMaxMotors) return;
    if (!isNodeValid(motor_page - 0x01)) return;   // node unknown → nothing to read

    m_batchMotorIdx = motor_page - 0x01;   // page index; node resolved per block
    m_regBatchIdx = 0;
    m_batchWaiting = false;
    m_batchRetries = 0;

    int mi = m_batchMotorIdx;
    m_btnReadAllMotor[mi]->setEnabled(false);
    m_btnReadAllMotor[mi]->setText("Reading...");
    m_lblReadAllMotor[mi]->setText("0 / " + QString::number(m_allRegs.size()));

    // Response-driven batch read: one block request → wait for its response
    // → send the next block. The timer acts as a watchdog only.
    if (!m_batchReadTimer->isActive())
        m_batchReadTimer->start(50);
    sendNextRegBlock();
}

// ============================================================================
// Send the next block of up to 8 RIDs and wait for its response (§8.1/§8.2).
// ============================================================================
void MainWindow::sendNextRegBlock()
{
    int mi = m_batchMotorIdx;

    if (m_regBatchIdx >= m_allRegs.size()) {
        // All registers read for this motor — done
        m_batchReadTimer->stop();
        m_batchWaiting = false;
        m_btnReadAllMotor[mi]->setEnabled(true);
        m_btnReadAllMotor[mi]->setText("Read All");
        m_lblReadAllMotor[mi]->setText("Done");
        logMessage(QString("Batch register read complete (Motor %1).").arg(mi + 1));
        return;
    }

    m_batchBlockStart = m_regBatchIdx;
    uint8_t mid = nodeIdForIndex(mi);
    std::vector<uint8_t> rids;
    // §8.1: a block READ/WRITE addresses up to 8 registers per frame
    for (int i = 0; i < 8 && m_regBatchIdx < m_allRegs.size(); ++i, ++m_regBatchIdx)
        rids.push_back(static_cast<uint8_t>(m_allRegs[m_regBatchIdx].addr));

    CanFdFrame f = m_proto.encodeRegBlockRead(mid, rids);
    m_device->sendFrame(f);
    logRawTx(f, "TX");

    m_batchPendingN = static_cast<int>(rids.size());
    m_batchWaiting  = true;
    m_batchRetries  = 0;
    m_batchElapsed.restart();

    m_lblReadAllMotor[mi]->setText(
        QString("%1 / %2").arg(m_regBatchIdx).arg(m_allRegs.size()));
}

// ============================================================================
// Re-send the block currently awaiting a response (response timeout).
// ============================================================================
void MainWindow::resendCurrentBlock()
{
    int mi = m_batchMotorIdx;
    uint8_t mid = nodeIdForIndex(mi);
    std::vector<uint8_t> rids;
    for (int i = 0; i < m_batchPendingN; ++i)
        rids.push_back(static_cast<uint8_t>(m_allRegs[m_batchBlockStart + i].addr));

    CanFdFrame f = m_proto.encodeRegBlockRead(mid, rids);
    m_device->sendFrame(f);
    logRawTx(f, "TX (retry)");

    ++m_batchRetries;
    m_batchElapsed.restart();
}

// ============================================================================
// Watchdog – only acts when a block response is overdue.
// ============================================================================
void MainWindow::onBatchReadNext()
{
    if (!m_connected) {
        m_batchReadTimer->stop();
        m_regBatchIdx = 0;
        m_batchMotorIdx = 0;
        m_batchWaiting = false;
        for (int mi = 0; mi < kMaxMotors; ++mi) {
            m_btnReadAllMotor[mi]->setEnabled(true);
            m_btnReadAllMotor[mi]->setText("Read All");
        }
        return;
    }

    if (!m_batchWaiting) {
        sendNextRegBlock();
        return;
    }

    // Still within the response timeout — keep waiting.
    if (m_batchElapsed.elapsed() < 200)
        return;

    if (m_batchRetries < 3) {
        resendCurrentBlock();          // response lost → retry same block
    } else {
        logMessage(
            QString("Batch read: Motor %1 (node 0x%2) block @0x%3 unresponsive, skipping.")
                .arg(m_batchMotorIdx + 1)
                .arg(nodeIdForIndex(m_batchMotorIdx), 2, 16, QChar('0'))
                .arg(m_allRegs[m_batchBlockStart].addr, 2, 16, QChar('0')));
        m_batchWaiting = false;
        sendNextRegBlock();            // skip — m_regBatchIdx already advanced
    }
}

// ============================================================================
// Stats poll
// ============================================================================
void MainWindow::onPollTimer()
{
    if (++m_pollTicks < 20) return;

    double hz = static_cast<double>(m_fbCount - m_lastFbCount)
                / (m_pollTicks * 0.05);
    m_lastFbCount = m_fbCount;
    m_pollTicks = 0;

    m_lblRxStats->setText(QString("RX: %1  |  Feedback: %2 Hz")
                              .arg(m_rxCount)
                              .arg(hz, 0, 'f', 1));
}

// ============================================================================
// Receive handler
// ============================================================================
void MainWindow::onFrameReceived(const CanFdFrame &frame)
{
    ++m_rxCount;

    // OTA uplink (extended 0x08000000|node, §1.2) — forwarded to the OTA window
    // when open, so heartbeats refresh device identity even when idle.
    if (m_otaWindow && m_otaWindow->handleFrame(frame))
        return;

    YwdProtocol::FrameType ft = YwdProtocol::classifyFrame(frame.id);
    if (ft != YwdProtocol::FT_FEEDBACK)
        logRawTx(frame, "RX");

    if (ft == YwdProtocol::FT_FEEDBACK) {
        // V0.2 §7.1: every feedback frame uses the same "header + N records"
        // layout, covering both single-node (N=1) and FB_AGG aggregate (N>1)
        // feedback. Feed each record into the same table / plot pipeline.
        ++m_fbCount;
        std::vector<FeedbackFrame> vec;
        if (m_proto.decodeFeedbackRecords(frame, vec)) {
            uint8_t node = 0, seq = 0;
            for (const auto &fb : vec) {
                m_lastFeedback[fb.motor_id] = fb;
                updateFeedbackTable(fb);
                m_plotPanel_->pushFeedback(fb);
                m_fbValid = true;
                node = fb.motor_id;
                seq  = fb.seq;
            }
            if (!vec.empty())
                updateFeedbackTableSeqCell(node, seq);
        }
    } else if (ft == YwdProtocol::FT_PARAM_RESP) {
        // Distinguish by B1 (N): single-register response (N ≤ 1, from the
        // register tab) vs block response (N > 1, from batch read, §8.2).
        const uint8_t n = (frame.len >= 2) ? frame.data[1] : 0;

        // Node-ID scan: a response to a REG_NODE_ID probe proves the CAN node
        // exists. Record it and stop early once the target count is reached.
        if (m_scanning && frame.len >= 7 && n >= 1 && frame.data[2] == REG_NODE_ID) {
            const uint8_t found = frame.id & 0x7F;
            if (!m_scanFound.contains(found)) {
                m_scanFound.append(found);
                logMessage(QString("[Scan] Node found: 0x%1 (%2/%3)")
                               .arg(found, 2, 16, QChar('0'))
                               .arg(m_scanFound.size())
                               .arg(kScanTargetCount));
                if (m_scanFound.size() >= kScanTargetCount) {
                    logMessage(QString("[Scan] Done — %1 nodes found: %2")
                                   .arg(m_scanFound.size())
                                   .arg(formatNodeList(m_scanFound)));
                    stopNodeScan();
                    applyNodeMap();
                }
            }
        }


        std::vector<ParamResponse> vec;
        if (m_proto.decodeRegBlockResponse(frame, vec)) {
            const uint8_t mid = frame.id & 0x7F;
            for (const auto &r : vec) {
                logMessage(
                    QString("RegBlk <- node 0x%1 addr=0x%2 val=0x%3 %4")
                        .arg(mid, 2, 16, QChar('0'))
                        .arg(r.addr, 2, 16, QChar('0'))
                        .arg(r.value, 8, 16, QChar('0'))
                        .arg(r.success ? "" : "FAIL"));
                m_proto.applyRegScaling(r.addr, r.value);

                if (!r.is_write) {
                    updateRegTableValue(mid, r.addr, r.rstat, r.value);
                    m_lblRegResult->setText(
                        QString("0x%1 = %2")
                            .arg(r.addr, 2, 16, QChar('0'))
                            .arg(formatRegValue(r.addr, r.value)));
                    m_lblRegResult->setStyleSheet("color: green; font-family: monospace;");
                    m_editRegValue->setText(formatRegValue(r.addr, r.value));
                } else {
                    m_lblRegResult->setText("Write OK");
                    m_lblRegResult->setStyleSheet("color: green; font-family: monospace;");
                }
            }

            // Advance the batch exactly ONCE per response frame (not once per
            // record). Compare against the *mapped* node ID (page → node), not
            // the page number, since node IDs may differ from 1..4 after the
            // connect-time scan. Doing this inside the record loop caused the
            // batch to jump several blocks ahead per response, so most
            // registers were never requested and the table stayed empty.
            if (m_batchWaiting
                && mid == nodeIdForIndex(m_batchMotorIdx)
                && !vec.empty()) {
                m_batchWaiting = false;
                m_batchRetries = 0;
                sendNextRegBlock();
            }
        }
    }
}

// ============================================================================
// Multi-motor feedback table update
// ============================================================================
void MainWindow::updateFeedbackTable(const FeedbackFrame &fb)
{
    int row = -1;
    for (int r = 0; r < m_feedbackTable->rowCount(); ++r) {
        auto *item = m_feedbackTable->item(r, 0);
        if (item && item->text() == QString("0x%1").arg(fb.motor_id, 2, 16, QChar('0')).toUpper()) {
            row = r;
            break;
        }
    }
    if (row < 0) {
        row = m_feedbackTable->rowCount();
        m_feedbackTable->insertRow(row);
    }

    auto setItem = [&](int col, const QString &text) {
        auto *it = m_feedbackTable->item(row, col);
        if (!it) {
            it = new QTableWidgetItem(text);
            m_feedbackTable->setItem(row, col, it);
        } else {
            it->setText(text);
        }
    };

    // State codes per protocol V0.2 §7.2
    static const char *stateNames[] = {
        "0:Disabled","1:Enabled","2:Fault","3:Calibrating" };
    const char *sn = (fb.state < 4) ? stateNames[fb.state] : "Reserved";

    // Fault codes per protocol V0.2 §7.3
    static const char *faultNames[] = {
        "No fault", "Overvoltage", "Undervoltage", "Overcurrent",
        "MOS overtemp", "Motor overtemp", "Comm lost (watchdog)",
        "Overload", "Encoder error", "Mode mismatch" };
    const char *fn = (fb.fault < 10) ? faultNames[fb.fault] : "Reserved";

    setItem(0, QString("0x%1").arg(fb.motor_id, 2, 16, QChar('0')).toUpper());
    setItem(1, QString::fromUtf8(sn));
    setItem(2, QString("0x%1").arg(fb.fault, 2, 16, QChar('0')).toUpper());
    if (auto *it = m_feedbackTable->item(row, 2))
        it->setToolTip(QString("Fault 0x%1: %2")
                           .arg(fb.fault, 2, 16, QChar('0'))
                           .arg(QString::fromUtf8(fn)));
    setItem(3, QString::number(fb.mode));
    setItem(4, QString::number(fb.position, 'f', 4));
    setItem(5, QString::number(fb.velocity, 'f', 4));
    setItem(6, QString::number(fb.torque,   'f', 4));
    setItem(7, QString::number(fb.voltage,  'f', 2));
    setItem(8, QString::number(fb.temp_mos)   + " °C");
    setItem(9, QString::number(fb.temp_motor) + " °C");
    setItem(10, QString::number(fb.node_count));   // header N (records in frame)
    setItem(11, QString::number(fb.seq));          // header SEQ (per feedback ID)

    for (int c = 0; c < m_feedbackTable->columnCount(); ++c) {
        auto *it = m_feedbackTable->item(row, c);
        if (it) {
            QColor bg = (fb.fault != 0) ? QColor(0x55, 0x22, 0x22)
                                         : QColor(0x23, 0x23, 0x23);
            it->setBackground(bg);
        }
    }
}

// ============================================================================
// Overwrite the SEQ cell of one motor's feedback row. In V0.2 SEQ lives in the
// feedback frame header (per feedback CAN ID), so an aggregate frame carries a
// single SEQ shared by all records; this keeps the newer value visible even
// when the same record content was already cached.
// ============================================================================
void MainWindow::updateFeedbackTableSeqCell(uint8_t node, uint8_t seq)
{
    for (int r = 0; r < m_feedbackTable->rowCount(); ++r) {
        auto *item = m_feedbackTable->item(r, 0);
        if (item && item->text() ==
                QString("0x%1").arg(node, 2, 16, QChar('0')).toUpper()) {
            auto *si = m_feedbackTable->item(r, 11);
            if (!si) {
                si = new QTableWidgetItem();
                m_feedbackTable->setItem(r, 11, si);
            }
            si->setText(QString::number(seq));
            return;
        }
    }
}

// ============================================================================
// Motor-page ↔ CAN node-ID mapping helpers
// ============================================================================
uint8_t MainWindow::nodeIdForIndex(int idx) const
{
    if (idx < 0 || idx >= kMaxMotors) return 0;
    return m_nodeIds[idx];   // 0 == unknown (page not bound to a node)
}

int MainWindow::motorIndexForNode(uint8_t node) const
{
    if (node == 0) return -1;
    for (int i = 0; i < kMaxMotors; ++i)
        if (m_nodeValid[i] && m_nodeIds[i] == node) return i;
    return -1;
}

// Enable/disable every control that belongs to one motor page. Pages whose
// node ID could not be discovered stay disabled so the user cannot command or
// read a non-existent node.
void MainWindow::setMotorPageEnabled(int idx, bool en)
{
    if (idx < 0 || idx >= kMaxMotors) return;

    // Control tabs: motor checkbox + its parameter edits.
    m_mitEn[idx]->setEnabled(en);
    for (QLineEdit *e : {m_mitPos[idx], m_mitVel[idx], m_mitKp[idx],
                         m_mitKd[idx], m_mitTff[idx]})
        e->setEnabled(en);
    m_pvEn[idx]->setEnabled(en);
    for (QLineEdit *e : {m_pvPos[idx], m_pvVelLim[idx], m_pvAcc[idx], m_pvDec[idx]})
        e->setEnabled(en);
    m_cvEn[idx]->setEnabled(en);
    for (QLineEdit *e : {m_cvVel[idx], m_cvAcc[idx], m_cvDec[idx]})
        e->setEnabled(en);

    // System tab motor checkbox.
    m_sysEn[idx]->setEnabled(en);

    // Register page: table interactions and "Read All".
    if (m_regTables[idx])      m_regTables[idx]->setEnabled(en);
    if (m_btnReadAllMotor[idx]) m_btnReadAllMotor[idx]->setEnabled(en);
    if (m_regMotorTabs)        m_regMotorTabs->setTabEnabled(idx, en);

    // Kick the motor out of any active mode if it just became invalid.
    if (!en && m_motorMode[idx] != MODE_NONE)
        m_motorMode[idx] = MODE_NONE;
}

// ============================================================================
// Register table per-motor value + RSTAT update
// Each motor page table: Addr(0)|Name(1)|Value(2)|RSTAT(3)
// ============================================================================
void MainWindow::updateRegTableValue(uint8_t motor_id, uint8_t addr, uint8_t rstat, uint32_t value)
{
    // Cache for later refreshes — map the real CAN node ID back to its page.
    int mi = motorIndexForNode(motor_id);
    if (mi < 0) return;
    m_regValues[mi][addr] = value;
    m_regValid[mi][addr]  = true;
    m_regRstat[mi][addr]  = rstat;

    // RSTAT decoded string per §8.3
    static const char *rstatNames[] = {
        "OK", "Unknown RID", "Read-only", "Out of range", "State forbid"
    };
    QString rstatStr;
    if (rstat < 5)
        rstatStr = QString("%1 (0x%2)").arg(rstatNames[rstat]).arg(rstat, 2, 16, QChar('0'));
    else
        rstatStr = QString("0x%1").arg(rstat, 2, 16, QChar('0'));

    // Determine decoded value string from the register catalogue (typed)
    const QString decoded = formatRegValue(addr, value);

    // Columns: 0=Addr  1=Name  2=Value  3=RSTAT
    static const int colVal   = 2;
    static const int colRstat = 3;

    QTableWidget *tbl = m_regTables[mi];

    // Update table row
    for (int r = 0; r < tbl->rowCount(); ++r) {
        auto *it = tbl->item(r, 0);
        if (!it) continue;
        bool ok;
        uint16_t rowAddr = it->text().toUInt(&ok, 16);
        if (ok && rowAddr == addr) {
            auto *vi = tbl->item(r, colVal);
            if (!vi) {
                vi = new QTableWidgetItem();
                tbl->setItem(r, colVal, vi);
            }
            vi->setText(decoded);

            auto *si = tbl->item(r, colRstat);
            if (!si) {
                si = new QTableWidgetItem();
                tbl->setItem(r, colRstat, si);
            }
            si->setText(rstatStr);

            // Color RSTAT: green for OK, red for errors
            if (rstat == 0x00)
                si->setForeground(QColor("#4caf50"));
            else
                si->setForeground(QColor("#ef5350"));
            return;
        }
    }
}
