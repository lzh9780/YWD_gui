#include "canfd_device.h"
#include <cstring>

extern "C" {
#include "zcan.h"
}

// Device parameters
static constexpr U32  DEV_TYPE   = 33;     // ZCAN
static constexpr U32  DEV_IDX    = 0;
static constexpr U32  DEV_CHN    = 0;
static constexpr U32  CLK_HZ     = 60000000; // 60 MHz

// Arbitration: 1 Mbps
static constexpr U8   ARB_BRP    = 2;
static constexpr U8   ARB_TSEG1  = 14;
static constexpr U8   ARB_TSEG2  = 3;
static constexpr U8   ARB_SJW    = 3;

// FD Data: 5 Mbps
static constexpr U8   DATA_BRP   = 2;
static constexpr U8   DATA_TSEG1 = 1;
static constexpr U8   DATA_TSEG2 = 0;
static constexpr U8   DATA_SJW   = 0;

// ---------------------------------------------------------------------------
CanFdDevice::CanFdDevice(QObject *parent)
    : QObject(parent)
    , m_opened(false)
    , m_rxTimer(new QTimer(this))
{
    connect(m_rxTimer, &QTimer::timeout, this, &CanFdDevice::pollFrames);
}

CanFdDevice::~CanFdDevice()
{
    close();
}

// ---------------------------------------------------------------------------
bool CanFdDevice::open()
{
    if (m_opened) return true;

    if (VCI_OpenDevice(DEV_TYPE, DEV_IDX, 0) != 1) {
        m_lastError = QString("VCI_OpenDevice failed (type=%1 idx=%2)")
                          .arg(DEV_TYPE).arg(DEV_IDX);
        qWarning() << m_lastError;
        return false;
    }

    if (!configureDevice()) {
        VCI_CloseDevice(DEV_TYPE, DEV_IDX);
        return false;
    }

    if (VCI_StartCAN(DEV_TYPE, DEV_IDX, DEV_CHN) != 1) {
        m_lastError = "VCI_StartCAN failed";
        VCI_CloseDevice(DEV_TYPE, DEV_IDX);
        return false;
    }

    m_opened = true;
    emit deviceStatusChanged(true);
    return true;
}

void CanFdDevice::close()
{
    stopReceive();
    if (m_opened) {
        VCI_CloseDevice(DEV_TYPE, DEV_IDX);
        m_opened = false;
        emit deviceStatusChanged(false);
    }
}

// ---------------------------------------------------------------------------
bool CanFdDevice::configureDevice()
{
    ZCAN_INIT init{};
    init.clk  = CLK_HZ;
    init.mode = 0;   // bit0=0 normal mode, bit1=0 ISO CAN-FD

    // Arbitration phase
    init.aset.brp   = ARB_BRP;
    init.aset.tseg1 = ARB_TSEG1;
    init.aset.tseg2 = ARB_TSEG2;
    init.aset.sjw   = ARB_SJW;
    init.aset.smp   = 1;   // triple sampling

    // FD Data phase
    init.dset.brp   = DATA_BRP;
    init.dset.tseg1 = DATA_TSEG1;
    init.dset.tseg2 = DATA_TSEG2;
    init.dset.sjw   = DATA_SJW;
    init.dset.smp   = 0;   // single sampling

    if (VCI_InitCAN(DEV_TYPE, DEV_IDX, DEV_CHN, &init) != 1) {
        m_lastError = "VCI_InitCAN failed: cannot initialize CAN controller";
        qWarning() << m_lastError;
        return false;
    }

    // Configure filter: accept all standard frames (11-bit ID)
    // ZCAN_FILTER_TABLE filter{};
    // filter.size       = 1;
    // filter.table[0].type = 0;        // standard frame
    // filter.table[0].sid  = 0;
    // filter.table[0].eid  = 0x7FF;    // accept all 11-bit IDs

    uint32_t filter = 1;
    if (VCI_SetReference(DEV_TYPE, DEV_IDX, DEV_CHN, CMD_CAN_TRES, &filter) != 1) {
        // Filter failure is not fatal on some devices; we log and continue
        qDebug() << "Warning: VCI_SetReference filter returned error, continuing...";
    }

    return true;
}

// ---------------------------------------------------------------------------
bool CanFdDevice::sendFrame(const CanFdFrame &frame)
{
    if (!m_opened) return false;

    if (frame.len > 64) {
        m_lastError = QString("sendFrame: invalid length %1 (max 64)").arg(frame.len);
        return false;
    }

    ZCAN_FD_MSG msg{};
    memset(&msg, 0, sizeof(msg));

    msg.hdr.id          = frame.id;
    msg.hdr.inf.fmt     = frame.is_fd ? 1U : 0U;    // CANFD
    msg.hdr.inf.sef     = frame.is_ext_id ? 1U : 0U; // extended frame
    msg.hdr.inf.sdf     = 0;   // data frame
    msg.hdr.inf.txm     = ZCAN_TX_NORM;
    msg.hdr.inf.brs     = frame.brs ? 1U : 0U;       // bit rate switch
    msg.hdr.chn         = DEV_CHN;   // channel
    msg.hdr.len         = static_cast<uint8_t>(frame.len);  // actual byte length

    memcpy(msg.dat, frame.data, std::min(sizeof(msg.dat), sizeof(frame.data)));

    U32 ret = VCI_TransmitFD(DEV_TYPE, DEV_IDX, DEV_CHN, &msg, 1);
    if (ret != 1) {
        m_lastError = QString("VCI_TransmitFD failed: ret=%1").arg(ret);
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
void CanFdDevice::startReceive()
{
    if (m_opened && !m_rxTimer->isActive()) {
        m_rxTimer->start(5);   // 5 ms polling = 200 Hz
    }
}

void CanFdDevice::stopReceive()
{
    if (m_rxTimer->isActive())
        m_rxTimer->stop();
}

void CanFdDevice::pollFrames()
{
    if (!m_opened) return;

    ZCAN_FD_MSG rxBuf[10];
    memset(rxBuf, 0, sizeof(rxBuf));

    U32 count = VCI_ReceiveFD(DEV_TYPE, DEV_IDX, DEV_CHN, rxBuf, 10, 0);
    for (U32 i = 0; i < count; ++i) {
        const ZCAN_FD_MSG &raw = rxBuf[i];

        CanFdFrame frame{};
        frame.id         = raw.hdr.id;
        frame.is_ext_id  = (raw.hdr.inf.sef != 0);
        frame.is_fd      = (raw.hdr.inf.fmt != 0);
        frame.brs        = (raw.hdr.inf.brs != 0);

        // hdr.len already carries the actual byte length (driver decodes DLC)
        frame.len = raw.hdr.len;

        memcpy(frame.data, raw.dat, std::min(64, (int)frame.len));

        emit frameReceived(frame);
    }
}
