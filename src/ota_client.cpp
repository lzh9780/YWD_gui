#include "ota_client.h"

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <cstring>

// ---------------------------------------------------------------------------
// CRC16-MODBUS (poly 0xA001) over data[offset .. offset+len)  (protocol §3.1)
// ---------------------------------------------------------------------------
ushort OtaClient::crc16Modbus(const QByteArray &data, int offset, int len)
{
    ushort crc = 0xFFFF;
    const auto *p = reinterpret_cast<const uint8_t *>(data.constData());
    for (int i = offset; i < offset + len; ++i) {
        crc ^= static_cast<ushort>(p[i] & 0xFF);
        for (int j = 0; j < 8; ++j)
            crc = (crc & 1) ? static_cast<ushort>((crc >> 1) ^ 0xA001)
                            : static_cast<ushort>(crc >> 1);
    }
    return crc;
}

OtaClient::OtaClient() {}

void OtaClient::log(const QString &msg)
{
    // Logging is routed by the UI via its own log; here it is a no-op hook.
    Q_UNUSED(msg);
}

// ---------------------------------------------------------------------------
// Firmware loading: read .bin, append 8-byte trailer [len32][crc16][0][0]
// ---------------------------------------------------------------------------
bool OtaClient::loadFirmware(const QString &path, QString &err)
{
    const QString lower = path.toLower();
    if (!lower.endsWith(".bin")) {
        err = "Only .bin firmware files are supported";
        return false;
    }

    QFile f(path);
    if (!f.exists()) { err = "File does not exist: " + path; return false; }
    if (!f.open(QIODevice::ReadOnly)) { err = "Cannot open file: " + f.errorString(); return false; }
    const QByteArray body = f.readAll();
    f.close();

    if (body.isEmpty()) { err = "Firmware file is empty"; return false; }

    const quint32 bodyLen = static_cast<quint32>(body.size());
    // board_code at firmware-header offset 0x20 (§1.1 START, §3.3)
    m_boardCode = (bodyLen >= 0x24)
                      ? static_cast<quint32>(static_cast<uint8_t>(body.at(0x20)))
                            | (static_cast<quint32>(static_cast<uint8_t>(body.at(0x21))) << 8)
                            | (static_cast<quint32>(static_cast<uint8_t>(body.at(0x22))) << 16)
                            | (static_cast<quint32>(static_cast<uint8_t>(body.at(0x23))) << 24)
                      : 0;

    m_crc = crc16Modbus(body, 0, body.size());

    // image = body + 8-byte trailer
    m_image = body;
    m_image.resize(static_cast<int>(bodyLen) + 8);
    m_image[bodyLen + 0] = static_cast<char>(bodyLen & 0xFF);
    m_image[bodyLen + 1] = static_cast<char>((bodyLen >> 8) & 0xFF);
    m_image[bodyLen + 2] = static_cast<char>((bodyLen >> 16) & 0xFF);
    m_image[bodyLen + 3] = static_cast<char>((bodyLen >> 24) & 0xFF);
    m_image[bodyLen + 4] = static_cast<char>(m_crc & 0xFF);
    m_image[bodyLen + 5] = static_cast<char>((m_crc >> 8) & 0xFF);
    m_image[bodyLen + 6] = 0;
    m_image[bodyLen + 7] = 0;

    m_firmLen = bodyLen + 8;
    m_path    = path;
    m_sendAddr = 0;
    m_state   = ST_IDLE;
    m_iapStart = false;
    m_bootHeartbeat = false;
    m_boardCodeMismatch = false;
    return true;
}

void OtaClient::start(quint8 targetNode)
{
    if (m_image.isEmpty()) return;
    m_targetNode = targetNode;
    m_sendAddr   = 0;
    m_delay      = 0;
    m_state      = ST_IDLE;
    m_iapStart   = true;
    m_bootHeartbeat = false;
    m_boardCodeMismatch = false;
}

void OtaClient::stop()
{
    m_iapStart = false;
    m_state    = ST_IDLE;
    m_delay    = 0;
}

quint32 OtaClient::firmwareBodyLen() const
{
    return (m_firmLen >= 8) ? (m_firmLen - 8) : 0;
}

