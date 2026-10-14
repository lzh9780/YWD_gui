#include "ota_window.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QPushButton>
#include <QProgressBar>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QShowEvent>

// ============================================================================
// Construction
// ============================================================================
OtaWindow::OtaWindow(QWidget *parent)
    : QDialog(parent)
    , m_timer(new QTimer(this))
{
    setWindowTitle("OTA Firmware Upgrade");
    setWindowFlags(windowFlags() | Qt::Window);
    buildUi();

    connect(m_timer, &QTimer::timeout, this, &OtaWindow::onTick);
    m_timer->start(20);   // ~20 ms, matches the reference WinForms app
}

void OtaWindow::buildUi()
{
    auto *vl = new QVBoxLayout(this);
    vl->setContentsMargins(10, 10, 10, 10);
    vl->setSpacing(8);

    // ---- Device info ----
    {
        auto *grp = new QGroupBox("Device Info");
        auto *g = new QGridLayout(grp);
        g->setContentsMargins(6, 4, 6, 4);
        g->setHorizontalSpacing(10);
        g->setVerticalSpacing(3);

        auto addField = [&](int row, int col, const QString &label, QLabel *&out) {
            g->addWidget(new QLabel(label), row, col * 2, Qt::AlignRight);
            out = new QLabel("--");
            out->setStyleSheet("font-weight: bold; font-family: monospace;");
            g->addWidget(out, row, col * 2 + 1);
        };

        addField(0, 0, "Status:",        m_devStatus);
        addField(0, 1, "Board ID:",      m_boardId);
        addField(1, 0, "Hardware Ver:",  m_hardVer);
        addField(1, 1, "Software Ver:",  m_softVer);
        addField(2, 0, "Boot Ver:",      m_bootVer);
        addField(2, 1, "Compile Date:",  m_compileDate);
        g->setColumnStretch(3, 1);
        vl->addWidget(grp);
    }

    // ---- Firmware info ----
    {
        auto *grp = new QGroupBox("Firmware Info");
        auto *box = new QVBoxLayout(grp);
        box->setContentsMargins(6, 4, 6, 4);
        box->setSpacing(4);

        auto *row = new QHBoxLayout();
        row->addWidget(new QLabel("File:"));
        m_filePath = new QLineEdit("No firmware selected");
        m_filePath->setReadOnly(true);
        m_filePath->setToolTip("Select a .bin firmware file");
        row->addWidget(m_filePath, 1);
        m_browseBtn = new QPushButton("Browse");
        m_browseBtn->setFixedWidth(90);
        connect(m_browseBtn, &QPushButton::clicked, this, &OtaWindow::onBrowse);
        row->addWidget(m_browseBtn);
        box->addLayout(row);

        auto *row2 = new QHBoxLayout();
        row2->addWidget(new QLabel("Size:"));
        m_fileSize = new QLabel("--");
        m_fileSize->setStyleSheet("color:#1F6FB2; font-weight:bold;");
        row2->addWidget(m_fileSize);
        row2->addWidget(new QLabel("  board_code:"));
        m_firmBoardCode = new QLabel("--");
        m_firmBoardCode->setStyleSheet("color:#1F6FB2; font-weight:bold;");
        row2->addWidget(m_firmBoardCode);
        row2->addWidget(new QLabel("  CRC16:"));
        m_firmCrc = new QLabel("--");
        m_firmCrc->setStyleSheet("color:#1F6FB2; font-weight:bold;");
        row2->addWidget(m_firmCrc);
        row2->addStretch();
        box->addLayout(row2);
        vl->addWidget(grp);
    }

    // ---- Upgrade operation ----
    {
        auto *grp = new QGroupBox("Firmware Upgrade");
        auto *box = new QVBoxLayout(grp);
        box->setContentsMargins(6, 4, 6, 4);
        box->setSpacing(6);

        auto *row = new QHBoxLayout();
        row->addWidget(new QLabel("Target Node:"));
        m_node = new QSpinBox();
        m_node->setRange(1, 0x7F);
        m_node->setPrefix("0x");
        m_node->setDisplayIntegerBase(16);
        m_node->setValue(0x01);
        m_node->setFixedWidth(80);
        row->addWidget(m_node);
        row->addStretch();
        m_msg = new QLabel("");
        m_msg->setStyleSheet("color:#888; font-size:9px;");
        row->addWidget(m_msg);
        box->addLayout(row);

        m_progress = new QProgressBar();
        m_progress->setRange(0, 100);
        m_progress->setValue(0);
        m_progress->setFixedHeight(14);
        box->addWidget(m_progress);

        m_progressText = new QLabel("Progress 0%");
        m_progressText->setStyleSheet("color:#888; font-size:9px;");
        box->addWidget(m_progressText);

        m_startBtn = new QPushButton("Start Upgrade");
        m_startBtn->setFixedHeight(30);
        m_startBtn->setStyleSheet(
            "QPushButton { background:#1F6FB2; color:white; font-weight:bold; }");
        connect(m_startBtn, &QPushButton::clicked, this, &OtaWindow::onStartStop);
        box->addWidget(m_startBtn);

        vl->addWidget(grp);
    }

    vl->addStretch();
    resize(420, 420);
}

