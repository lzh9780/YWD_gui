#include "ywd_protocol.h"
#include <cstring>

// ---------------------------------------------------------------------------
// CAN-FD padding (V0.2 §2.1). V0.2 distinguishes the *DLC code* from the *data
// field length*: legal data field lengths are 0..8, 12, 16, 20, 24, 32, 48, 64.
// Per §2.1 the transmitter uses the smallest legal data length that can hold
// the semantic payload. We model `len` as the data field length (bytes); the
// driver maps it to a DLC code internally.
// ---------------------------------------------------------------------------
static constexpr uint8_t validLenLut[] = {0, 1, 2, 3, 4, 5, 6, 7, 8,
                                          12, 16, 20, 24, 32, 48, 64};

// Smallest legal CAN-FD data field length that can hold `len` semantic bytes.
static uint8_t padToValidLen(uint8_t len)
{
    for (uint8_t v : validLenLut)
        if (len <= v) return v;
    return 64;
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
    f.brs       = true;    // §2.1: BRS enabled (data phase at 5 Mbps)
    f.len       = padToValidLen(12);

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
                                     float pos_des, float vel_limit,
                                     float acc, float dec)
{
    CanFdFrame f{};
    f.id        = buildCmdId(CMD_POS_VEL_CTRL, motor_id);
    f.is_ext_id = false;
    f.is_fd     = true;
    f.brs       = true;    // §2.1: BRS enabled (data phase at 5 Mbps)
    f.len       = padToValidLen(12);

    memset(f.data, 0, sizeof(f.data));

    // P_des  int32 LE
    int32_t p = (int32_t)std::round(pos_des / m_pmax * 2147483647.0f);
    f.data[0] =  p        & 0xFF;
    f.data[1] = (p >> 8)  & 0xFF;
    f.data[2] = (p >> 16) & 0xFF;
    f.data[3] = (p >> 24) & 0xFF;

    // Vmax  uint16 LE
    uint16_t v = (uint16_t)std::round(vel_limit / m_vmax * 65535.0f);
    f.data[4] =  v        & 0xFF;
    f.data[5] = (v >> 8)  & 0xFF;

    // ACC/DEC  uint16 LE, LSB=1 rad/s², 0=use register default
    uint16_t a = (uint16_t)std::round(std::max(0.0f, std::min(acc, 65535.0f)));
    uint16_t d = (uint16_t)std::round(std::max(0.0f, std::min(dec, 65535.0f)));
    f.data[6] =  a        & 0xFF;
    f.data[7] = (a >> 8)  & 0xFF;
    f.data[8] =  d        & 0xFF;
    f.data[9] = (d >> 8)  & 0xFF;

    return f;
}

// Constant velocity control frame (0x200)   §5.3   V0.2: 6-byte data field
// B0-B1: V_des  int16 LE (round(vel / VMAX * 32767))
// B2-B3: ACC    uint16 LE (LSB=1 rad/s², 0=use default)
// B4-B5: DEC    uint16 LE (LSB=1 rad/s², 0=use default)
CanFdFrame YwdProtocol::encodeConstVel(uint8_t motor_id, float vel_des,
                                       float acc, float dec)
{
    CanFdFrame f{};
    f.id        = buildCmdId(CMD_CONST_VEL, motor_id);
    f.is_ext_id = false;
    f.is_fd     = true;
    f.brs       = true;    // §2.1: BRS enabled (data phase at 5 Mbps)
    f.len       = padToValidLen(6);   // V0.2 §5.3: 6-byte data field, no padding

    memset(f.data, 0, sizeof(f.data));

    // V_des  int16 LE
    int16_t v = (int16_t)std::round(vel_des / m_vmax * 32767.0f);
    f.data[0] =  v        & 0xFF;
    f.data[1] = (v >> 8)  & 0xFF;

    // ACC/DEC  uint16 LE, LSB=1 rad/s², 0=use register default
    uint16_t a = (uint16_t)std::round(std::max(0.0f, std::min(acc, 65535.0f)));
    uint16_t d = (uint16_t)std::round(std::max(0.0f, std::min(dec, 65535.0f)));
    f.data[2] =  a        & 0xFF;
    f.data[3] = (a >> 8)  & 0xFF;
    f.data[4] =  d        & 0xFF;
    f.data[5] = (d >> 8)  & 0xFF;

    return f;
}