quint32 OtaClient::progressPercent() const
{
    if (m_firmLen == 0) return 0;
    quint32 pct = static_cast<quint32>((static_cast<quint64>(m_sendAddr) * 100) / m_firmLen);
    return pct > 100 ? 100 : pct;
}

bool OtaClient::deviceOnline() const
{
    if (m_lastHeartbeatMs == 0) return false;
    return (QDateTime::currentMSecsSinceEpoch() - m_lastHeartbeatMs) < 1500;
}

bool OtaClient::takeBoardCodeMismatch()
{
    const bool v = m_boardCodeMismatch;
    m_boardCodeMismatch = false;
    return v;
}

// ---------------------------------------------------------------------------
// Frame helper
// ---------------------------------------------------------------------------
CanFdFrame OtaClient::makeFrame(uint32_t id, bool ext, const uint8_t *data, uint8_t len) const
{
    CanFdFrame f{};
    f.id        = id;
    f.is_ext_id = ext;
    f.is_fd     = true;
    f.brs       = true;   // OTA uses CAN-FD per YWD V0.2 §2.1 timing: BRS enabled
    f.len       = len;
    memset(f.data, 0, sizeof(f.data));
    if (data && len)
        memcpy(f.data, data, len);
    return f;
}

// ---------------------------------------------------------------------------
// Periodic state machine (protocol §3)
// ---------------------------------------------------------------------------
CanFdFrame OtaClient::tick()
{
    CanFdFrame none{};
    none.len = 0;
    if (!m_iapStart) return none;

    uint8_t d[64];
    memset(d, 0, sizeof(d));
    const uint8_t cmdByte = static_cast<uint8_t>(CMD_HEARTBEAT);  // slaveIdx 0

    switch (m_state) {
    case ST_IDLE:
        // Periodically send HEARTBEAT (extended frame) to probe the device
        if (++m_delay > 25) {
            m_delay = 0;
            d[0] = cmdByte;
            return makeFrame(ID_DOWNLINK | m_targetNode, true, d, 8);
        }
        break;

    case ST_START:
        if (++m_delay > 25) {
            m_delay = 0;
            if (m_bootHeartbeat) {
                // Device already in Boot: extended START handshake (board_code, 4 bytes), await cmd=START response
                d[0] = static_cast<uint8_t>(CMD_START);
                d[1] = static_cast<uint8_t>(m_boardCode & 0xFF);
                d[2] = static_cast<uint8_t>((m_boardCode >> 8) & 0xFF);
                d[3] = static_cast<uint8_t>((m_boardCode >> 16) & 0xFF);
                d[4] = static_cast<uint8_t>((m_boardCode >> 24) & 0xFF);
                return makeFrame(ID_DOWNLINK | m_targetNode, true, d, 8);
            } else {
                // Still in APP: YWD standard frame 0x500|node triggers the jump (START + board_code low byte)
                d[0] = static_cast<uint8_t>(CMD_START);
                d[1] = static_cast<uint8_t>(m_boardCode & 0xFF);
                return makeFrame(ID_YWD_OTA | m_targetNode, false, d, 8);
            }
        }
        break;

    case ST_ERASE:
        if (++m_delay > 30) {
            m_delay = 0;
            const quint32 bodyLen = firmwareBodyLen();
            d[0] = static_cast<uint8_t>(CMD_ERASE);
            d[1] = 0;
            d[2] = static_cast<uint8_t>(bodyLen & 0xFF);
            d[3] = static_cast<uint8_t>((bodyLen >> 8) & 0xFF);
            d[4] = static_cast<uint8_t>((bodyLen >> 16) & 0xFF);
            d[5] = static_cast<uint8_t>((bodyLen >> 24) & 0xFF);
            return makeFrame(ID_DOWNLINK | m_targetNode, true, d, 8);
        }
        break;

    case ST_PROG: {
        if (++m_delay > 5) {
            m_delay = 0;
            const int recordLen = 56;
            int slen = static_cast<int>(m_firmLen - m_sendAddr);
            if (slen > recordLen) slen = recordLen;
            if (slen < 0) slen = 0;
            for (int i = 0; i < slen; ++i)
                d[6 + i] = static_cast<uint8_t>(m_image.at(static_cast<int>(m_sendAddr) + i));

            d[0] = static_cast<uint8_t>(CMD_PROG);
            d[1] = static_cast<uint8_t>(slen);
            d[2] = static_cast<uint8_t>(m_sendAddr & 0xFF);
            d[3] = static_cast<uint8_t>((m_sendAddr >> 8) & 0xFF);
            d[4] = static_cast<uint8_t>((m_sendAddr >> 16) & 0xFF);
            d[5] = static_cast<uint8_t>((m_sendAddr >> 24) & 0xFF);
            return makeFrame(ID_DOWNLINK | m_targetNode, true, d, 64);
        }
        break;
    }

    case ST_CHECK:
        d[0] = static_cast<uint8_t>(CMD_CHECK);
        d[1] = 0;
        return makeFrame(ID_DOWNLINK | m_targetNode, true, d, 8);

    case ST_JUMP:
        d[0] = static_cast<uint8_t>(CMD_JUMP);
        d[1] = 0;
        m_iapStart = false;
        m_state    = ST_END;
        return makeFrame(ID_DOWNLINK | m_targetNode, true, d, 8);

    case ST_END:
    default:
        break;
    }
    return none;
}

