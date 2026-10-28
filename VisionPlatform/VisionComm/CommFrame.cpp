/**
 * @file CommFrame.cpp
 * @brief VPRT 帧的粘包和发送
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "CommFrame.h"
#include <cstring>

namespace Vision {

namespace {

const unsigned char kMagic[4] = { 'V', 'P', 'R', 'T' };
const int kHead = 10;

void putU32(std::vector<unsigned char>* buf, uint32_t v)
{
    buf->push_back(static_cast<unsigned char>((v >> 24) & 0xff));
    buf->push_back(static_cast<unsigned char>((v >> 16) & 0xff));
    buf->push_back(static_cast<unsigned char>((v >> 8) & 0xff));
    buf->push_back(static_cast<unsigned char>(v & 0xff));
}

uint32_t getU32(const unsigned char* p)
{
    return (static_cast<uint32_t>(p[0]) << 24)
        | (static_cast<uint32_t>(p[1]) << 16)
        | (static_cast<uint32_t>(p[2]) << 8)
        | static_cast<uint32_t>(p[3]);
}

}

CommFrame::CommFrame()
{
}

bool CommFrame::writeFrame(const ObjectId& channelId, const std::string& peerId, Type type,
                          const std::vector<unsigned char>& payload, std::string* err)
{
    if (payload.size() > kMaxPayload) {
        setCommErr(err, "帧负载超过 8MB");
        return false;
    }
    CommBytes bytes;
    bytes.insert(bytes.end(), kMagic, kMagic + 4);
    bytes.push_back(1);
    bytes.push_back(static_cast<unsigned char>(type));
    putU32(&bytes, static_cast<uint32_t>(payload.size()));
    bytes.insert(bytes.end(), payload.begin(), payload.end());
    return CommMgr::instance().writeBytes(channelId, peerId, bytes, err);
}

bool CommFrame::takeFrame(const std::string& peerId, Frame* out, std::string* err)
{
    std::vector<unsigned char>& buf = m_pending[peerId];
    if (buf.size() < static_cast<size_t>(kHead) || !out) {
        return false;
    }
    if (std::memcmp(buf.data(), kMagic, 4) != 0 || buf[4] != 1) {
        buf.clear();
        setCommErr(err, "帧头不是 VPRT");
        return false;
    }
    const uint32_t len = getU32(buf.data() + 6);
    if (len > kMaxPayload) {
        buf.clear();
        setCommErr(err, "帧负载超过 8MB");
        return false;
    }
    if (buf.size() < kHead + len) {
        return false;
    }
    out->type = static_cast<Type>(buf[5]);
    out->peerId = peerId;
    out->payload.assign(buf.begin() + kHead, buf.begin() + kHead + static_cast<int>(len));
    buf.erase(buf.begin(), buf.begin() + kHead + static_cast<int>(len));
    return true;
}

bool CommFrame::readFrame(const ObjectId& channelId, const std::string& peerId, int timeoutMs, Frame* out, std::string* err)
{
    if (takeFrame(peerId, out, err)) {
        return true;
    }
    if (err && !err->empty()) {
        return false;
    }
    const CommBytes chunk = CommMgr::instance().readBytes(channelId, peerId, 64 * 1024, timeoutMs, err);
    if (chunk.empty()) {
        return false;
    }
    std::vector<unsigned char>& buf = m_pending[peerId];
    buf.insert(buf.end(), chunk.begin(), chunk.end());
    return takeFrame(peerId, out, err);
}

void CommFrame::clearPeer(const std::string& peerId)
{
    m_pending.erase(peerId);
}

}
