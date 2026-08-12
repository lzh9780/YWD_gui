#include "ywd_protocol.h"
#include <cstring>

// ---------------------------------------------------------------------------
// CAN-FD DLC padding: only 0/8/12/16/20/24/32/48/64 are valid (per §2.1)
// ---------------------------------------------------------------------------
static constexpr uint8_t validDlcLut[] = {0, 8, 12, 16, 20, 24, 32, 48, 64};

static uint8_t padToValidDlc(uint8_t len)
{
    for (uint8_t v : validDlcLut)
        if (len <= v) return v;
    return len;
}

YwdProtocol::YwdProtocol() {}

void YwdProtocol::applyRegScaling(uint16_t addr, uint32_t raw)
{
    float val;
    memcpy(&val, &raw, sizeof(val));   // uint32→float32 reinterpret
    // Reject garbage values: a NaN/inf/<=0 scaling limit would cause
    // division-by-zero and undefined behaviour in the encoders.
    if (!std::isfinite(val) || val <= 0.0f)
        return;
    switch (addr) {
        case 0x11: m_pmax = val; break;
        case 0x12: m_vmax = val; break;
        case 0x13: m_tmax = val; break;
        default:   break;
    }
}

// ============================================================================
// Encoding functions
// ============================================================================

// MIT Control frame (0x100)   §5.1   DLC=12
// B0-B3:   P_des  int32 LE  (round(pos_des / PMAX * (2^31-1)))
// B4-B5:   V_des  int16 LE  (round(vel_des / VMAX * 32767))
// B6-B7:   Kp     uint16 LE (round(kp / 0.01))
// B8-B9:   Kd     uint16 LE (round(kd / 0.001))
// B10-B11: Tff    int16 LE  (round(torque / TMAX * 32767))
CanFdFrame YwdProtocol::encodeMitCtrl(uint8_t motor_id,
                                      float pos_des, float vel_des,
                                      float kp, float kd,
                                      float ff_torque)
{
    CanFdFrame f{};
    f.id        = buildCmdId(CMD_MIT_CTRL, motor_id);
    f.is_ext_id = false;
    f.is_fd     = true;
    f.brs       = false;
    f.len       = padToValidDlc(12);

    memset(f.data, 0, sizeof(f.data));

    // Use stored scaling limits (synchronised with device reg 0x11/12/13)

    // P_des  int32 LE
    int32_t p = (int32_t)std::round(pos_des / m_pmax * 2147483647.0f);  // 2^31-1
    f.data[0] =  p        & 0xFF;
    f.data[1] = (p >> 8)  & 0xFF;
    f.data[2] = (p >> 16) & 0xFF;
    f.data[3] = (p >> 24) & 0xFF;

    // V_des  int16 LE
    int16_t v = (int16_t)std::round(vel_des / m_vmax * 32767.0f);
    f.data[4] =  v        & 0xFF;
    f.data[5] = (v >> 8)  & 0xFF;

    // Kp  uint16 LE  LSB=0.01
    uint16_t k = (uint16_t)std::round(kp / 0.01f);
    f.data[6] =  k        & 0xFF;
    f.data[7] = (k >> 8)  & 0xFF;

    // Kd  uint16 LE  LSB=0.001
    uint16_t d = (uint16_t)std::round(kd / 0.001f);
    f.data[8] =  d        & 0xFF;
    f.data[9] = (d >> 8)  & 0xFF;

    // Tff  int16 LE  normalized
    int16_t t = (int16_t)std::round(ff_torque / m_tmax * 32767.0f);
    f.data[10] =  t        & 0xFF;
    f.data[11] = (t >> 8)  & 0xFF;

    return f;
}