// ---------------------------------------------------------------------------
// Uplink parsing (protocol §1.2 / §3)
// ---------------------------------------------------------------------------
bool OtaClient::handleFrame(const CanFdFrame &frame)
{
    if (!frame.is_ext_id) return false;
    const uint32_t rawId = frame.id & 0x1FFFFFFFU;
    const uint8_t  devId = static_cast<uint8_t>(rawId & 0x7F);
    if (devId != m_targetNode) return false;

    const uint32_t base = rawId & 0xFFFFF80U;
    if (base != ID_UPLINK) return false;      // only 0x08000000|node
    if (frame.len < 2) return false;

    m_lastHeartbeatMs = QDateTime::currentMSecsSinceEpoch();

    const uint8_t *d = frame.data;
    const uint8_t cmd = d[0];
    const uint8_t stas = d[1];

    switch (cmd) {
    case CMD_HEARTBEAT:
        if (frame.len >= 8) {
            m_boardId     = d[2];
            m_hardVer     = d[3];
            m_softVer     = d[4];
            m_bootVer     = d[5];
            m_compileDate = static_cast<ushort>(d[6] | (d[7] << 8));
        }
        if (m_state == ST_IDLE && (stas == STAS_APP || stas <= 5)) {
            m_state = ST_START;
            m_delay = 30;   // send the first frame immediately
        } else if (m_state == ST_START && stas <= 5) {
            m_bootHeartbeat = true;   // device already in Boot
        }
        break;

    case CMD_START:
        if (frame.len >= 8) {
            m_boardId     = d[2];
            m_hardVer     = d[3];
            m_softVer     = d[4];
            m_bootVer     = d[5];
            m_compileDate = static_cast<ushort>(d[6] | (d[7] << 8));
        }
        if (stas == STAS_APP) {
            m_iapStart = false;
            m_state    = ST_IDLE;
            m_boardCodeMismatch = true;
            break;
        }
        if ((m_boardCode & 0xFF) != m_boardId) {
            m_iapStart = false;
            m_state    = ST_IDLE;
            m_boardCodeMismatch = true;
            break;
        }
        m_state = ST_ERASE;
        break;

    case CMD_ERASE:
        if (stas != 1) { m_iapStart = false; m_state = ST_IDLE; break; }
        m_state    = ST_PROG;
        m_sendAddr = 0;
        break;

    case CMD_PROG:
        if (frame.len >= 6) {
            m_sendAddr = static_cast<quint32>(d[2])
                       | (static_cast<quint32>(d[3]) << 8)
                       | (static_cast<quint32>(d[4]) << 16)
                       | (static_cast<quint32>(d[5]) << 24);
            m_sendAddr += 56;
            m_delay = 5;
            if (m_sendAddr >= m_firmLen) {
                m_sendAddr = m_firmLen;
                m_state    = ST_CHECK;
            }
        }
        break;

    case CMD_CHECK:
        if (stas == 1) {
            m_state = ST_JUMP;
        } else {
            m_iapStart = false;
            m_state    = ST_IDLE;
        }
        break;

    default:
        break;
    }
    return true;
}
