#ifndef YWD_PROTOCOL_H
#define YWD_PROTOCOL_H

#include "canfd_device.h"
#include <QString>
#include <QMap>
#include <QVector>
#include <cstdint>
#include <cmath>
#include <algorithm>

// ============================================================================
// Frame ID definitions (11-bit standard ID, CAN-FD)
// ============================================================================
#define CMD_MIT_CTRL            0x100   // MIT control
#define CMD_POS_VEL_CTRL        0x180   // Position-Velocity control
#define CMD_CONST_VEL           0x200   // Constant velocity
#define CMD_SYSTEM              0x400   // System command
#define CMD_REG_OP              0x480   // Register read/write

#define FRAME_FEEDBACK          0x600   // Motor feedback
#define FRAME_PARAM_RESP        0x680   // Register read response

// Aggregation control frames (H→D, §3.1 / §6)
#define FRAME_AGG_CTRL1         0x001   // MIT
#define FRAME_AGG_CTRL2         0x002   // Pos+Vel
#define FRAME_AGG_CTRL3         0x003   // ConstVel

// Vendor aggregate control frames (H→D, §1.5 / §3.1): vendor CTRL_MODE 4..6
#define FRAME_AGG_CTRL_VENDOR4  0x004
#define FRAME_AGG_CTRL_VENDOR5  0x005
#define FRAME_AGG_CTRL_VENDOR6  0x006

// Vendor OTA channels (§12) — not used by this GUI's control path
#define CMD_VENDOR_OTA          0x500   // H→D  OTA (0x500 | node; 0x500 = broadcast)
#define FRAME_VENDOR_OTA_RESP   0x780   // D→H  OTA response

// ============================================================================
// System command codes (CMD_SYSTEM frame, data[0])
// ============================================================================
#define SYS_CMD_ENABLE          0x01
#define SYS_CMD_DISABLE         0x02
#define SYS_CMD_SET_ZERO        0x03
#define SYS_CMD_CLEAR_FAULT     0x04
#define SYS_CMD_SAVE            0x05
#define SYS_CMD_RESET           0x06
#define SYS_CMD_LOAD_DEFAULTS   0x07

// ============================================================================
// Register bank definitions
// ============================================================================
#define REG_BANK_COMM           0x00    // Identity & Communication  (0x00-0x0F)
#define REG_BANK_CTRL           0x10    // Control config & soft limits (0x10-0x1F)
#define REG_BANK_PID            0x20    // Three-loop PID gains       (0x20-0x2F)
#define REG_BANK_HARDLIM        0x30    // Hardware limits (read-only) (0x30-0x3F)
#define REG_BANK_MOTOR          0x40    // Motor body parameters      (0x40-0x4F)
#define REG_BANK_TELEM          0x50    // Real-time telemetry (RO)   (0x50-0x5F)
#define REG_BANK_VERSION        0x60    // Version info (read-only)   (0x60-0x6F)
#define REG_BANK_RESERVED       0x70    // Standard reserved          (0x70-0x7F)
#define REG_BANK_USER           0x80    // Vendor-defined             (0x80-0xFF)

// Key register addresses (matching getAllRegisters catalogue)
#define REG_NODE_ID             0x00  // Node ID
#define REG_CMD_TIMEOUT         0x01  // Command timeout
#define REG_FB_AGG              0x02  // Feedback grouping (V0.2: was FB_DIV)
#define REG_CTRL_MODE           0x10  // Control mode
#define REG_PMAX                0x11  // Position limit (rad)
#define REG_VMAX                0x12  // Velocity limit (rad/s)
#define REG_TMAX                0x13  // Torque limit (N·m)
#define REG_UV_VALUE            0x30  // Under-voltage threshold
#define REG_OV_VALUE            0x31  // Over-voltage threshold
#define REG_OC_VALUE            0x34  // Over-current threshold
#define REG_NPP                 0x40  // Pole pairs
#define REG_KT                  0x45  // Torque constant
#define REG_ROTOR_POS           0x50  // Rotor position (rad)
#define REG_OUT_POS             0x51  // Output position (rad)
#define REG_VB                  0x52  // Bus voltage
#define REG_T_PCB               0x53  // PCB temperature
#define REG_T_MOTOR             0x54  // Motor temperature
#define REG_IQ                  0x55  // Q-axis current (peak)
#define REG_ID                  0x56  // D-axis current (peak) — V0.2 new
#define REG_PROTOCOL_VER        0x60  // Protocol version
#define REG_VENDOR_ID           0x61  // Vendor ID
#define REG_PRODUCT_ID          0x62  // Product ID
#define REG_SW_VER              0x64  // Firmware version
#define REG_SN                  0x66  // Serial number