// ============================================================================
// Aggregated multi-motor control frames (V0.2 §6)
//   B0 = Header (bits[2:0] = rec_cnt)  +  N × [NODE_ID(1B) + Body]
//   MIT Body (12B): p_des i32 | v_des i16 | kp u16(0.01) | kd u16(0.001) | tff i16
//   PV  Body (10B): p_des i32 | vmax u16(vel/VMAX*65535) | acc u16(LSB=1) | dec u16(LSB=1)
//   CV  Body (6B):  v_des i16 | acc u16(LSB=1) | dec u16(LSB=1)
// ============================================================================
CanFdFrame YwdProtocol::encodeAggMit(const std::vector<AggMitRecord> &recs)
{
    CanFdFrame f{};
    f.id        = 0x001;   // aggregate MIT control
    f.is_ext_id = false;
    f.is_fd     = true;
    f.brs       = true;    // §2.1: BRS enabled (data phase at 5 Mbps)

    memset(f.data, 0, sizeof(f.data));

    const int n = std::min(static_cast<int>(recs.size()), 4);
    f.data[0] = static_cast<uint8_t>(n & 0x07);   // Header: rec_cnt

    int off = 1;
    for (int i = 0; i < n; ++i) {
        const auto &r = recs[i];
        f.data[off++] = r.motor_id & 0x7F;                    // NODE_ID

        int32_t p = (int32_t)std::round(r.pos_des / m_pmax * 2147483647.0f);
        f.data[off++] =  p        & 0xFF;
        f.data[off++] = (p >> 8)  & 0xFF;
        f.data[off++] = (p >> 16) & 0xFF;
        f.data[off++] = (p >> 24) & 0xFF;

        int16_t v = (int16_t)std::round(r.vel_des / m_vmax * 32767.0f);
        f.data[off++] =  v        & 0xFF;
        f.data[off++] = (v >> 8)  & 0xFF;

        uint16_t kp = (uint16_t)std::round(r.kp / 0.01f);
        f.data[off++] =  kp       & 0xFF;
        f.data[off++] = (kp >> 8) & 0xFF;

        uint16_t kd = (uint16_t)std::round(r.kd / 0.001f);   // LSB=0.001
        f.data[off++] =  kd       & 0xFF;
        f.data[off++] = (kd >> 8) & 0xFF;

        int16_t tff = (int16_t)std::round(r.ff_torque / m_tmax * 32767.0f);
        f.data[off++] =  tff      & 0xFF;
        f.data[off++] = (tff >> 8) & 0xFF;
    }
    f.len = padToValidLen(static_cast<uint8_t>(off));
    return f;
}

CanFdFrame YwdProtocol::encodeAggPosVel(const std::vector<AggPosVelRecord> &recs)
{
    CanFdFrame f{};
    f.id        = 0x002;   // aggregate Pos-Vel control
    f.is_ext_id = false;
    f.is_fd     = true;
    f.brs       = true;    // §2.1: BRS enabled (data phase at 5 Mbps)

    memset(f.data, 0, sizeof(f.data));

    const int n = std::min(static_cast<int>(recs.size()), 4);
    f.data[0] = static_cast<uint8_t>(n & 0x07);   // Header: rec_cnt

    int off = 1;
    for (int i = 0; i < n; ++i) {
        const auto &r = recs[i];
        f.data[off++] = r.motor_id & 0x7F;                    // NODE_ID

        int32_t p = (int32_t)std::round(r.pos_des / m_pmax * 2147483647.0f);
        f.data[off++] =  p        & 0xFF;
        f.data[off++] = (p >> 8)  & 0xFF;
        f.data[off++] = (p >> 16) & 0xFF;
        f.data[off++] = (p >> 24) & 0xFF;

        // vmax normalized vel/VMAX*65535 (same as single frame); acc/dec LSB=1 rad/s²
        uint16_t vm  = (uint16_t)std::round(r.vel_limit / m_vmax * 65535.0f);
        uint16_t acc = (uint16_t)std::round(std::max(0.0f, std::min(r.acc, 65535.0f)));
        uint16_t dec = (uint16_t)std::round(std::max(0.0f, std::min(r.dec, 65535.0f)));
        f.data[off++] =  vm       & 0xFF;
        f.data[off++] = (vm >> 8) & 0xFF;
        f.data[off++] =  acc      & 0xFF;
        f.data[off++] = (acc >> 8) & 0xFF;
        f.data[off++] =  dec      & 0xFF;
        f.data[off++] = (dec >> 8) & 0xFF;
    }
    f.len = padToValidLen(static_cast<uint8_t>(off));
    return f;
}