// ============================================================================
// External hooks
// ============================================================================
void OtaWindow::setTargetNode(quint8 node)
{
    m_node->setValue(node);
}

quint8 OtaWindow::targetNode() const
{
    return static_cast<quint8>(m_node->value());
}

void OtaWindow::setConnected(bool connected)
{
    m_connected = connected;
    refreshUi();
}

bool OtaWindow::handleFrame(const CanFdFrame &frame)
{
    if (!m_ota.handleFrame(frame))
        return false;
    emit logMessage(QString("[OTA] RX id=0x%1 stas=0x%2")
                        .arg(frame.id, 8, 16, QChar('0'))
                        .arg(frame.len >= 2 ? frame.data[1] : 0, 2, 16, QChar('0')));
    refreshUi();
    return true;
}

void OtaWindow::setAnchorGeometry(const QRect &anchor)
{
    m_anchor = anchor;
}

void OtaWindow::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    if (!m_anchored && m_anchor.isValid()) {
        m_anchored = true;
        move(m_anchor.right() + 12, m_anchor.top());
    }
    raise();
    activateWindow();
}

// ============================================================================
// Slots
// ============================================================================
void OtaWindow::onBrowse()
{
    if (m_ota.isRunning()) {
        QMessageBox::information(this, "OTA", "Upgrade in progress; cannot change firmware.");
        return;
    }
    const QString path = QFileDialog::getOpenFileName(
        this, "Select Firmware File", QString(),
        "Firmware (*.bin);;All Files (*.*)");
    if (path.isEmpty()) return;

    QString err;
    if (!m_ota.loadFirmware(path, err)) {
        QMessageBox::warning(this, "Firmware Parse Failed", err);
        return;
    }

    m_filePath->setText(path);
    const QFileInfo fi(path);
    m_fileSize->setText(fi.size() >= 1024
                            ? QString("%1 KB").arg(fi.size() / 1024.0, 0, 'f', 1)
                            : QString("%1 B").arg(fi.size()));
    m_firmBoardCode->setText(
        QString("0x%1").arg(m_ota.firmwareBoardCode(), 8, 16, QChar('0')).toUpper());
    m_firmCrc->setText(
        QString("0x%1").arg(m_ota.firmwareCrc(), 4, 16, QChar('0')).toUpper());
    log(QString("[OTA] Firmware loaded %1  len=%2B board_code=0x%3 CRC16=0x%4")
            .arg(fi.fileName())
            .arg(m_ota.firmwareLength())
            .arg(m_ota.firmwareBoardCode(), 8, 16, QChar('0'))
            .arg(m_ota.firmwareCrc(), 4, 16, QChar('0')));
}

