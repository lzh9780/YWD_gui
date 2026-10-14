#ifndef OTA_WINDOW_H
#define OTA_WINDOW_H

// ============================================================================
// OtaWindow — standalone firmware-upgrade window (protocol v2, §1/§3).
// Owns the OTA UI and the OtaClient state machine. The parent MainWindow
// forwards every received CAN frame via handleFrame() and supplies the
// sendFrame callback used to transmit OTA frames onto the bus.
// ============================================================================

#include <QDialog>
#include <QTimer>
#include <QString>

#include "canfd_device.h"
#include "ota_client.h"

class QLabel;
class QLineEdit;
class QSpinBox;
class QPushButton;
class QProgressBar;

class OtaWindow : public QDialog
{
    Q_OBJECT

public:
    explicit OtaWindow(QWidget *parent = nullptr);

    // Target node shown / used for the session.
    void setTargetNode(quint8 node);
    quint8 targetNode() const;

    // Whether the CAN device is currently connected (drives UI state).
    void setConnected(bool connected);

    // Route a received frame: consumed when it is an OTA uplink frame.
    bool handleFrame(const CanFdFrame &frame);

    // Anchor the OTA window next to the main window on first show.
    void setAnchorGeometry(const QRect &anchor);

signals:
    // Request the parent to transmit a frame onto the bus.
    void sendFrameRequested(const CanFdFrame &frame);
    // Human-readable log line (forwarded to the main window's log).
    void logMessage(const QString &msg);

protected:
    void showEvent(QShowEvent *event) override;

private slots:
    void onBrowse();
    void onStartStop();
    void onTick();

private:
    void buildUi();
    void refreshUi();
    void log(const QString &msg) { emit logMessage(msg); }

    OtaClient    m_ota;
    QTimer      *m_timer = nullptr;
    bool         m_connected = false;
    bool         m_anchored = false;
    QRect        m_anchor;

    QLabel      *m_devStatus = nullptr;
    QLabel      *m_boardId = nullptr;
    QLabel      *m_hardVer = nullptr;
    QLabel      *m_softVer = nullptr;
    QLabel      *m_bootVer = nullptr;
    QLabel      *m_compileDate = nullptr;
    QLineEdit   *m_filePath = nullptr;
    QLabel      *m_fileSize = nullptr;
    QLabel      *m_firmBoardCode = nullptr;
    QLabel      *m_firmCrc = nullptr;
    QSpinBox    *m_node = nullptr;
    QPushButton *m_browseBtn = nullptr;
    QPushButton *m_startBtn = nullptr;
    QProgressBar*m_progress = nullptr;
    QLabel      *m_progressText = nullptr;
    QLabel      *m_msg = nullptr;
};

#endif // OTA_WINDOW_H
