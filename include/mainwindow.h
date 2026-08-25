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

#include "canfd_device.h"
#include "ywd_protocol.h"
#include "YwdPlotPanel.hpp"

// A motor can be commanded by exactly one control mode at a time.
// System-tab commands (Enable/Disable/...) are exempt from this rule.
enum MotorControlMode { MODE_NONE = 0, MODE_MIT = 1, MODE_PV = 2, MODE_CV = 3 };

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
    void onReadAllRegs(uint8_t motor_id);
    void onBatchReadNext();          // watchdog: retry/skip on response timeout

    // Batch read helpers (response-driven, per protocol §8)
    void sendNextRegBlock();
    void resendCurrentBlock();

    // Receive
    void onFrameReceived(const CanFdFrame &frame);

    // Poll / stats
    void onPollTimer();

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
    void updateRegTableValue(uint8_t motor_id, uint8_t addr, uint8_t rstat, uint32_t value);

    // Stop all per-tab periodic send timers
    void stopAllTimers();

    // Helper: horizontal row of "Label | [s0] [s1] [s2]"
    QWidget *makeSpinRow(const QString &label,
                         QDoubleSpinBox *s0, QDoubleSpinBox *s1, QDoubleSpinBox *s2);

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

    // Helper: create a standard double spinbox
    static QDoubleSpinBox *makeDblSpin(double min, double max, int decimals, double val);

    static QString hexStr(uint32_t val, int digits);

    // --- Device ---
    CanFdDevice *m_device;
    bool         m_connected = false;
    QComboBox   *m_cmbDevice;
    QPushButton *m_btnConnect;
    QLabel      *m_lblDevStatus;

    // --- Command tabs ---
    QTabWidget  *m_cmdTabs;

    // MIT tab — 3 motors: {0x01, 0x02, 0x03}
    QCheckBox      *m_mitEn[3];
    QDoubleSpinBox *m_mitPos[3], *m_mitVel[3], *m_mitKp[3], *m_mitKd[3], *m_mitTff[3];
    QPushButton    *m_btnMitToggle;
    QSpinBox       *m_spinMitInterval;
    QLabel         *m_lblMitStatus;
    QTimer         *m_mitTimer;
    bool            m_mitRunning = false;

    // Pos-Vel tab — 3 motors
    QCheckBox      *m_pvEn[3];
    QDoubleSpinBox *m_pvPos[3], *m_pvVelLim[3];
    QPushButton    *m_btnPvToggle;
    QSpinBox       *m_spinPvInterval;
    QLabel         *m_lblPvStatus;
    QTimer         *m_pvTimer;
    bool            m_pvRunning = false;

    // Const Vel tab — 3 motors
    QCheckBox      *m_cvEn[3];
    QDoubleSpinBox *m_cvVel[3];
    QPushButton    *m_btnCvToggle;
    QSpinBox       *m_spinCvInterval;
    QLabel         *m_lblCvStatus;
    QTimer         *m_cvTimer;
    bool            m_cvRunning = false;

    // --- One control mode per motor ---
    // Ownership of each motor (0x01..0x03): MODE_NONE / MODE_MIT / MODE_PV / MODE_CV.
    // System-tab commands are exempt from this rule.
    int m_motorMode[3] = {MODE_NONE, MODE_NONE, MODE_NONE};

    // System tab — 3 motor checkboxes
    QCheckBox   *m_sysEn[3];
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

    // Motor pages in register tab widget (3 motors)
    QTabWidget   *m_regMotorTabs;
    QTableWidget *m_regTables[3];       // one table per motor page
    QPushButton  *m_btnReadAllMotor[3]; // "Read All" per motor page
    QLabel       *m_lblReadAllMotor[3]; // status per motor page

    QVector<RegInfo> m_allRegs;

    // Per-motor register values
    uint32_t m_regValues[3][256];
    bool     m_regValid[3][256];
    uint8_t  m_regRstat[3][256];

    int     m_regBatchIdx = 0;
    int     m_batchMotorIdx = 0;    // which motor the current batch is reading
    int     m_batchBlockStart = 0;  // index of first RID in the pending block
    int     m_batchPendingN = 0;    // # of RIDs in the pending block (≤ 8)
    int     m_batchRetries = 0;     // response-timeout retries for current block
    bool    m_batchWaiting = false; // true while a block response is pending
    QElapsedTimer m_batchElapsed;   // monotonic clock since current block was sent
    QTimer *m_batchReadTimer;

    // --- Log ---
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

    // Latest feedback for UI update
    bool m_fbValid = false;
};

#endif // MAINWINDOW_H