// Position-Velocity control frame (0x180)   §5.2   DLC=12
// B0-B3: P_des  int32 LE  (round(pos / PMAX * (2^31-1)))
// B4-B5: Vmax   uint16 LE (round(v / VMAX * 65535))
// B6-B7: ACC    uint16 LE (LSB=1 rad/s², 0=use default)
// B8-B9: DEC    uint16 LE (LSB=1 rad/s², 0=use default)
CanFdFrame YwdProtocol::encodePosVel(uint8_t motor_id,
                                     float pos_des, float vel_limit)
{
    CanFdFrame f{};
    f.id        = buildCmdId(CMD_POS_VEL_CTRL, motor_id);
    f.is_ext_id = false;
    f.is_fd     = true;
    f.len       = padToValidDlc(12);

    memset(f.data, 0, sizeof(f.data));

    // P_des  int32 LE
    int32_t p = (int32_t)std::round(pos_des / m_pmax * 2147483647.0f);
    f.data[0] =  p        & 0xFF;
    f.data[1] = (p >> 8)  & 0xFF;
    f.data[2] = (p >> 16) & 0xFF;
    f.data[3] = (p >> 24) & 0xFF;

    // Vmax  uint16 LE  (use default ACC/DEC = 0)
    uint16_t v = (uint16_t)std::round(vel_limit / m_vmax * 65535.0f);
    f.data[4] =  v        & 0xFF;
    f.data[5] = (v >> 8)  & 0xFF;
    // B6-B9: ACC/DEC = 0 (use register defaults)

    return f;
}

// Constant velocity control frame (0x200)   §5.3   DLC=8
// B0-B1: V_des  int16 LE (round(vel / VMAX * 32767))
// B2-B3: ACC    uint16 LE (LSB=1 rad/s², 0=use default)
// B4-B5: DEC    uint16 LE (LSB=1 rad/s², 0=use default)
CanFdFrame YwdProtocol::encodeConstVel(uint8_t motor_id, float vel_des)
{
    CanFdFrame f{};
    f.id        = buildCmdId(CMD_CONST_VEL, motor_id);
    f.is_ext_id = false;
    f.is_fd     = true;
    f.len       = padToValidDlc(8);

    memset(f.data, 0, sizeof(f.data));

    // V_des  int16 LE
    int16_t v = (int16_t)std::round(vel_des / m_vmax * 32767.0f);
    f.data[0] =  v        & 0xFF;
    f.data[1] = (v >> 8)  & 0xFF;
    // B2-B5: ACC/DEC = 0 (use register defaults)

    return f;
}

// System command frame (0x400)   §10  DLC=1, B0=sub-command only
CanFdFrame YwdProtocol::encodeSystemCmd(uint8_t motor_id,
                                        uint8_t cmd, uint8_t /*param*/)
{
    CanFdFrame f{};
    f.id      = buildCmdId(CMD_SYSTEM, motor_id);
    f.is_ext_id = false;
    f.is_fd   = true;
    f.len     = padToValidDlc(1);   // 1 → 8  (spec §10: DLC=1, only B0)

    memset(f.data, 0, sizeof(f.data));
    f.data[0] = cmd;

    return f;
}

// Register write frame (0x480)   §8.1
// B0=0x02(write)  B1=N=1  B2=RID  B3-B6=value(4B LE)
CanFdFrame YwdProtocol::encodeRegWrite(uint8_t motor_id,
                                       uint16_t addr, uint32_t value)
{
    CanFdFrame f{};
    f.id        = buildCmdId(CMD_REG_OP, motor_id);
    f.is_ext_id = false;
    f.is_fd     = true;
    f.len       = padToValidDlc(7);   // 7 → 8

    memset(f.data, 0, sizeof(f.data));
    f.data[0] = 0x02;                    // CMD write
    f.data[1] = 1;                       // N = 1
    f.data[2] = addr & 0xFF;             // RID (1 byte)
    // value 4B little-endian
    f.data[3] =  value        & 0xFF;
    f.data[4] = (value >> 8)  & 0xFF;
    f.data[5] = (value >> 16) & 0xFF;
    f.data[6] = (value >> 24) & 0xFF;

    return f;
}

// Register read frame (0x480)   §8.1
// B0=0x01(read)  B1=N=1  B2=RID
CanFdFrame YwdProtocol::encodeRegRead(uint8_t motor_id, uint16_t addr)
{
    CanFdFrame f{};
    f.id        = buildCmdId(CMD_REG_OP, motor_id);
    f.is_ext_id = false;
    f.is_fd     = true;
    f.len       = padToValidDlc(3);   // 3 → 8

    memset(f.data, 0, sizeof(f.data));
    f.data[0] = 0x01;             // CMD read
    f.data[1] = 1;                // N = 1
    f.data[2] = addr & 0xFF;      // RID (1 byte)

    return f;
}