// ============================================================================
// Structs for decoded data
// ============================================================================
// One 13-byte feedback record (V0.2 §7.1). Values are converted from the
// record-relative offsets to physical units using the cached PMAX/VMAX/TMAX.
struct FeedbackFrame {
    uint8_t  motor_id;     // +0  NODE_ID of this record
    uint8_t  state;        // +1[7:4]  state code (0=Dis,1=En,2=Fault,3=Calib)
    uint8_t  mode;         // +1[3:0]  control mode (1..3 std, 4..6 vendor)
    uint8_t  fault;        // +2       fault code (§7.3)
    float    position;     // +3..+6   int32 normalized → rad
    float    velocity;     // +7..+8   int16 normalized → rad/s
    float    torque;       // +9..+10  int16 normalized → N·m (output shaft)
    int8_t   temp_mos;     // +11      int8 °C (MOSFET)
    int8_t   temp_motor;   // +12      int8 °C (rotor/winding)
    float    voltage;      // header B2 uint8, 0.25 V/LSB (shared, low-precision)
    uint8_t  seq;          // header B1, per-feedback-CAN-ID counter (shared)
    uint8_t  node_count;   // header B0 record count N (1..4)
};

struct ParamResponse {
    uint16_t addr;
    uint32_t value;
    uint8_t  rstat;    // §8.3: 0=OK, 1=unknown RID, 2=readonly, 3=out of range, 4=state forbid
    bool     is_write;
    bool     success;
};

// Per-register metadata for UI table
struct RegInfo {
    uint16_t addr;
    QString  name;
    uint8_t  bank;
    bool     is_readonly;
    QString  data_type;   // "uint32" / "float32" / "int32"
};

// ============================================================================
// YwdProtocol class - handles encoding and decoding
// ============================================================================
class YwdProtocol
{
public:
    YwdProtocol();

    // ============================================================
    // Scaling limits (read from device registers 0x11/0x12/0x13)
    // ============================================================
    float pmax() const { return m_pmax; }
    float vmax() const { return m_vmax; }
    float tmax() const { return m_tmax; }

    void setPmax(float v) { m_pmax = v; }
    void setVmax(float v) { m_vmax = v; }
    void setTmax(float v) { m_tmax = v; }

    // Update scaling from a register response (reg 0x11→pmax, 0x12→vmax, 0x13→tmax)
    // raw == register value (uint32_t holding IEEE 754 float32)
    void applyRegScaling(uint16_t addr, uint32_t raw);

    // -- Encoding (command -> CAN-FD frame) --

    // MIT control (NODE_ID 0x01..0x7F; 0x00 is the broadcast address, §2.2)
    CanFdFrame encodeMitCtrl(uint8_t motor_id,
                             float pos_des, float vel_des,
                             float kp, float kd, float ff_torque);

    // Position-Velocity control (acc/dec: rad/s², 0 = use register default)
    CanFdFrame encodePosVel(uint8_t motor_id,
                            float pos_des, float vel_limit,
                            float acc = 0.0f, float dec = 0.0f);

    // Constant velocity control (acc/dec: rad/s², 0 = use register default)
    CanFdFrame encodeConstVel(uint8_t motor_id, float vel_des,
                              float acc = 0.0f, float dec = 0.0f);