CanFdFrame YwdProtocol::encodeAggConstVel(const std::vector<AggConstVelRecord> &recs)
{
    CanFdFrame f{};
    f.id        = 0x003;   // aggregate Const-Vel control
    f.is_ext_id = false;
    f.is_fd     = true;
    f.brs       = true;    // §2.1: BRS enabled (data phase at 5 Mbps)

    memset(f.data, 0, sizeof(f.data));

    const int n = std::min(static_cast<int>(recs.size()), 4);
    f.data[0] = static_cast<uint8_t>(n & 0x07);   // Header: rec_cnt

    int off = 1;
    for (int i = 0; i < n; ++i) {
        const auto &r = recs[i];
        f.data[off++] = r.motor_id & 0x7F;                    // NODE_ID

        int16_t v = (int16_t)std::round(r.vel_des / m_vmax * 32767.0f);
        f.data[off++] =  v        & 0xFF;
        f.data[off++] = (v >> 8)  & 0xFF;
        // acc / dec LSB=1 rad/s²
        uint16_t acc = (uint16_t)std::round(std::max(0.0f, std::min(r.acc, 65535.0f)));
        uint16_t dec = (uint16_t)std::round(std::max(0.0f, std::min(r.dec, 65535.0f)));
        f.data[off++] =  acc      & 0xFF;
        f.data[off++] = (acc >> 8) & 0xFF;
        f.data[off++] =  dec      & 0xFF;
        f.data[off++] = (dec >> 8) & 0xFF;
    }
    f.len = padToValidLen(static_cast<uint8_t>(off));
    return f;
}

// System command frame (0x400)   §10  data field = 1 byte (DLC code 1), B0=sub-command
CanFdFrame YwdProtocol::encodeSystemCmd(uint8_t motor_id,
                                        uint8_t cmd, uint8_t /*param*/)
{
    CanFdFrame f{};
    f.id      = buildCmdId(CMD_SYSTEM, motor_id);
    f.is_ext_id = false;
    f.is_fd   = true;
    f.brs     = true;    // §2.1: BRS enabled (data phase at 5 Mbps)
    f.len     = padToValidLen(1);   // §10: 1-byte data field, only B0

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
    f.brs       = true;    // §2.1: BRS enabled (data phase at 5 Mbps)
    f.len       = padToValidLen(7);   // E = 2 + 5 = 7 (legal length, no padding)

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
    f.brs       = true;    // §2.1: BRS enabled (data phase at 5 Mbps)
    f.len       = padToValidLen(3);   // E = 2 + 1 = 3 (legal length, no padding)

    memset(f.data, 0, sizeof(f.data));
    f.data[0] = 0x01;             // CMD read
    f.data[1] = 1;                // N = 1
    f.data[2] = addr & 0xFF;      // RID (1 byte)

    return f;
}

// ============================================================================
// Decoding functions
// ============================================================================

