#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QTextEdit>
#include <QTabWidget>
#include <QTableWidget>
#include <QGroupBox>
#include <QTimer>
#include <QElapsedTimer>
#include <QSplitter>
#include <QProgressBar>
#include <QCheckBox>

#include "canfd_device.h"
#include "ywd_protocol.h"
#include "ota_window.h"
#include "YwdPlotPanel.hpp"

// A motor can be commanded by exactly one control mode at a time.
// System-tab commands (Enable/Disable/...) are exempt from this rule.
enum MotorControlMode { MODE_NONE = 0, MODE_MIT = 1, MODE_PV = 2, MODE_CV = 3 };

// Maximum number of controllable motors (nodes 0x01..0x04) per protocol §11.
static constexpr int kMaxMotors = 4;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override = default;

private slots:
    // Device
    void onDeviceToggle();

    // MIT tab
    void onMitToggle();
    void onMitTick();

    // Pos-Vel tab
    void onPvToggle();
    void onPvTick();

    // Const Vel tab
    void onCvToggle();
    void onCvTick();

    // System tab
    void onSendSystemCmd(int cmdCode);

    // Register access
    void onRegRead();
    void onRegWrite();
    void onReadAllRegs(uint8_t motor_page);   // 1-based motor page (1..kMaxMotors)
    void onBatchReadNext();          // watchdog: retry/skip on response timeout

    // Batch read helpers (response-driven, per protocol §8)
    void sendNextRegBlock();
    void resendCurrentBlock();

    // Receive
    void onFrameReceived(const CanFdFrame &frame);

    // Poll / stats
    void onPollTimer();

    // Node-ID scan (connect-time discovery of active nodes)
    void startNodeScan();
    void stopNodeScan();
    void onNodeScanTick();
    void applyNodeMap();

    // OTA / IAP window
    void onOpenOtaWindow();