    // -- Aggregated multi-motor control frames (V0.2 §6) --
    // One frame carries up to 4 motors:
    //   B0 = Header (bits[2:0] = rec_cnt)  +  N × [NODE_ID(1B) + Body]
    //   MIT ID=0x001 (Body 12B), PosVel ID=0x002 (Body 10B), ConstVel ID=0x003 (Body 6B)
    struct AggMitRecord {
        uint8_t motor_id;    // node id carried inside the frame (0x01..0x7F)
        float   pos_des;     // rad
        float   vel_des;     // rad/s
        float   kp;          // N·m/rad   (0.01/LSB)
        float   kd;          // N·m·s/rad (0.001/LSB)
        float   ff_torque;   // N·m
    };
    struct AggPosVelRecord {
        uint8_t motor_id;
        float   pos_des;     // rad
        float   vel_limit;   // rad/s, normalized vel/VMAX*65535 (same as single frame)
        float   acc;         // rad/s², LSB=1
        float   dec;         // rad/s², LSB=1
    };
    struct AggConstVelRecord {
        uint8_t motor_id;
        float   vel_des;     // rad/s
        float   acc;         // rad/s², LSB=1
        float   dec;         // rad/s², LSB=1
    };

    CanFdFrame encodeAggMit(const std::vector<AggMitRecord> &recs);
    CanFdFrame encodeAggPosVel(const std::vector<AggPosVelRecord> &recs);
    CanFdFrame encodeAggConstVel(const std::vector<AggConstVelRecord> &recs);

    // System command
    CanFdFrame encodeSystemCmd(uint8_t motor_id,
                               uint8_t cmd, uint8_t param = 0);

    // Register write
    CanFdFrame encodeRegWrite(uint8_t motor_id,
                              uint16_t addr, uint32_t value);

    // Register read
    CanFdFrame encodeRegRead(uint8_t motor_id, uint16_t addr);

    // Block register read (up to 8 RIDs per frame per protocol spec)
    CanFdFrame encodeRegBlockRead(uint8_t motor_id,
                                  const std::vector<uint8_t> &rids);

    // -- Decoding (CAN-FD frame -> structured data) --
    // (uses stored pmax/vmax/tmax to convert raw→physical units)

    bool decodeParamResponse(const CanFdFrame &frame, ParamResponse &resp);

    // Decode a feedback frame (V0.2 §7.1): every feedback frame — single-node or
    // aggregate — uses the same "3-byte header + N × 13-byte record" layout.
    // Performs the §7.1 format validation and returns one FeedbackFrame per
    // record (each already carrying the shared SEQ/VBUS from the header);
    // `out` is cleared on entry. Returns false if the frame is malformed.
    bool decodeFeedbackRecords(const CanFdFrame &frame,
                               std::vector<FeedbackFrame> &out);

    // Decode a block register response (C: B0=CMD, B1=N, then RID+RSTAT+val)
    bool decodeRegBlockResponse(const CanFdFrame &frame,
                                std::vector<ParamResponse> &results);

    // Frame type detection
    enum FrameType {
        FT_UNKNOWN = 0,
        FT_MIT_CTRL,
        FT_POS_VEL_CTRL,
        FT_CONST_VEL,
        FT_SYSTEM_CMD,
        FT_REG_OP,
        FT_FEEDBACK,
        FT_PARAM_RESP,
        FT_VENDOR_OTA,
        FT_AGG_CTRL1,
        FT_AGG_CTRL2,
        FT_AGG_CTRL3,
        FT_AGG_CTRL_VENDOR4,
        FT_AGG_CTRL_VENDOR5,
        FT_AGG_CTRL_VENDOR6
    };

    static FrameType classifyFrame(uint32_t canId);

    // Register name mapping
    static QString getRegisterName(uint16_t addr);
    static QString getBankName(uint8_t bank);

    // Full register catalogue
    static QVector<RegInfo> getAllRegisters();

private:
    // Build the CAN ID: base_cmd | NODE_ID. NODE_ID is a 7-bit address
    // (0x01..0x7F, 0x00 = broadcast, §2.2) — bit 7 must never leak into the
    // ID (e.g. 0x480|0x80 would alias the 0x500 OTA channel).
    uint32_t buildCmdId(uint16_t baseCmd, uint8_t motor_id) const {
        return baseCmd | (motor_id & 0x7F);
    }

    // Scaling limits – defaults from typical motor datasheet,
    // updated dynamically from device registers 0x11 (PMAX / pos limit),
    // 0x12 (VMAX / vel limit), 0x13 (TMAX / torque limit) via reg read response
    float m_pmax = 6.283185f;   // rad
    float m_vmax = 45.0f;       // rad/s
    float m_tmax = 18.0f;       // N·m
};

#endif // YWD_PROTOCOL_H