// ============================================================================
// Decoding functions
// ============================================================================

bool YwdProtocol::decodeFeedback(const CanFdFrame &frame, FeedbackFrame &fb)
{
    // Must be 0x600..0x67F, NOT 0x680..0x6FF (param resp)
    if ((frame.id & 0xFF80) != FRAME_FEEDBACK) return false;

    // DLC is fixed to 16 per §7.1; short frames would read stale data
    if (frame.len < 16) return false;

    const uint8_t *d = frame.data;
    fb.motor_id = frame.id & 0x7F;   // 7-bit NODE_ID per §2.2

    // B0: STATE[7:4] | MODE[3:0]
    fb.state = (d[0] >> 4) & 0x0F;
    fb.mode  = d[0] & 0x0F;

    // B1: FAULT
    fb.fault = d[1];

    // B2-B5: POS  int32  LE  normalized position
    int32_t pos_raw = d[2] | (d[3] << 8) | (d[4] << 16) | (d[5] << 24);
    fb.position = (float)pos_raw / 2147483647.0f * m_pmax;   // raw→rad
    // actual  x = pos_raw / (2³¹-1) * PMAX

    // B6-B7: VEL  int16  LE  normalized velocity
    int16_t vel_raw = (int16_t)(d[6] | (d[7] << 8));
    fb.velocity = (float)vel_raw / 32767.0f * m_vmax;         // raw→rad/s

    // B8-B9: TORQUE  int16  LE  normalized torque
    int16_t trq_raw = (int16_t)(d[8] | (d[9] << 8));
    fb.torque = (float)trq_raw / 32767.0f * m_tmax;          // raw→N·m

    // B10: T_MOS  int8  ℃
    fb.temp_mos = (int8_t)d[10];

    // B11: T_ROTOR  int8  ℃
    fb.temp_motor = (int8_t)d[11];

    // B12-B13: VBUS  int16  LE  LSB=0.01V
    int16_t vbus_raw = (int16_t)(d[12] | (d[13] << 8));
    fb.voltage = (float)vbus_raw * 0.01f;

    // B14: SEQ
    fb.seq = d[14];

    // B15: Reserved

    return true;
}

// Single-register response decode (same block format, §8.2, N typically = 1)
// Response layout: B0=CMD  B1=N  B2=RID  B3=RSTAT  B4-B7=value(LE)
bool YwdProtocol::decodeParamResponse(const CanFdFrame &frame,
                                      ParamResponse &resp)
{
    if ((frame.id & 0xFF80) != FRAME_PARAM_RESP) return false;

    // Need B0..B7 (CMD, N, RID, RSTAT, value[4])
    if (frame.len < 8) return false;

    uint8_t cmd = frame.data[0];
    resp.is_write = (cmd == 0x02);
    resp.rstat    = frame.data[3];                     // RSTAT at offset 3
    resp.success  = (frame.data[3] == 0x00);
    resp.addr     = frame.data[2];                     // RID  at offset 2
    // value 4B LE
    resp.value = static_cast<uint32_t>(frame.data[4])
               | (static_cast<uint32_t>(frame.data[5]) << 8)
               | (static_cast<uint32_t>(frame.data[6]) << 16)
               | (static_cast<uint32_t>(frame.data[7]) << 24);

    return true;
}

YwdProtocol::FrameType YwdProtocol::classifyFrame(uint32_t canId)
{
    // Aggregation control frames use very low IDs (0x001~0x003) — match the
    // full ID here. (Previously they sat in the switch on (canId & 0xFF00),
    // which is always 0 for these IDs, so they could never match.)
    switch (canId) {
        case FRAME_AGG_CTRL1:    return FT_AGG_CTRL1;
        case FRAME_AGG_CTRL2:    return FT_AGG_CTRL2;
        case FRAME_AGG_CTRL3:    return FT_AGG_CTRL3;
        default:                 break;
    }

    // 0x600~0x7FF range: feedback & param response share same high byte,
    // so use 0xFF80 mask (NODE_ID is 7-bit per §2.2, bit7 is free)
    uint32_t hi = canId & 0xFF00;
    if (hi == 0x600) {
        return (canId & 0x0080) ? FT_PARAM_RESP : FT_FEEDBACK;
    }

    switch (hi) {
        case CMD_MIT_CTRL:       return FT_MIT_CTRL;
        case CMD_POS_VEL_CTRL:   return FT_POS_VEL_CTRL;
        case CMD_CONST_VEL:      return FT_CONST_VEL;
        case CMD_SYSTEM:         return FT_SYSTEM_CMD;
        case CMD_REG_OP:         return FT_REG_OP;
        default:                 return FT_UNKNOWN;
    }
}

