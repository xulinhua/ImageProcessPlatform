/**
 * @file Frame.cpp
 * @brief VRMT 第 2 版帧的组包和拆包
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "Frame.h"

namespace Vision {

Frame::Frame()
{
}

FrameTx::FrameTx()
{
    for (int i = 0; i < 8; ++i) {
        m_seq[i] = 0;
    }
}

std::vector<CommBytes> FrameTx::pack(uint8_t type, uint8_t channel, const CommBytes& payload, std::string* err)
{
    const uint8_t ch = static_cast<uint8_t>(channel & 7);
    const uint32_t seq = m_seq[ch]++;
    return Frame::encodeAll(type, ch, seq, payload, err);
}

std::vector<CommBytes> FrameTx::packControl(uint8_t type, const CommBytes& payload, std::string* err)
{
    return pack(type, Frame::ChControl, payload, err);
}

void Frame::appendU8(CommBytes* out, uint8_t v)
{
    out->push_back(v);
}

void Frame::appendU16(CommBytes* out, uint16_t v)
{
    out->push_back(static_cast<unsigned char>((v >> 8) & 0xff));
    out->push_back(static_cast<unsigned char>(v & 0xff));
}

void Frame::appendU32(CommBytes* out, uint32_t v)
{
    out->push_back(static_cast<unsigned char>((v >> 24) & 0xff));
    out->push_back(static_cast<unsigned char>((v >> 16) & 0xff));
    out->push_back(static_cast<unsigned char>((v >> 8) & 0xff));
    out->push_back(static_cast<unsigned char>(v & 0xff));
}

void Frame::appendU64(CommBytes* out, uint64_t v)
{
    appendU32(out, static_cast<uint32_t>(v >> 32));
    appendU32(out, static_cast<uint32_t>(v & 0xffffffffu));
}

void Frame::appendI16(CommBytes* out, int16_t v)
{
    appendU16(out, static_cast<uint16_t>(v));
}

bool Frame::readU8(const CommBytes& in, size_t* off, uint8_t* v)
{
    if (*off >= in.size()) {
        return false;
    }
    *v = in[*off];
    *off += 1;
    return true;
}

bool Frame::readU16(const CommBytes& in, size_t* off, uint16_t* v)
{
    if (*off + 2 > in.size()) {
        return false;
    }
    *v = static_cast<uint16_t>((in[*off] << 8) | in[*off + 1]);
    *off += 2;
    return true;
}

bool Frame::readU32(const CommBytes& in, size_t* off, uint32_t* v)
{
    if (*off + 4 > in.size()) {
        return false;
    }
    *v = (static_cast<uint32_t>(in[*off]) << 24)
        | (static_cast<uint32_t>(in[*off + 1]) << 16)
        | (static_cast<uint32_t>(in[*off + 2]) << 8)
        | static_cast<uint32_t>(in[*off + 3]);
    *off += 4;
    return true;
}

bool Frame::readU64(const CommBytes& in, size_t* off, uint64_t* v)
{
    uint32_t hi = 0;
    uint32_t lo = 0;
    if (!readU32(in, off, &hi) || !readU32(in, off, &lo)) {
        return false;
    }
    *v = (static_cast<uint64_t>(hi) << 32) | lo;
    return true;
}

bool Frame::readI16(const CommBytes& in, size_t* off, int16_t* v)
{
    uint16_t u = 0;
    if (!readU16(in, off, &u)) {
        return false;
    }
    *v = static_cast<int16_t>(u);
    return true;
}

CommBytes Frame::encodeOne(uint8_t type, uint8_t channel, uint8_t flags, uint32_t seq,
                           const unsigned char* data, uint32_t len, std::string* err)
{
    CommBytes out;
    if (len > kMaxPayload) {
        if (err) {
            *err = "单片负载超过 1 MB";
        }
        return out;
    }
    out.push_back('V');
    out.push_back('R');
    out.push_back('M');
    out.push_back('T');
    out.push_back(static_cast<unsigned char>(kVersion));
    out.push_back(type);
    out.push_back(channel);
    out.push_back(flags);
    appendU32(&out, seq);
    appendU32(&out, len);
    if (len > 0 && data != nullptr) {
        out.insert(out.end(), data, data + len);
    }
    return out;
}

CommBytes Frame::encode(uint8_t type, const CommBytes& payload, std::string* err)
{
    const std::vector<CommBytes> all = encodeAll(type, ChControl, 0, payload, err);
    if (all.empty()) {
        return CommBytes();
    }
    return all.front();
}

CommBytes Frame::encode(uint8_t type, uint8_t channel, uint32_t seq, const CommBytes& payload, std::string* err)
{
    const std::vector<CommBytes> all = encodeAll(type, channel, seq, payload, err);
    if (all.empty()) {
        return CommBytes();
    }
    return all.front();
}

std::vector<CommBytes> Frame::encodeAll(uint8_t type, uint8_t channel, uint32_t seq,
                                        const CommBytes& payload, std::string* err)
{
    std::vector<CommBytes> out;
    if (payload.empty()) {
        CommBytes one = encodeOne(type, channel, FlagFin, seq, nullptr, 0, err);
        if (one.empty() && err && !err->empty()) {
            return out;
        }
        out.push_back(one);
        return out;
    }
    size_t off = 0;
    const size_t total = payload.size();
    while (off < total) {
        const size_t remain = total - off;
        const uint32_t chunk = remain > kMaxPayload ? kMaxPayload : static_cast<uint32_t>(remain);
        const bool first = (off == 0);
        const bool last = (off + chunk >= total);
        // 只填分片位；不置 FlagAck（该位预留未用，与 *Ack 报文类型无关）
        uint8_t flags = 0;
        if (!first) {
            flags |= FlagCont;
        }
        if (last) {
            flags |= FlagFin;
        }
        CommBytes one = encodeOne(type, channel, flags, seq,
                                  &payload[off], chunk, err);
        if (one.empty()) {
            out.clear();
            return out;
        }
        out.push_back(one);
        off += chunk;
    }
    return out;
}

void Frame::clear()
{
    m_pending.clear();
    for (int i = 0; i < 8; ++i) {
        m_asm[i] = Asm();
    }
}

bool Frame::takeRaw(RawFrame* out, std::string* err)
{
    if (m_pending.size() < static_cast<size_t>(kHeaderBytes)) {
        return false;
    }
    if (m_pending[0] != 'V' || m_pending[1] != 'R' || m_pending[2] != 'M' || m_pending[3] != 'T') {
        if (err) {
            *err = "帧头不是 VRMT";
        }
        return false;
    }
    if (m_pending[4] != kVersion) {
        if (err) {
            *err = "帧版本不对（需要第 2 版）";
        }
        return false;
    }
    const uint8_t type = m_pending[5];
    const uint8_t channel = m_pending[6];
    const uint8_t flags = m_pending[7];
    size_t off = 8;
    uint32_t seq = 0;
    uint32_t len = 0;
    if (!readU32(m_pending, &off, &seq) || !readU32(m_pending, &off, &len)) {
        return false;
    }
    if (len > kMaxPayload) {
        if (err) {
            *err = "负载超过 1 MB";
        }
        return false;
    }
    if (m_pending.size() < static_cast<size_t>(kHeaderBytes) + len) {
        return false;
    }
    out->type = type;
    out->channel = channel;
    out->flags = flags;
    out->seq = seq;
    out->payload.assign(m_pending.begin() + kHeaderBytes, m_pending.begin() + kHeaderBytes + len);
    m_pending.erase(m_pending.begin(), m_pending.begin() + kHeaderBytes + len);
    return true;
}

bool Frame::feedAsm(const RawFrame& raw, Packet* out, std::string* err)
{
    const uint8_t ch = static_cast<uint8_t>(raw.channel & 7);
    Asm& slot = m_asm[ch];
    // FlagAck 若被对端置位也忽略；装配只看 Cont / Fin
    const bool cont = (raw.flags & FlagCont) != 0;
    const bool fin = (raw.flags & FlagFin) != 0;

    if (!cont) {
        slot.active = true;
        slot.type = raw.type;
        slot.seq = raw.seq;
        slot.body = raw.payload;
    } else {
        if (!slot.active || slot.type != raw.type || slot.seq != raw.seq) {
            if (err) {
                *err = "分片续帧与首片不匹配";
            }
            slot = Asm();
            return false;
        }
        slot.body.insert(slot.body.end(), raw.payload.begin(), raw.payload.end());
    }

    if (!fin) {
        return false;
    }
    out->type = slot.type;
    out->channel = ch;
    out->flags = raw.flags;
    out->seq = slot.seq;
    out->payload.swap(slot.body);
    slot = Asm();
    return true;
}

bool Frame::push(const CommBytes& chunk, std::vector<Packet>* out, std::string* err)
{
    m_pending.insert(m_pending.end(), chunk.begin(), chunk.end());
    for (;;) {
        if (m_pending.size() < static_cast<size_t>(kHeaderBytes)) {
            return true;
        }
        const size_t before = m_pending.size();
        RawFrame raw;
        std::string local;
        if (!takeRaw(&raw, &local)) {
            if (!local.empty()) {
                if (err) {
                    *err = local;
                }
                m_pending.clear();
                return false;
            }
            if (m_pending.size() == before) {
                return true;
            }
            continue;
        }
        Packet one;
        std::string asmErr;
        if (feedAsm(raw, &one, &asmErr)) {
            out->push_back(one);
        } else if (!asmErr.empty()) {
            if (err) {
                *err = asmErr;
            }
            m_pending.clear();
            for (int i = 0; i < 8; ++i) {
                m_asm[i] = Asm();
            }
            return false;
        }
    }
}

}
