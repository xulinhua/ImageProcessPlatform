/**
 * @file Messages.cpp
 * @brief VRMT 第 2 版负载字段
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "Messages.h"

namespace Vision {

bool Messages::packText(CommBytes* out, const std::string& text, std::string* err)
{
    if (text.size() > static_cast<size_t>(RemoteConst::kTextMax)) {
        if (err) {
            *err = "文本超过 64 字节";
        }
        return false;
    }
    Frame::appendU8(out, static_cast<uint8_t>(text.size()));
    out->insert(out->end(), text.begin(), text.end());
    return true;
}

bool Messages::unpackText(const CommBytes& in, size_t* off, std::string* text)
{
    uint8_t n = 0;
    if (!Frame::readU8(in, off, &n)) {
        return false;
    }
    if (*off + n > in.size()) {
        return false;
    }
    // 长度为 0 时 off 可能正好在末尾，operator[] 在 Debug 下会断言
    if (n == 0) {
        text->clear();
        return true;
    }
    text->assign(reinterpret_cast<const char*>(in.data() + *off), n);
    *off += n;
    return true;
}

bool Messages::packLong(CommBytes* out, const std::string& text, std::string* err)
{
    if (text.size() > 16000) {
        if (err) {
            *err = "文本超过 16000 字节";
        }
        return false;
    }
    Frame::appendU16(out, static_cast<uint16_t>(text.size()));
    out->insert(out->end(), text.begin(), text.end());
    return true;
}

bool Messages::unpackLong(const CommBytes& in, size_t* off, std::string* text)
{
    uint16_t n = 0;
    if (!Frame::readU16(in, off, &n)) {
        return false;
    }
    if (*off + n > in.size()) {
        return false;
    }
    if (n == 0) {
        text->clear();
        return true;
    }
    text->assign(reinterpret_cast<const char*>(in.data() + *off), n);
    *off += n;
    return true;
}

CommBytes Messages::finish(uint8_t type, uint8_t channel, const CommBytes& body, std::string* err)
{
    const std::vector<CommBytes> parts = Frame::encodeAll(type, channel, 0, body, err);
    CommBytes out;
    for (size_t i = 0; i < parts.size(); ++i) {
        out.insert(out.end(), parts[i].begin(), parts[i].end());
    }
    return out;
}

bool Messages::packHelloBody(CommBytes* body, const HelloMsg& msg, std::string* err)
{
    if (!packText(body, msg.name, err) || !packText(body, msg.source, err)) {
        return false;
    }
    Frame::appendU8(body, msg.share);
    Frame::appendU8(body, msg.visible);
    Frame::appendU8(body, msg.monitor);
    if (!packText(body, msg.title, err) || !packText(body, msg.process, err) || !packText(body, msg.deviceId, err)) {
        return false;
    }
    Frame::appendU8(body, msg.monitors);
    if (!packText(body, msg.system, err)) {
        return false;
    }
    // 指纹之后再跟 1 字节：是否强制录屏。旧对端读不到就当 0
    if (!packText(body, msg.fingerprint, err)) {
        return false;
    }
    Frame::appendU8(body, msg.forceRecord);
    return true;
}

CommBytes Messages::packHello(const HelloMsg& msg, std::string* err)
{
    CommBytes body;
    if (!packHelloBody(&body, msg, err)) {
        return CommBytes();
    }
    return finish(Frame::TypeHello, Frame::ChControl, body, err);
}

bool Messages::unpackHello(const CommBytes& payload, HelloMsg* msg)
{
    size_t off = 0;
    if (!unpackText(payload, &off, &msg->name)
        || !unpackText(payload, &off, &msg->source)
        || !Frame::readU8(payload, &off, &msg->share)
        || !Frame::readU8(payload, &off, &msg->visible)
        || !Frame::readU8(payload, &off, &msg->monitor)
        || !unpackText(payload, &off, &msg->title)
        || !unpackText(payload, &off, &msg->process)
        || !unpackText(payload, &off, &msg->deviceId)
        || !Frame::readU8(payload, &off, &msg->monitors)
        || !unpackText(payload, &off, &msg->system)) {
        return false;
    }
    msg->fingerprint.clear();
    msg->forceRecord = 0;
    if (off < payload.size()) {
        unpackText(payload, &off, &msg->fingerprint);
    }
    if (off < payload.size()) {
        Frame::readU8(payload, &off, &msg->forceRecord);
    }
    return true;
}

CommBytes Messages::packHelloAck(const HelloAckMsg& msg, std::string* err)
{
    CommBytes body;
    Frame::appendU8(&body, msg.code);
    if (!packText(&body, msg.name, err) || !packText(&body, msg.fingerprint, err)) {
        return CommBytes();
    }
    return finish(Frame::TypeHelloAck, Frame::ChControl, body, err);
}

bool Messages::unpackHelloAck(const CommBytes& payload, HelloAckMsg* msg)
{
    size_t off = 0;
    if (!Frame::readU8(payload, &off, &msg->code) || !unpackText(payload, &off, &msg->name)) {
        return false;
    }
    msg->fingerprint.clear();
    if (off < payload.size()) {
        unpackText(payload, &off, &msg->fingerprint);
    }
    return true;
}

CommBytes Messages::packAuthRequest(const AuthRequestMsg& msg, std::string* err)
{
    CommBytes body;
    Frame::appendU8(&body, msg.method);
    Frame::appendU32(&body, msg.perms);
    if (!packText(&body, msg.token, err) || !packText(&body, msg.challenge, err)) {
        return CommBytes();
    }
    return finish(Frame::TypeAuthRequest, Frame::ChControl, body, err);
}

bool Messages::unpackAuthRequest(const CommBytes& payload, AuthRequestMsg* msg)
{
    size_t off = 0;
    return Frame::readU8(payload, &off, &msg->method)
        && Frame::readU32(payload, &off, &msg->perms)
        && unpackText(payload, &off, &msg->token)
        && unpackText(payload, &off, &msg->challenge);
}

CommBytes Messages::packAuthResult(const AuthResultMsg& msg, std::string* err)
{
    CommBytes body;
    Frame::appendU8(&body, msg.code);
    Frame::appendU32(&body, msg.perms);
    Frame::appendU8(&body, msg.triesLeft);
    Frame::appendU32(&body, msg.lockSec);
    if (!packText(&body, msg.challenge, err)) {
        return CommBytes();
    }
    return finish(Frame::TypeAuthResult, Frame::ChControl, body, err);
}

bool Messages::unpackAuthResult(const CommBytes& payload, AuthResultMsg* msg)
{
    size_t off = 0;
    return Frame::readU8(payload, &off, &msg->code)
        && Frame::readU32(payload, &off, &msg->perms)
        && Frame::readU8(payload, &off, &msg->triesLeft)
        && Frame::readU32(payload, &off, &msg->lockSec)
        && unpackText(payload, &off, &msg->challenge);
}

CommBytes Messages::packHeartbeat(const HeartbeatMsg& msg, std::string* err)
{
    CommBytes body;
    Frame::appendU64(&body, msg.ms);
    return finish(Frame::TypeHeartbeat, Frame::ChControl, body, err);
}

bool Messages::unpackHeartbeat(const CommBytes& payload, HeartbeatMsg* msg)
{
    size_t off = 0;
    return Frame::readU64(payload, &off, &msg->ms);
}

CommBytes Messages::packHeartbeatAck(const HeartbeatAckMsg& msg, std::string* err)
{
    CommBytes body;
    Frame::appendU64(&body, msg.peerMs);
    Frame::appendU64(&body, msg.selfMs);
    return finish(Frame::TypeHeartbeatAck, Frame::ChControl, body, err);
}

bool Messages::unpackHeartbeatAck(const CommBytes& payload, HeartbeatAckMsg* msg)
{
    size_t off = 0;
    return Frame::readU64(payload, &off, &msg->peerMs) && Frame::readU64(payload, &off, &msg->selfMs);
}

CommBytes Messages::packPicture(const PictureMsg& msg, std::string* err)
{
    return packPicture(msg, Frame::ChVideo, err);
}

CommBytes Messages::packPicture(const PictureMsg& msg, uint8_t channel, std::string* err)
{
    CommBytes body;
    Frame::appendU8(&body, msg.visible);
    Frame::appendU8(&body, msg.operable);
    Frame::appendU32(&body, msg.clientW);
    Frame::appendU32(&body, msg.clientH);
    Frame::appendU32(&body, msg.imageW);
    Frame::appendU32(&body, msg.imageH);
    Frame::appendU8(&body, msg.codec == 0 ? 1 : msg.codec);
    Frame::appendU8(&body, msg.key);
    body.insert(body.end(), msg.jpeg.begin(), msg.jpeg.end());
    return finish(Frame::TypeVideoFrame, channel, body, err);
}

CommBytes Messages::packSubscribe(const SubscribeMsg& msg, std::string* err)
{
    CommBytes body;
    Frame::appendU8(&body, msg.stream);
    Frame::appendU8(&body, msg.enable);
    Frame::appendU16(&body, msg.maxFps);
    Frame::appendU8(&body, msg.jpegQuality);
    Frame::appendU16(&body, msg.longEdge);
    Frame::appendU8(&body, msg.codec);
    return finish(Frame::TypeSubscribe, Frame::ChControl, body, err);
}

bool Messages::unpackSubscribe(const CommBytes& payload, SubscribeMsg* msg)
{
    size_t off = 0;
    if (!Frame::readU8(payload, &off, &msg->stream)
        || !Frame::readU8(payload, &off, &msg->enable)
        || !Frame::readU16(payload, &off, &msg->maxFps)
        || !Frame::readU8(payload, &off, &msg->jpegQuality)
        || !Frame::readU16(payload, &off, &msg->longEdge)) {
        return false;
    }
    msg->codec = 0;
    if (off < payload.size()) {
        if (!Frame::readU8(payload, &off, &msg->codec)) {
            return false;
        }
    }
    return true;
}

CommBytes Messages::packStreamInfo(const StreamInfoMsg& msg, std::string* err)
{
    CommBytes body;
    Frame::appendU8(&body, msg.stream);
    Frame::appendU8(&body, msg.codec);
    Frame::appendU16(&body, msg.width);
    Frame::appendU16(&body, msg.height);
    Frame::appendU8(&body, msg.fps);
    return finish(Frame::TypeStreamInfo, Frame::ChControl, body, err);
}

bool Messages::unpackStreamInfo(const CommBytes& payload, StreamInfoMsg* msg)
{
    size_t off = 0;
    return Frame::readU8(payload, &off, &msg->stream)
        && Frame::readU8(payload, &off, &msg->codec)
        && Frame::readU16(payload, &off, &msg->width)
        && Frame::readU16(payload, &off, &msg->height)
        && Frame::readU8(payload, &off, &msg->fps);
}

CommBytes Messages::packNetStats(const NetStatsMsg& msg, std::string* err)
{
    CommBytes body;
    Frame::appendU16(&body, msg.rttMs);
    Frame::appendU8(&body, msg.congested);
    return finish(Frame::TypeNetStats, Frame::ChControl, body, err);
}

bool Messages::unpackNetStats(const CommBytes& payload, NetStatsMsg* msg)
{
    size_t off = 0;
    return Frame::readU16(payload, &off, &msg->rttMs) && Frame::readU8(payload, &off, &msg->congested);
}

CommBytes Messages::packCursorPos(const CursorPosMsg& msg, std::string* err)
{
    CommBytes body;
    Frame::appendU16(&body, msg.x);
    Frame::appendU16(&body, msg.y);
    Frame::appendU8(&body, msg.visible);
    return finish(Frame::TypeCursorPos, Frame::ChCursor, body, err);
}

CommBytes Messages::packCursorShape(const CursorShapeMsg& msg, std::string* err)
{
    CommBytes body;
    Frame::appendU16(&body, msg.hotX);
    Frame::appendU16(&body, msg.hotY);
    Frame::appendU16(&body, msg.width);
    Frame::appendU16(&body, msg.height);
    body.insert(body.end(), msg.png.begin(), msg.png.end());
    return finish(Frame::TypeCursorShape, Frame::ChCursor, body, err);
}

bool Messages::unpackCursorShape(const CommBytes& payload, CursorShapeMsg* msg)
{
    size_t off = 0;
    if (!Frame::readU16(payload, &off, &msg->hotX)
        || !Frame::readU16(payload, &off, &msg->hotY)
        || !Frame::readU16(payload, &off, &msg->width)
        || !Frame::readU16(payload, &off, &msg->height)) {
        return false;
    }
    if (msg->width > 128 || msg->height > 128) {
        return false;
    }
    msg->png.assign(payload.begin() + static_cast<std::ptrdiff_t>(off), payload.end());
    return msg->png.size() <= 64 * 1024;
}

bool Messages::unpackCursorPos(const CommBytes& payload, CursorPosMsg* msg)
{
    size_t off = 0;
    return Frame::readU16(payload, &off, &msg->x)
        && Frame::readU16(payload, &off, &msg->y)
        && Frame::readU8(payload, &off, &msg->visible);
}

bool Messages::unpackPicture(const CommBytes& payload, PictureMsg* msg)
{
    size_t off = 0;
    if (!Frame::readU8(payload, &off, &msg->visible)
        || !Frame::readU8(payload, &off, &msg->operable)
        || !Frame::readU32(payload, &off, &msg->clientW)
        || !Frame::readU32(payload, &off, &msg->clientH)
        || !Frame::readU32(payload, &off, &msg->imageW)
        || !Frame::readU32(payload, &off, &msg->imageH)
        || !Frame::readU8(payload, &off, &msg->codec)
        || !Frame::readU8(payload, &off, &msg->key)) {
        return false;
    }
    msg->jpeg.assign(payload.begin() + static_cast<std::ptrdiff_t>(off), payload.end());
    return true;
}

CommBytes Messages::packInput(const InputMsg& msg, std::string* err)
{
    CommBytes body;
    Frame::appendU16(&body, msg.x);
    Frame::appendU16(&body, msg.y);
    Frame::appendU8(&body, msg.buttons);
    Frame::appendI16(&body, msg.wheel);
    Frame::appendU16(&body, msg.vk);
    Frame::appendU8(&body, msg.down);
    Frame::appendU8(&body, msg.action);
    const uint8_t type = msg.vk != 0 ? static_cast<uint8_t>(Frame::TypeKey) : static_cast<uint8_t>(Frame::TypeMouse);
    return finish(type, Frame::ChInput, body, err);
}

bool Messages::unpackInput(const CommBytes& payload, InputMsg* msg)
{
    size_t off = 0;
    if (!Frame::readU16(payload, &off, &msg->x)
        || !Frame::readU16(payload, &off, &msg->y)
        || !Frame::readU8(payload, &off, &msg->buttons)
        || !Frame::readI16(payload, &off, &msg->wheel)
        || !Frame::readU16(payload, &off, &msg->vk)
        || !Frame::readU8(payload, &off, &msg->down)) {
        return false;
    }
    if (!Frame::readU8(payload, &off, &msg->action)) {
        msg->action = InputMsg::ActionLegacy;
    }
    return true;
}

CommBytes Messages::packOperate(const OperateMsg& msg, std::string* err)
{
    CommBytes body;
    Frame::appendU8(&body, msg.action);
    if (!msg.who.empty() && !packLong(&body, msg.who, err)) {
        return CommBytes();
    }
    return finish(Frame::TypeOperate, Frame::ChControl, body, err);
}

bool Messages::unpackOperate(const CommBytes& payload, OperateMsg* msg)
{
    size_t off = 0;
    if (!Frame::readU8(payload, &off, &msg->action)) {
        return false;
    }
    msg->who.clear();
    if (off < payload.size() && !unpackLong(payload, &off, &msg->who)) {
        return false;
    }
    return true;
}

CommBytes Messages::packSpecial(const SpecialMsg& msg, std::string* err)
{
    CommBytes body;
    Frame::appendU8(&body, msg.kind);
    return finish(Frame::TypeSpecial, Frame::ChInput, body, err);
}

bool Messages::unpackSpecial(const CommBytes& payload, SpecialMsg* msg)
{
    size_t off = 0;
    return Frame::readU8(payload, &off, &msg->kind);
}

CommBytes Messages::packEmpty(uint8_t type, std::string* err)
{
    return finish(type, Frame::ChControl, CommBytes(), err);
}

CommBytes Messages::packEmpty(uint8_t type, uint8_t channel, std::string* err)
{
    return finish(type, channel, CommBytes(), err);
}

CommBytes Messages::packConfig(const ConfigMsg& msg, std::string* err)
{
    CommBytes body;
    Frame::appendU8(&body, msg.share);
    Frame::appendU8(&body, msg.monitor);
    if (!packText(&body, msg.title, err) || !packText(&body, msg.process, err)) {
        return CommBytes();
    }
    // 尾部可选：旧被控端忽略；新字段控制帧率与画质（maxFps 为 u16，支持到 500）
    Frame::appendU16(&body, msg.maxFps);
    Frame::appendU8(&body, msg.jpegQuality);
    Frame::appendU16(&body, msg.longEdge);
    Frame::appendU8(&body, msg.codec);
    return finish(Frame::TypeConfig, Frame::ChControl, body, err);
}

bool Messages::unpackConfig(const CommBytes& payload, ConfigMsg* msg)
{
    size_t off = 0;
    if (!Frame::readU8(payload, &off, &msg->share)
        || !Frame::readU8(payload, &off, &msg->monitor)
        || !unpackText(payload, &off, &msg->title)
        || !unpackText(payload, &off, &msg->process)) {
        return false;
    }
    msg->maxFps = 0;
    msg->jpegQuality = 0;
    msg->longEdge = 0;
    msg->codec = 0;
    if (off + 1 < payload.size()) {
        if (!Frame::readU16(payload, &off, &msg->maxFps)) {
            return false;
        }
    }
    if (off < payload.size()) {
        if (!Frame::readU8(payload, &off, &msg->jpegQuality)) {
            return false;
        }
    }
    if (off + 1 < payload.size()) {
        if (!Frame::readU16(payload, &off, &msg->longEdge)) {
            return false;
        }
    }
    if (off < payload.size()) {
        if (!Frame::readU8(payload, &off, &msg->codec)) {
            return false;
        }
    }
    return true;
}

CommBytes Messages::packAnnounce(const HelloMsg& msg, std::string* err)
{
    CommBytes body;
    if (!packHelloBody(&body, msg, err)) {
        return CommBytes();
    }
    return finish(Frame::TypeAnnounce, Frame::ChControl, body, err);
}

bool Messages::unpackAnnounce(const CommBytes& payload, HelloMsg* msg)
{
    return unpackHello(payload, msg);
}

CommBytes Messages::packClipText(const std::string& text, std::string* err)
{
    CommBytes body;
    if (!packLong(&body, text, err)) {
        return CommBytes();
    }
    return finish(Frame::TypeClipText, Frame::ChClip, body, err);
}

bool Messages::unpackClipText(const CommBytes& payload, std::string* text)
{
    size_t off = 0;
    return unpackLong(payload, &off, text);
}

CommBytes Messages::packClipImage(const CommBytes& png, std::string* err)
{
    if (png.size() > 512 * 1024) {
        if (err) {
            *err = "剪贴板图片超过 512KB";
        }
        return CommBytes();
    }
    CommBytes body;
    Frame::appendU32(&body, static_cast<uint32_t>(png.size()));
    body.insert(body.end(), png.begin(), png.end());
    return finish(Frame::TypeClipImage, Frame::ChClip, body, err);
}

bool Messages::unpackClipImage(const CommBytes& payload, CommBytes* png)
{
    size_t off = 0;
    uint32_t n = 0;
    if (!Frame::readU32(payload, &off, &n) || off + n > payload.size() || n > 512 * 1024) {
        return false;
    }
    png->assign(payload.begin() + static_cast<std::ptrdiff_t>(off), payload.begin() + static_cast<std::ptrdiff_t>(off + n));
    return true;
}

CommBytes Messages::packClipFiles(const std::string& paths, std::string* err)
{
    CommBytes body;
    if (!packLong(&body, paths, err)) {
        return CommBytes();
    }
    return finish(Frame::TypeClipFiles, Frame::ChClip, body, err);
}

bool Messages::unpackClipFiles(const CommBytes& payload, std::string* paths)
{
    size_t off = 0;
    return unpackLong(payload, &off, paths);
}

CommBytes Messages::packChat(const std::string& name, const std::string& text, std::string* err, uint8_t kind)
{
    CommBytes body;
    if (!packLong(&body, name, err) || !packLong(&body, text, err)) {
        return CommBytes();
    }
    Frame::appendU8(&body, kind == 1 ? 1 : 0);
    return finish(Frame::TypeChatText, Frame::ChChat, body, err);
}

bool Messages::unpackChat(const CommBytes& payload, std::string* name, std::string* text, uint8_t* kind)
{
    size_t off = 0;
    if (!unpackLong(payload, &off, name) || !unpackLong(payload, &off, text)) {
        return false;
    }
    uint8_t got = 0;
    if (off < payload.size()) {
        Frame::readU8(payload, &off, &got);
    }
    if (kind) {
        *kind = got == 1 ? 1 : 0;
    }
    return true;
}

CommBytes Messages::packPower(uint8_t action, std::string* err)
{
    CommBytes body;
    Frame::appendU8(&body, action);
    return finish(Frame::TypePower, Frame::ChControl, body, err);
}

bool Messages::unpackPower(const CommBytes& payload, uint8_t* action)
{
    size_t off = 0;
    return Frame::readU8(payload, &off, action);
}

CommBytes Messages::packPrivacy(uint8_t on, std::string* err)
{
    CommBytes body;
    Frame::appendU8(&body, on);
    return finish(Frame::TypePrivacy, Frame::ChControl, body, err);
}

bool Messages::unpackPrivacy(const CommBytes& payload, uint8_t* on)
{
    size_t off = 0;
    return Frame::readU8(payload, &off, on);
}

CommBytes Messages::packSysInfo(const std::string& text, std::string* err)
{
    CommBytes body;
    if (!packLong(&body, text, err)) {
        return CommBytes();
    }
    return finish(Frame::TypeSysInfo, Frame::ChControl, body, err);
}

bool Messages::unpackSysInfo(const CommBytes& payload, std::string* text)
{
    size_t off = 0;
    return unpackLong(payload, &off, text);
}

CommBytes Messages::packListDir(const std::string& path, std::string* err)
{
    CommBytes body;
    if (!packLong(&body, path, err)) {
        return CommBytes();
    }
    return finish(Frame::TypeListDir, Frame::ChFile, body, err);
}

bool Messages::unpackListDir(const CommBytes& payload, std::string* path)
{
    size_t off = 0;
    return unpackLong(payload, &off, path);
}

CommBytes Messages::packDirEntries(const std::string& listing, std::string* err)
{
    CommBytes body;
    if (!packLong(&body, listing, err)) {
        return CommBytes();
    }
    return finish(Frame::TypeDirEntries, Frame::ChFile, body, err);
}

bool Messages::unpackDirEntries(const CommBytes& payload, std::string* listing)
{
    size_t off = 0;
    return unpackLong(payload, &off, listing);
}

CommBytes Messages::packMkDir(uint32_t id, const std::string& path, std::string* err)
{
    CommBytes body;
    Frame::appendU32(&body, id);
    if (!packLong(&body, path, err)) {
        return CommBytes();
    }
    return finish(Frame::TypeMkDir, Frame::ChFile, body, err);
}

bool Messages::unpackMkDir(const CommBytes& payload, uint32_t* id, std::string* path)
{
    size_t off = 0;
    return Frame::readU32(payload, &off, id) && unpackLong(payload, &off, path);
}

CommBytes Messages::packFileOpen(uint32_t id, uint8_t pull, const std::string& path, uint64_t size, std::string* err,
    const std::string& sha256, uint64_t resume)
{
    CommBytes body;
    Frame::appendU32(&body, id);
    Frame::appendU8(&body, pull);
    if (!packLong(&body, path, err)) {
        return CommBytes();
    }
    Frame::appendU64(&body, size);
    if (!packLong(&body, sha256, err)) {
        return CommBytes();
    }
    Frame::appendU64(&body, resume);
    return finish(Frame::TypeFileOpen, Frame::ChFile, body, err);
}

bool Messages::unpackFileOpen(const CommBytes& payload, uint32_t* id, uint8_t* pull, std::string* path, uint64_t* size,
    std::string* sha256, uint64_t* resume)
{
    size_t off = 0;
    if (!Frame::readU32(payload, &off, id)
        || !Frame::readU8(payload, &off, pull)
        || !unpackLong(payload, &off, path)
        || !Frame::readU64(payload, &off, size)) {
        return false;
    }
    if (sha256) {
        sha256->clear();
    }
    if (resume) {
        *resume = 0;
    }
    if (off < payload.size()) {
        std::string sha;
        if (!unpackLong(payload, &off, &sha)) {
            return false;
        }
        if (sha256) {
            *sha256 = sha;
        }
    }
    if (off + 8 <= payload.size()) {
        uint64_t at = 0;
        if (!Frame::readU64(payload, &off, &at)) {
            return false;
        }
        if (resume) {
            *resume = at;
        }
    }
    return true;
}

CommBytes Messages::packFileAck(uint32_t id, uint64_t offset, std::string* err)
{
    CommBytes body;
    Frame::appendU32(&body, id);
    Frame::appendU64(&body, offset);
    return finish(Frame::TypeFileAck, Frame::ChFile, body, err);
}

bool Messages::unpackFileAck(const CommBytes& payload, uint32_t* id, uint64_t* offset)
{
    size_t off = 0;
    return Frame::readU32(payload, &off, id) && Frame::readU64(payload, &off, offset);
}

CommBytes Messages::packViewers(const std::string& listing, std::string* err)
{
    CommBytes body;
    if (!packLong(&body, listing, err)) {
        return CommBytes();
    }
    return finish(Frame::TypeViewers, Frame::ChControl, body, err);
}

bool Messages::unpackViewers(const CommBytes& payload, std::string* listing)
{
    size_t off = 0;
    return unpackLong(payload, &off, listing);
}

CommBytes Messages::packInvite(const std::string& host, uint16_t port, std::string* err)
{
    CommBytes body;
    if (!packLong(&body, host, err)) {
        return CommBytes();
    }
    Frame::appendU16(&body, port);
    return finish(Frame::TypeInvite, Frame::ChControl, body, err);
}

bool Messages::unpackInvite(const CommBytes& payload, std::string* host, uint16_t* port)
{
    size_t off = 0;
    return unpackLong(payload, &off, host) && Frame::readU16(payload, &off, port);
}

CommBytes Messages::packFileChunk(uint32_t id, uint32_t offset, const CommBytes& data, std::string* err)
{
    if (data.size() > static_cast<size_t>(RemoteConst::kFileChunk)) {
        if (err) {
            *err = "文件块超过 256KB";
        }
        return CommBytes();
    }
    CommBytes body;
    Frame::appendU32(&body, id);
    Frame::appendU32(&body, offset);
    body.insert(body.end(), data.begin(), data.end());
    return finish(Frame::TypeFileChunk, Frame::ChFile, body, err);
}

bool Messages::unpackFileChunk(const CommBytes& payload, uint32_t* id, uint32_t* offset, CommBytes* data)
{
    size_t off = 0;
    if (!Frame::readU32(payload, &off, id) || !Frame::readU32(payload, &off, offset) || off > payload.size()) {
        return false;
    }
    data->assign(payload.begin() + static_cast<std::ptrdiff_t>(off), payload.end());
    return data->size() <= static_cast<size_t>(RemoteConst::kFileChunk);
}

CommBytes Messages::packFileDone(uint32_t id, uint8_t ok, const std::string& note, std::string* err)
{
    CommBytes body;
    Frame::appendU32(&body, id);
    Frame::appendU8(&body, ok);
    if (!packLong(&body, note, err)) {
        return CommBytes();
    }
    return finish(Frame::TypeFileDone, Frame::ChFile, body, err);
}

bool Messages::unpackFileDone(const CommBytes& payload, uint32_t* id, uint8_t* ok, std::string* note)
{
    size_t off = 0;
    return Frame::readU32(payload, &off, id)
        && Frame::readU8(payload, &off, ok)
        && unpackLong(payload, &off, note);
}

}