// ============================================================================
// Block register read  (CMD=0x01, up to 8 RIDs per frame, per §8.1)
// ============================================================================
CanFdFrame YwdProtocol::encodeRegBlockRead(uint8_t motor_id,
                                           const std::vector<uint8_t> &rids)
{
    CanFdFrame f{};
    f.id        = buildCmdId(CMD_REG_OP, motor_id);
    f.is_ext_id = false;
    f.is_fd     = true;

    int n = std::min((int)rids.size(), 8);
    f.len = padToValidDlc(static_cast<uint8_t>(2 + n));   // CMD(1) + N(1) + N × RID(1)
    memset(f.data, 0, sizeof(f.data));
    f.data[0] = 0x01;  // read
    f.data[1] = static_cast<uint8_t>(n);
    for (int i = 0; i < n; ++i)
        f.data[2 + i] = rids[i];

    return f;
}

// ============================================================================
// Decode block response  (0x680|node, per §8.2)
// Layout: B0=CMD  B1=N   per record: RID(1)+RSTAT(1)+value(4 LE)
// ============================================================================
bool YwdProtocol::decodeRegBlockResponse(const CanFdFrame &frame,
                                         std::vector<ParamResponse> &results)
{
    if ((frame.id & 0xFF80) != FRAME_PARAM_RESP) return false;
    if (frame.len < 2) return false;   // need at least CMD + N
    results.clear();

    uint8_t cmd  = frame.data[0];
    uint8_t n    = frame.data[1];
    bool is_write = (cmd == 0x02);

    int off = 2;
    for (int i = 0; i < n && off + 5 < (int)frame.len; ++i) {
        ParamResponse r;
        r.addr     = frame.data[off];  off++;
        r.rstat    = frame.data[off];
        r.success  = (frame.data[off] == 0x00);  off++;
        r.is_write = is_write;
        // value 4B LE
        r.value    = static_cast<uint32_t>(frame.data[off])
                   | (static_cast<uint32_t>(frame.data[off + 1]) << 8)
                   | (static_cast<uint32_t>(frame.data[off + 2]) << 16)
                   | (static_cast<uint32_t>(frame.data[off + 3]) << 24);
        off += 4;
        results.push_back(r);
    }
    return true;
}