void OtaWindow::onStartStop()
{
    // Second click = finish/cancel the upgrade
    if (m_ota.isRunning()) {
        m_ota.stop();
        m_startBtn->setText("Start Upgrade");
        m_startBtn->setStyleSheet(
            "QPushButton { background:#1F6FB2; color:white; font-weight:bold; }");
        m_browseBtn->setEnabled(true);
        m_node->setEnabled(true);
        m_msg->setStyleSheet("color:#c62828; font-size:9px;");
        m_msg->setText("Upgrade stopped");
        return;
    }

    if (!m_connected) {
        QMessageBox::warning(this, "OTA", "Device not connected; please connect the CAN device first.");
        return;
    }
    if (m_ota.firmwareLength() == 0) {
        QMessageBox::warning(this, "OTA", "Please select a firmware file first.");
        return;
    }

    m_ota.start(static_cast<quint8>(m_node->value()));
    m_startBtn->setText("Stop Upgrade");
    m_startBtn->setStyleSheet(
        "QPushButton { background:#c62828; color:white; font-weight:bold; }");
    m_browseBtn->setEnabled(false);
    m_node->setEnabled(false);
    m_msg->setStyleSheet("color:#c62828; font-size:9px;");
    m_msg->setText("Upgrading...");
    log(QString("[OTA] Start upgrade target node=0x%1")
            .arg(m_node->value(), 2, 16, QChar('0')));
}

void OtaWindow::onTick()
{
    // Board-code mismatch → abort and report (checked first: terminal)
    if (m_ota.takeBoardCodeMismatch()) {
        m_startBtn->setText("Start Upgrade");
        m_startBtn->setStyleSheet(
            "QPushButton { background:#1F6FB2; color:white; font-weight:bold; }");
        m_browseBtn->setEnabled(true);
        m_node->setEnabled(true);
        m_msg->setStyleSheet("color:#c62828; font-size:9px;");
        m_msg->setText(QString("Aborted: board_code mismatch (fw=0x%1 dev=0x%2)")
                           .arg(m_ota.firmwareBoardCode() & 0xFF, 2, 16, QChar('0'))
                           .arg(m_ota.devBoardId(), 2, 16, QChar('0')));
        log("[OTA] Aborted: board_code mismatch");
        refreshUi();
        return;
    }

    if (m_connected && m_ota.isRunning()) {
        CanFdFrame f = m_ota.tick();
        if (f.len > 0)
            emit sendFrameRequested(f);

        if (m_ota.state() == OtaClient::ST_END) {
            m_ota.stop();
            m_startBtn->setText("Start Upgrade");
            m_startBtn->setStyleSheet(
                "QPushButton { background:#1F6FB2; color:white; font-weight:bold; }");
            m_browseBtn->setEnabled(true);
            m_node->setEnabled(true);
            m_msg->setStyleSheet("color:#2e7d32; font-size:9px;");
            m_msg->setText("Upgrade complete!");
            log("[OTA] Upgrade complete");
        }
    }
    refreshUi();
}

void OtaWindow::refreshUi()
{
    const bool online = m_ota.deviceOnline();
    const bool inBoot = m_ota.isRunning() && m_ota.state() > OtaClient::ST_START;

    if (inBoot) {
        m_devStatus->setText("● Boot Mode");
        m_devStatus->setStyleSheet("color:#f57c00; font-weight:bold;");
    } else if (online) {
        m_devStatus->setText("● Online");
        m_devStatus->setStyleSheet("color:#2e7d32; font-weight:bold;");
    } else if (m_connected) {
        m_devStatus->setText("● Connected (APP Mode)");
        m_devStatus->setStyleSheet("color:#1565c0; font-weight:bold;");
    } else {
        m_devStatus->setText("○ Not Connected");
        m_devStatus->setStyleSheet("color:#888; font-weight:bold;");
    }

    if (online) {
        m_boardId->setText(
            QString("0x%1").arg(m_ota.devBoardId(), 2, 16, QChar('0')).toUpper());
        m_hardVer->setText(QString::number(m_ota.devHardVer() / 10.0, 'f', 1));
        m_softVer->setText(QString::number(m_ota.devSoftVer() / 10.0, 'f', 1));
        m_bootVer->setText(QString::number(m_ota.devBootVer()));
        const ushort packed = m_ota.devCompilePacked();
        m_compileDate->setText(
            packed != 0
                ? QString("%1-%2-%3")
                      .arg(m_ota.devCompileYear())
                      .arg(m_ota.devCompileMonth(), 2, 10, QChar('0'))
                      .arg(m_ota.devCompileDay(), 2, 10, QChar('0'))
                : QString("--"));
    }

    const quint32 pct = m_ota.progressPercent();
    m_progress->setValue(static_cast<int>(pct));
    m_progressText->setText(QString("Progress %1%").arg(pct));
}
