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

// Aggregation control frames
#define FRAME_AGG_CTRL1         0x001
#define FRAME_AGG_CTRL2         0x002
#define FRAME_AGG_CTRL3         0x003

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
#define REG_FB_DIV              0x02  // Feedback divider
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
#define REG_IQ                  0x55  // Q-axis current
#define REG_PROTOCOL_VER        0x60  // Protocol version
#define REG_VENDOR_ID           0x61  // Vendor ID
#define REG_PRODUCT_ID          0x62  // Product ID
#define REG_SW_VER              0x64  // Firmware version
#define REG_SN                  0x66  // Serial number

// ============================================================================
// Structs for decoded data
// ============================================================================
struct MitCtrlCmd {
    float pos_des;      // Desired position (rad)    data[0..3] int16
    float vel_des;      // Desired velocity (rad/s)  data[4..5] int12
    float kp;           // Position stiffness (Nm/rad) data[6..7] uint12
    float kd;           // Velocity damping (Nm/(rad/s)) data[8..9] uint12
    float ff_torque;    // Feedforward torque (Nm)   data[10..11] int12
};

struct PosVelCmd {
    float pos_des;      // Desired position (rad)    data[0..3] 16+16
    float vel_limit;    // Velocity limit (rad/s)     data[4..7]
};

struct ConstVelCmd {
    float vel_des;      // Desired velocity (rad/s)  data[0..3] int32
};

struct SystemCmd {
    uint8_t cmd;        // Command code
    uint8_t param;      // Parameter
};

struct FeedbackFrame {
    uint8_t  motor_id;
    uint8_t  state;       // B0[7:4]  state code
    uint8_t  mode;        // B0[3:0]  control mode
    uint8_t  fault;       // B1       fault code
    float    position;    // rad,     normalized (raw / (2³¹-1) * PMAX)
    float    velocity;    // rad/s,   normalized (raw / 32767 * VMAX)
    float    torque;      // N·m,    normalized (raw / 32767 * TMAX)
    float    voltage;     // V,       VBUS int16 LSB=0.01V
    int8_t   temp_mos;     // ℃
    int8_t   temp_motor;   // ℃
    uint8_t  seq;          // frame counter 0~255
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

    // MIT control (motor_id: 0xFF = broadcast)
    CanFdFrame encodeMitCtrl(uint8_t motor_id,
                             float pos_des, float vel_des,
                             float kp, float kd, float ff_torque);

    // Position-Velocity control
    CanFdFrame encodePosVel(uint8_t motor_id,
                            float pos_des, float vel_limit);

    // Constant velocity control
    CanFdFrame encodeConstVel(uint8_t motor_id, float vel_des);

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

    bool decodeFeedback(const CanFdFrame &frame, FeedbackFrame &fb);
    bool decodeParamResponse(const CanFdFrame &frame, ParamResponse &resp);

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
        FT_AGG_CTRL1,
        FT_AGG_CTRL2,
        FT_AGG_CTRL3
    };

    static FrameType classifyFrame(uint32_t canId);

    // Scaling helpers
    static float int16ToFloat(uint16_t val, float scale) {
        int16_t sval = static_cast<int16_t>(val);
        return static_cast<float>(sval) * scale;
    }

    static float uint12ToFloat(uint16_t val, float maxVal) {
        return static_cast<float>(val & 0x0FFF) * maxVal / 4095.0f;
    }

    static float int12ToFloat(uint16_t val, float maxVal) {
        int16_t sval = static_cast<int16_t>(val << 4) >> 4;  // sign extend
        return static_cast<float>(sval) * maxVal / 2047.0f;
    }

    static uint16_t floatToInt16(float val, float scale) {
        int16_t sval = static_cast<int16_t>(std::round(val / scale));
        return static_cast<uint16_t>(sval);
    }

    static uint16_t floatToUint12(float val, float maxVal) {
        uint16_t u = static_cast<uint16_t>(
            std::round(std::max(0.0f, std::min(val, maxVal)) / maxVal * 4095.0f));
        return u & 0x0FFF;
    }

    static uint16_t floatToInt12(float val, float maxVal) {
        int16_t s = static_cast<int16_t>(
            std::round(std::max(-maxVal, std::min(val, maxVal)) / maxVal * 2047.0f));
        return static_cast<uint16_t>(s) & 0x0FFF;
    }

    // Register name mapping
    static QString getRegisterName(uint16_t addr);
    static QString getBankName(uint8_t bank);

    // Full register catalogue
    static QVector<RegInfo> getAllRegisters();

private:
    // Build the base CAN ID: (0x000 | bank<<8 | motor_id)
    // For CMD frames: ID = base_cmd + motor_id (low 8 bits)
    uint32_t buildCmdId(uint16_t baseCmd, uint8_t motor_id) const {
        return baseCmd | (motor_id & 0xFF);
    }

    // Scaling limits – defaults from typical motor datasheet,
    // updated dynamically from device registers 0x11 (PMAX / pos limit),
    // 0x12 (VMAX / vel limit), 0x13 (TMAX / torque limit) via reg read response
    float m_pmax = 6.283185f;   // rad
    float m_vmax = 45.0f;       // rad/s
    float m_tmax = 18.0f;       // N·m
};

#endif // YWD_PROTOCOL_H