// ============================================================================
// Full register catalogue  (from protocol §9)
// ============================================================================
QVector<RegInfo> YwdProtocol::getAllRegisters()
{
    // clang-format off
    QVector<RegInfo> regs = {
        // Bank 0x00 – Identity / Communication
        {0x00, "NODE_ID",       0x00, false, "uint32"},
        {0x01, "CMD_TIMEOUT",   0x00, false, "uint32"},
        {0x02, "FB_DIV",        0x00, false, "uint32"},
        // Bank 0x10 – Control config & soft limits
        {0x10, "CTRL_MODE",     0x10, false, "uint32"},
        {0x11, "PMAX",          0x10, false, "float32"},
        {0x12, "VMAX",          0x10, false, "float32"},
        {0x13, "TMAX",          0x10, false, "float32"},
        {0x14, "ACC",           0x10, false, "float32"},
        {0x15, "DEC",           0x10, false, "float32"},
        {0x18, "P_SOFT",        0x10, false, "float32"},
        {0x19, "V_SOFT",        0x10, false, "float32"},
        {0x1A, "T_SOFT",        0x10, false, "float32"},
        {0x1B, "ACC_SOFT",      0x10, false, "float32"},
        {0x1C, "DEC_SOFT",      0x10, false, "float32"},
        // Bank 0x20 – Three-loop PID gains
        {0x20, "KP_CUR",        0x20, false, "float32"},
        {0x21, "KI_CUR",        0x20, false, "float32"},
        {0x22, "KD_CUR",        0x20, false, "float32"},
        {0x23, "KP_SPD",        0x20, false, "float32"},
        {0x24, "KI_SPD",        0x20, false, "float32"},
        {0x25, "KD_SPD",        0x20, false, "float32"},
        {0x26, "KP_POS",        0x20, false, "float32"},
        {0x27, "KI_POS",        0x20, false, "float32"},
        {0x28, "KD_POS",        0x20, false, "float32"},
        {0x29, "I_BW",          0x20, false, "float32"},
        {0x2A, "V_BW",          0x20, false, "float32"},
        // Bank 0x30 – Hardware limits  (read-only)
        {0x30, "UV_VALUE",      0x30, true,  "float32"},
        {0x31, "OV_VALUE",      0x30, true,  "float32"},
        {0x32, "OT_COIL",       0x30, true,  "float32"},
        {0x33, "OT_MOS",        0x30, true,  "float32"},
        {0x34, "OC_VALUE",      0x30, true,  "float32"},
        {0x35, "OVERLOAD",      0x30, true,  "float32"},
        {0x36, "P_HARD",        0x30, true,  "float32"},
        {0x37, "V_HARD",        0x30, true,  "float32"},
        {0x38, "T_HARD",        0x30, true,  "float32"},
        {0x39, "ACC_HARD",      0x30, true,  "float32"},
        {0x3A, "DEC_HARD",      0x30, true,  "float32"},
        // Bank 0x40 – Motor body parameters  (mostly read-only on integrated motors)
        {0x40, "NPP",           0x40, true,  "uint32"},
        {0x41, "RS",            0x40, true,  "float32"},
        {0x42, "LS",            0x40, true,  "float32"},
        {0x43, "FLUX",          0x40, true,  "float32"},
        {0x44, "GR",            0x40, true,  "float32"},
        {0x45, "KT",            0x40, true,  "float32"},
        {0x46, "GREF",          0x40, true,  "float32"},
        // Bank 0x50 – Real-time telemetry  (read-only)
        {0x50, "ROTOR_POS",     0x50, true,  "float32"},
        {0x51, "OUT_POS",       0x50, true,  "float32"},
        {0x52, "VB",            0x50, true,  "float32"},
        {0x53, "T_PCB",         0x50, true,  "float32"},
        {0x54, "T_MOTOR",       0x50, true,  "float32"},
        {0x55, "IQ",            0x50, true,  "float32"},
        // Bank 0x60 – Version info  (read-only)
        {0x60, "PROTOCOL_VER",  0x60, true,  "uint32"},
        {0x61, "VENDOR_ID",     0x60, true,  "uint32"},
        {0x62, "PRODUCT_ID",    0x60, true,  "uint32"},
        {0x63, "HW_VER",        0x60, true,  "uint32"},
        {0x64, "SW_VER",        0x60, true,  "uint32"},
        {0x65, "BOOT_VER",      0x60, true,  "uint32"},
        {0x66, "SN",            0x60, true,  "uint32"},
    };
    // clang-format on
    return regs;
}

QString YwdProtocol::getRegisterName(uint16_t addr)
{
    // Look up from full catalogue first
    auto regs = getAllRegisters();
    for (const auto &r : regs) {
        if (r.addr == addr)
            return r.name;
    }
    return QString("0x%1").arg(addr, 2, 16, QChar('0'));
}

QString YwdProtocol::getBankName(uint8_t bank)
{
    switch (bank) {
        case REG_BANK_COMM:     return "Identity / Communication";
        case REG_BANK_CTRL:     return "Control & Soft Limits";
        case REG_BANK_PID:      return "PID Gains";
        case REG_BANK_HARDLIM:  return "Hardware Limits (RO)";
        case REG_BANK_MOTOR:    return "Motor Parameters";
        case REG_BANK_TELEM:    return "Telemetry (RO)";
        case REG_BANK_VERSION:  return "Version Info (RO)";
        case REG_BANK_RESERVED: return "Reserved";
        case REG_BANK_USER:     return "Vendor-defined";
        default:                return "Unknown";
    }
}