private:
    void setupUi();
    void buildMitTab();
    void buildPosVelTab();
    void buildConstVelTab();
    void buildSystemTab();
    // Same-motor checkbox mutual exclusion across the three control-mode tabs
    void setupModeCheckboxExclusion();
    void logMessage(const QString &msg);
    void logRawTx(const CanFdFrame &frame, const QString &dir);
    void updateFeedbackTable(const FeedbackFrame &fb);
    void updateFeedbackTableSeqCell(uint8_t node, uint8_t seq);
    void updateRegTableValue(uint8_t motor_id, uint8_t addr, uint8_t rstat, uint32_t value);

    // Stop all per-tab periodic send timers
    void stopAllTimers();

    // Helper: horizontal row of "Label | [e0] [e1] ..." (one column per motor)
    QWidget *makeLineRow(const QString &label,
                         QLineEdit *const *edits, int n);

    // Helper: derive diagram update interval from the active send timers
    void updateDiagramTiming();

    // Helpers for the one-control-mode-per-motor rule
    bool    modeHasMotors(int mode) const;
    QString modeMotorList(int mode) const;
    void    refreshModeLabels();

    // Typed register value helpers — type from RegInfo::data_type
    QString formatRegValue(uint8_t addr, uint32_t value) const;
    bool    parseRegValue(uint8_t addr, const QString &text,
                          uint32_t &value, QString &err) const;

    // Helper: create a standard decimal line edit for a send parameter
    static QLineEdit *makeLineEdit(const QString &def = "0", int width = 78);

    // Parse a send-parameter line edit; returns false on empty/invalid text
    static bool   parseLineEdit(QLineEdit *le, double &out);
    // Same, but fall back to `fallback` when the text is empty/invalid
    static double lineEditValue(QLineEdit *le, double fallback);
    // Validate the n line edits of a motor; warn and return false if any is invalid
    bool validateSendInputs(QLineEdit *const edits[], int n, int motorIdx) const;

    static QString hexStr(uint32_t val, int digits);

    // --- Device ---
    CanFdDevice *m_device;
    bool         m_connected = false;
    QComboBox   *m_cmbDevice;
    QPushButton *m_btnConnect;
    QLabel      *m_lblDevStatus;

    // --- Command tabs ---
    QTabWidget  *m_cmdTabs;

    // MIT tab — 4 motors: {0x01, 0x02, 0x03, 0x04}
    QCheckBox *m_mitEn[kMaxMotors];
    QLineEdit *m_mitPos[kMaxMotors], *m_mitVel[kMaxMotors], *m_mitKp[kMaxMotors], *m_mitKd[kMaxMotors], *m_mitTff[kMaxMotors];
    QPushButton    *m_btnMitToggle;
    QSpinBox       *m_spinMitInterval;
    QCheckBox      *m_mitSync = nullptr;   // replicate motor 1 data to all
    QLabel         *m_lblMitStatus;
    QTimer         *m_mitTimer;
    bool            m_mitRunning = false;

    // Pos-Vel tab — 4 motors
    QCheckBox *m_pvEn[kMaxMotors];
    QLineEdit *m_pvPos[kMaxMotors], *m_pvVelLim[kMaxMotors], *m_pvAcc[kMaxMotors], *m_pvDec[kMaxMotors];
    QPushButton    *m_btnPvToggle;
    QSpinBox       *m_spinPvInterval;
    QCheckBox      *m_pvSync = nullptr;    // replicate motor 1 data to all
    QLabel         *m_lblPvStatus;
    QTimer         *m_pvTimer;
    bool            m_pvRunning = false;

    // Const Vel tab — 4 motors
    QCheckBox *m_cvEn[kMaxMotors];
    QLineEdit *m_cvVel[kMaxMotors], *m_cvAcc[kMaxMotors], *m_cvDec[kMaxMotors];
    QPushButton    *m_btnCvToggle;
    QSpinBox       *m_spinCvInterval;
    QCheckBox      *m_cvSync = nullptr;    // replicate motor 1 data to all
    QLabel         *m_lblCvStatus;
    QTimer         *m_cvTimer;
    bool            m_cvRunning = false;

    // --- One control mode per motor ---
    // Ownership of each motor (0x01..0x04): MODE_NONE / MODE_MIT / MODE_PV / MODE_CV.
    // System-tab commands are exempt from this rule.
    int m_motorMode[kMaxMotors] = {MODE_NONE, MODE_NONE, MODE_NONE, MODE_NONE};

    // System tab — 4 motor checkboxes
    QCheckBox   *m_sysEn[kMaxMotors];
    QPushButton *m_btnSysEnable, *m_btnSysDisable, *m_btnSysSetZero;
    QPushButton *m_btnSysClearFault, *m_btnSysSave, *m_btnSysReset;
    QPushButton *m_btnSysLoadDefaults;

    // --- Multi-motor feedback table ---
    QTableWidget *m_feedbackTable;
    QMap<uint8_t, FeedbackFrame> m_lastFeedback;

    // --- Register access ---
    QSpinBox    *m_regMotorId;
    QSpinBox    *m_spinRegAddr;
    QLineEdit   *m_editRegValue;
    QPushButton *m_btnRegRead;
    QPushButton *m_btnRegWrite;
    QLabel      *m_lblRegResult;

    // Motor pages in register tab widget (4 motors)
    QTabWidget   *m_regMotorTabs;
    QTableWidget *m_regTables[kMaxMotors];       // one table per motor page
    QPushButton  *m_btnReadAllMotor[kMaxMotors]; // "Read All" per motor page
    QLabel       *m_lblReadAllMotor[kMaxMotors]; // status per motor page

    QVector<RegInfo> m_allRegs;

    // Per-motor register values
    uint32_t m_regValues[kMaxMotors][256];
    bool     m_regValid[kMaxMotors][256];
    uint8_t  m_regRstat[kMaxMotors][256];

    int     m_regBatchIdx = 0;
    int     m_batchMotorIdx = 0;    // which motor the current batch is reading
    int     m_batchBlockStart = 0;  // index of first RID in the pending block
    int     m_batchPendingN = 0;    // # of RIDs in the pending block (≤ 8)
    int     m_batchRetries = 0;     // response-timeout retries for current block
    bool    m_batchWaiting = false; // true while a block response is pending
    QElapsedTimer m_batchElapsed;   // monotonic clock since current block was sent
    QTimer *m_batchReadTimer;

    // --- Node-ID scan (connect-time discovery) ---
    // Probes REG_NODE_ID (0x00) on every candidate CAN node ID 1..127. A device
    // replies only when its own node ID matches the CAN ID, so each valid
    // response proves that node exists. Scan ends when 4 nodes are found or all
    // candidates have been probed; each probe waits kScanProbeTimeoutMs.
    static constexpr int kScanProbeTimeoutMs = 50;   // 0.05 s per probe
    static constexpr int kScanTargetCount    = 4;    // stop early after 4 hits
    static constexpr int kScanMaxNode        = 0x7F; // candidate IDs 1..127
    bool          m_scanning = false;
    int           m_scanNode = 0x01;       // current probe CAN node ID
    QTimer       *m_scanTimer = nullptr;   // probes once per timeout
    QElapsedTimer m_scanElapsed;           // since the current probe was sent
    QVector<uint8_t> m_scanFound;          // discovered node IDs (in order)

    // Motor-page → CAN node-ID mapping. The UI tabs keep the fixed names
    // "Motor 1".."Motor 4", but each page talks to whatever node ID the scan
    // discovered (node IDs are reassignable). Index 0..kMaxMotors-1.
    // A page with no discovered node keeps m_nodeIds == 0 (unknown) and
    // m_nodeValid == false; all of its related controls are disabled.
    uint8_t  m_nodeIds[kMaxMotors] = {0, 0, 0, 0};
    bool     m_nodeValid[kMaxMotors] = {false, false, false, false};
    // Reverse lookup: CAN node ID → motor-page index, or -1 if unmapped.
    int      motorIndexForNode(uint8_t node) const;
    // Node ID to use for a motor-page index; 0 if the page has no known node.
    uint8_t  nodeIdForIndex(int idx) const;
    // True if the page has a discovered (valid) node ID.
    bool     isNodeValid(int idx) const
    { return idx >= 0 && idx < kMaxMotors && m_nodeValid[idx]; }
    // Enable/disable every control tied to one motor page.
    void     setMotorPageEnabled(int idx, bool enabled);

    // --- Feedback / Log switchable bottom panel ---
    QTabWidget *m_bottomTabs = nullptr;

    // --- Log ---
    QCheckBox *m_chkShowRaw = nullptr;   // toggle raw CAN-FD frame dumps
    QTextEdit *m_logView;

    // --- Plot ---
    YwdPlotPanel *m_plotPanel_;
    QTimer       *m_diagramTimer_;

    // --- Status bar ---
    QLabel  *m_lblRxStats = nullptr;
    quint64  m_rxCount    = 0;
    quint64  m_fbCount    = 0;
    quint64  m_lastFbCount = 0;
    int      m_pollTicks  = 0;

    // Periodic poll
    QTimer   *m_pollTimer;

    // Protocol
    YwdProtocol m_proto;

    // --- OTA / IAP window ---
    QPushButton *m_btnOta = nullptr;         // "OTA" button on the connection row
    OtaWindow   *m_otaWindow = nullptr;      // lazily created separate window

    // Latest feedback for UI update
    bool m_fbValid = false;
};

#endif // MAINWINDOW_H
