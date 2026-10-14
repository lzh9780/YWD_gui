#ifndef OTA_CLIENT_H
#define OTA_CLIENT_H

// ============================================================================
// OTA (IAP) firmware-update state machine — ported from the WinForms reference
// implementation (winformCtrlapp/tpMotionYWD/Core/OtaCanFd.cs) and the protocol
// document "OTA升级协议v2.md" (OTA Upgrade Protocol v2).
//
// Frame encoding (protocol v2 §1):
//   Downlink (host → device) : 29-bit extended ID  0x04000000 | devID
//                              data[0] = (slaveIdx << 3) | cmd   (slaveIdx fixed 0)
//   Uplink   (device → host) : 29-bit extended ID  0x08000000 | devID, 8 bytes
//                              [cmd][iapStas][boardID][HARD_VER][SOFT_VER]
//                              [BOOT_VER][dateLo][dateHi]
//   APP → Boot trigger (YWD) : 11-bit standard ID  0x500 | node
//                              data[0] = START(1), data[1] = board_code low byte
//
// Boot session commands (sent as extended frames):
//   HEARTBEAT(0): 8B [cmd]                                   boot heartbeat
//   START(1)    : 8B [cmd][board_code ×4 bytes]              board_code handshake
//   ERASE(2)    : 8B [cmd][0][len32 LE]                      len = fw length (excl. trailer)
//   PROG(3)     : 64B [cmd][slen][addr32 LE][56B data]       56 bytes per packet
//   CHECK(4)    : 8B [cmd][0]                                whole-image CRC16 check
//   JUMP(5)     : 8B [cmd][0]                                jump to application
//
// Firmware package: the loaded .bin has an 8-byte trailer appended before
// download: [len32 LE][crc16-modbus LE][reserved 0 ×2] (§3.1).
// ============================================================================

#include "canfd_device.h"
#include <QByteArray>
#include <QString>

class OtaClient
{
public:
    enum State {
        ST_IDLE = 0,   // idle / heartbeat phase
        ST_START,      // jump triggered, waiting for Boot
        ST_ERASE,      // erasing
        ST_PROG,       // downloading
        ST_CHECK,      // verifying
        ST_JUMP,       // jumping to application
        ST_END         // done
    };

    // Command codes (OTA upgrade protocol v2 §1.1)
    enum Cmd {
        CMD_HEARTBEAT = 0,
        CMD_START     = 1,
        CMD_ERASE     = 2,
        CMD_PROG      = 3,
        CMD_CHECK     = 4,
        CMD_JUMP      = 5
    };

    static constexpr uint32_t ID_DOWNLINK   = 0x04000000;  // H→D extended frame
    static constexpr uint32_t ID_UPLINK     = 0x08000000;  // D→H extended frame
    static constexpr uint32_t ID_YWD_OTA    = 0x500;       // YWD standard frame 0x500|node
    static constexpr uint8_t  STAS_APP      = 0xFF;        // device running in APP

    explicit OtaClient();

    // ---- Session control ----
    // Load firmware from `path` (must be .bin). Builds the trailer and computes
    // CRC16. Returns false if the file is unsupported/unreadable.
    bool loadFirmware(const QString &path, QString &err);
    // Reset the state machine and begin a session to the given target node.
    void start(quint8 targetNode);
    void stop();

    bool isRunning() const { return m_iapStart; }
    State state() const { return m_state; }

    // ---- Periodic drive (call from a ~20 ms timer) ----
    // Returns a frame to transmit (frame.len == 0 means "nothing to send").
    CanFdFrame tick();

    // ---- Uplink handling ----
    // Returns true if `frame` belongs to the OTA uplink channel and was consumed.
    bool handleFrame(const CanFdFrame &frame);

    // ---- Firmware / session info for the UI ----
    QString firmwarePath() const   { return m_path; }
    quint32 firmwareLength() const { return m_firmLen; }      // incl. 8B trailer
    quint32 firmwareBodyLen() const;                          // excl. trailer
    quint32 firmwareBoardCode() const { return m_boardCode; }
    quint16 firmwareCrc() const    { return m_crc; }
    quint32 progressPercent() const;
    quint32 sendAddress() const    { return m_sendAddr; }

    quint8  targetNode() const     { return m_targetNode; }
    // Device identity (from the last boot heartbeat / handshake response)
    bool    deviceOnline() const;
    quint8  devBoardId() const     { return m_boardId; }
    quint8  devHardVer() const     { return m_hardVer; }
    quint8  devSoftVer() const     { return m_softVer; }
    quint8  devBootVer() const     { return m_bootVer; }
    ushort  devCompilePacked() const { return m_compileDate; }
    int     devCompileYear() const  { return ((m_compileDate >> 9) & 0x7F) + 2020; }
    int     devCompileMonth() const { return (m_compileDate >> 5) & 0x0F; }
    int     devCompileDay() const   { return m_compileDate & 0x1F; }

    // True once when the session aborted due to a board_code mismatch (the UI
    // polls and clears it to show a message).
    bool takeBoardCodeMismatch();

    static ushort crc16Modbus(const QByteArray &data, int offset, int len);

private:
    CanFdFrame makeFrame(uint32_t id, bool ext, const uint8_t *data, uint8_t len) const;
    void log(const QString &msg);

    // Firmware image with the 8-byte trailer appended
    QByteArray m_image;
    QString    m_path;
    quint32    m_firmLen = 0;      // total length including trailer
    quint32    m_boardCode = 0;
    quint16    m_crc = 0;

    // Session state
    bool    m_iapStart = false;
    State   m_state = ST_IDLE;
    quint8  m_targetNode = 1;
    quint32 m_sendAddr = 0;
    int     m_delay = 0;           // tick divider for the current step

    // Device identity / liveness
    qint64  m_lastHeartbeatMs = 0; // monotonic ms of the last uplink frame
    bool    m_bootHeartbeat = false;  // saw a Boot heartbeat (iapStas 0..5)
    quint8  m_boardId = 0;
    quint8  m_hardVer = 0;
    quint8  m_softVer = 0;
    quint8  m_bootVer = 0;
    ushort  m_compileDate = 0;

    bool    m_boardCodeMismatch = false;
};

#endif // OTA_CLIENT_H