// V0.2 §7.1 feedback decode — unified for single-node and aggregate frames.
// Layout: 3-byte header (B0=N, B1=SEQ, B2=VBUS uint8 0.25V), then N records of
// 13 bytes: +0 NODE_ID | +1 STATE/MODE | +2 FAULT | +3..+6 POS int32 |
// +7..+8 VEL int16 | +9..+10 TORQUE int16 | +11 T_MOS | +12 T_ROTOR.
// The data field length must be the smallest legal length that holds E=3+13N;
// padding must be 0x00. Malformed frames (reserved bits set, N out of 1..4,
// wrong length, non-zero padding, duplicate/out-of-range NODE_ID) are dropped.
bool YwdProtocol::decodeFeedbackRecords(const CanFdFrame &frame,
                                        std::vector<FeedbackFrame> &out)
{
    out.clear();

    // Only 0x600..0x67F is feedback; 0x680..0x6FF is the parameter response.
    if ((frame.id & 0xFF80) != FRAME_FEEDBACK) return false;
    if (frame.len < 3) return false;

    const uint8_t *d = frame.data;

    // ---- Header ----
    const uint8_t b0 = d[0];
    if (b0 & 0xF8) return false;                 // reserved bits[7:3] must be 0
    const int n = b0 & 0x07;                     // record count N
    if (n < 1 || n > 4) return false;            // §11: at most 4 nodes/driver

    const uint8_t seq   = d[1];
    const uint8_t vbus  = d[2];                  // uint8, 0.25 V/LSB
    const float   voltage = static_cast<float>(vbus) * 0.25f;

    // ---- Length & padding validation ----
    const int semanticLen = 3 + 13 * n;          // E
    const uint8_t expectLen = padToValidLen(static_cast<uint8_t>(semanticLen));
    if (frame.len != expectLen) return false;
    for (int i = semanticLen; i < expectLen; ++i)
        if (d[i] != 0x00) return false;          // padding must be zero

    const uint8_t canNode = frame.id & 0x7F;     // node from the CAN ID

    // ---- Records ----
    std::vector<FeedbackFrame> recs;
    recs.reserve(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) {
        const int off = 3 + i * 13;
        FeedbackFrame fb{};
        fb.motor_id  = d[off + 0];               // NODE_ID (1..127)
        fb.state     = (d[off + 1] >> 4) & 0x0F;
        fb.mode      =  d[off + 1]       & 0x0F;
        fb.fault     =  d[off + 2];

        int32_t pos_raw = (int32_t)(static_cast<uint32_t>(d[off + 3])
                        | (static_cast<uint32_t>(d[off + 4]) << 8)
                        | (static_cast<uint32_t>(d[off + 5]) << 16)
                        | (static_cast<uint32_t>(d[off + 6]) << 24));
        // INT32_MIN is a reserved value → saturate to -PMAX (§4.2.1)
        fb.position = (pos_raw == INT32_MIN)
                          ? -m_pmax
                          : (float)pos_raw / 2147483647.0f * m_pmax;

        int16_t vel_raw = (int16_t)(d[off + 7] | (d[off + 8] << 8));
        fb.velocity = (vel_raw == INT16_MIN)
                          ? -m_vmax
                          : (float)vel_raw / 32767.0f * m_vmax;

        int16_t trq_raw = (int16_t)(d[off + 9] | (d[off + 10] << 8));
        fb.torque = (trq_raw == INT16_MIN)
                        ? -m_tmax
                        : (float)trq_raw / 32767.0f * m_tmax;

        fb.temp_mos   = (int8_t)d[off + 11];
        fb.temp_motor = (int8_t)d[off + 12];

        // Header-shared fields
        fb.voltage    = voltage;
        fb.seq        = seq;
        fb.node_count = static_cast<uint8_t>(n);

        if (fb.motor_id < 1 || fb.motor_id > 127) return false;   // out of range
        recs.push_back(fb);
    }

    // NODE_ID uniqueness within the frame (§7.1)
    for (size_t i = 0; i < recs.size(); ++i)
        for (size_t j = i + 1; j < recs.size(); ++j)
            if (recs[i].motor_id == recs[j].motor_id) return false;

    // NODE_ID vs CAN ID node field (§7.1)
    uint8_t minNode = 0xFF;
    for (const auto &r : recs)
        minNode = std::min(minNode, r.motor_id);
    if (minNode != canNode) return false;

    out = std::move(recs);
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
    // Aggregation control frames use very low IDs (0x001~0x006) — match the
    // full ID here. (Previously they sat in the switch on (canId & 0xFF00),
    // which is always 0 for these IDs, so they could never match.)
    switch (canId) {
        case FRAME_AGG_CTRL1:      return FT_AGG_CTRL1;
        case FRAME_AGG_CTRL2:      return FT_AGG_CTRL2;
        case FRAME_AGG_CTRL3:      return FT_AGG_CTRL3;
        case FRAME_AGG_CTRL_VENDOR4: return FT_AGG_CTRL_VENDOR4;
        case FRAME_AGG_CTRL_VENDOR5: return FT_AGG_CTRL_VENDOR5;
        case FRAME_AGG_CTRL_VENDOR6: return FT_AGG_CTRL_VENDOR6;
        default:                   break;
    }

    // 0x600~0x7FF range: use the 0xF80 page bits — NODE_ID is 7-bit (§2.2),
    // so bit 7 of the low byte selects the sub-channel within the page.
    uint32_t hi = canId & 0xFF00;
    if (hi == 0x600) {
        return (canId & 0x0080) ? FT_PARAM_RESP : FT_FEEDBACK;
    }
    if (hi == 0x700) {
        // 0x780|node = Vendor OTA D→H (V0.2 §12); 0x700..0x77F is reserved
        return (canId & 0x0080) ? FT_VENDOR_OTA : FT_UNKNOWN;
    }

    switch (hi) {
        case CMD_MIT_CTRL:       return FT_MIT_CTRL;
        case CMD_POS_VEL_CTRL:   return FT_POS_VEL_CTRL;
        case CMD_CONST_VEL:      return FT_CONST_VEL;
        case CMD_SYSTEM:         return FT_SYSTEM_CMD;
        case CMD_REG_OP:         return FT_REG_OP;
        case CMD_VENDOR_OTA:     return FT_VENDOR_OTA;   // 0x500 | node (§12)
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
    f.brs       = true;    // §2.1: BRS enabled (data phase at 5 Mbps)

    int n = std::min((int)rids.size(), 8);
    f.len = padToValidLen(static_cast<uint8_t>(2 + n));   // CMD(1) + N(1) + N × RID(1)
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
        {0x02, "FB_AGG",        0x00, false, "uint32"},   // V0.2: was FB_DIV
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
        {0x55, "IQ",            0x50, true,  "float32"},   // peak q-axis current
        {0x56, "ID",            0x50, true,  "float32"},   // V0.2: peak d-axis current
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
