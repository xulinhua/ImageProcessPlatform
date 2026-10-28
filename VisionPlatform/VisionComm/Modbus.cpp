/**
 * @file Modbus.cpp
 * @brief Modbus实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "Modbus.h"
#include "TextEncoding.h"
#include "CommWin.h"
#include <cstdio>
#include <cstring>

namespace Vision {

namespace {

// 按大端追加一个 16 位值
void append16(CommBytes* b, int v)
{
    b->push_back(static_cast<unsigned char>((v >> 8) & 0xFF));
    b->push_back(static_cast<unsigned char>(v & 0xFF));
}

// 按大端读一个 16 位值
int read16(const unsigned char* p)
{
    return (static_cast<int>(p[0]) << 8) | static_cast<int>(p[1]);
}

// 计算 MODBUS RTU 的 CRC
uint16_t crc16(const unsigned char* data, int n)
{
    uint16_t crc = 0xFFFF;
    for (int i = 0; i < n; ++i) {
        crc ^= data[i];
        for (int b = 0; b < 8; ++b) {
            if (crc & 1) {
                crc = static_cast<uint16_t>((crc >> 1) ^ 0xA001);
            } else {
                crc = static_cast<uint16_t>(crc >> 1);
            }
        }
    }
    return crc;
}

// 地址和数量是否落在合法范围
bool rangeOk(int address, int count, int maxCount, std::string* err)
{
    if (address < 0 || count <= 0 || count > maxCount || address > 65535 || count > 65536 - address) {
        setCommErr(err, "参数无效");
        return false;
    }
    return true;
}

// 给 PDU 套上 MBAP 或 RTU 帧头和校验
CommBytes buildAdu(bool tcp, uint16_t tid, int slave, const CommBytes& pdu)
{
    CommBytes out;
    if (tcp) {
        append16(&out, tid);
        append16(&out, 0);
        append16(&out, static_cast<int>(pdu.size()) + 1);
        out.push_back(static_cast<unsigned char>(slave & 0xFF));
        out.insert(out.end(), pdu.begin(), pdu.end());
        return out;
    }
    out.push_back(static_cast<unsigned char>(slave & 0xFF));
    out.insert(out.end(), pdu.begin(), pdu.end());
    const uint16_t c = crc16(&out[0], static_cast<int>(out.size()));
    out.push_back(static_cast<unsigned char>(c & 0xFF));
    out.push_back(static_cast<unsigned char>((c >> 8) & 0xFF));
    return out;
}

// 读到一整帧；TCP 按长度，RTU 至少 4 字节
bool readUntil(ICommChannel* ch, const std::string& peerId, bool tcp, int timeoutMs, CommBytes* frame, std::string* err)
{
    frame->clear();
    CommBytes acc;
    const int budget = timeoutMs <= 0 ? 1000 : timeoutMs;
    DWORD start = GetTickCount();
    while (static_cast<int>(GetTickCount() - start) < budget) {
        const int left = budget - static_cast<int>(GetTickCount() - start);
        std::string readErr;
        const CommBytes part = ch->read(peerId, 256, left > 0 ? left : 1, &readErr);
        if (part.empty()) {
            if (!acc.empty()) {
                break;
            }
            setCommErr(err, readErr.empty() ? "读超时" : readErr);
            return false;
        }
        acc.insert(acc.end(), part.begin(), part.end());
        if (!tcp) {
            if (acc.size() >= 4) {
                *frame = acc;
                return true;
            }
            continue;
        }
        if (acc.size() < 6) {
            continue;
        }
        const int len = read16(&acc[4]);
        if (len < 2 || len > 260) {
            setCommErr(err, "MODBUS-TCP 长度无效");
            return false;
        }
        if (static_cast<int>(acc.size()) >= 6 + len) {
            frame->assign(acc.begin(), acc.begin() + 6 + len);
            return true;
        }
    }
    setCommErr(err, "读超时");
    return false;
}

// 发一帧并收回功能码匹配的 PDU
bool transact(ICommChannel* ch, const std::string& peerId, bool tcp, uint16_t tid, int slave,
              const CommBytes& pdu, CommBytes* respPdu, std::string* err)
{
    if (!ch || !ch->isOpen()) {
        setCommErr(err, "通道未打开");
        return false;
    }
    ch->drain(peerId);
    const CommBytes req = buildAdu(tcp, tid, slave, pdu);
    if (!ch->write(peerId, req, 1000, err)) {
        return false;
    }
    CommBytes frame;
    if (!readUntil(ch, peerId, tcp, 1000, &frame, err)) {
        return false;
    }
    CommBytes body;
    if (tcp) {
        if (frame.size() < 8) {
            setCommErr(err, "MODBUS-TCP 响应过短");
            return false;
        }
        const int gotTid = read16(&frame[0]);
        if (gotTid != tid) {
            setCommErr(err, "事务号不匹配");
            return false;
        }
        body.assign(frame.begin() + 7, frame.end());
    } else {
        if (frame.size() < 4) {
            setCommErr(err, "MODBUS-RTU 响应过短");
            return false;
        }
        const uint16_t c = crc16(&frame[0], static_cast<int>(frame.size()) - 2);
        const uint16_t got = static_cast<uint16_t>(frame[frame.size() - 2]
                                                    | (static_cast<uint16_t>(frame[frame.size() - 1]) << 8));
        if (c != got) {
            setCommErr(err, "CRC 校验失败");
            return false;
        }
        if (frame[0] != static_cast<unsigned char>(slave & 0xFF)) {
            setCommErr(err, "从站号不匹配");
            return false;
        }
        body.assign(frame.begin() + 1, frame.end() - 2);
    }
    if (body.empty()) {
        setCommErr(err, "应答为空");
        return false;
    }
    const unsigned char fc = pdu[0];
    if (body[0] == static_cast<unsigned char>(fc | 0x80)) {
        const int code = body.size() > 1 ? body[1] : 0;
        char buf[64];
        sprintf_s(buf, "MODBUS 异常码 %d", code);
        setCommErr(err, buf);
        return false;
    }
    if (body[0] != fc) {
        setCommErr(err, "功能码不匹配");
        return false;
    }
    *respPdu = body;
    return true;
}

// 从读位应答里拆出每一位
bool parseBits(const CommBytes& pdu, int count, std::vector<unsigned char>* bits, std::string* err)
{
    if (pdu.size() < 2) {
        setCommErr(err, "数据长度不足");
        return false;
    }
    const int byteCount = pdu[1];
    if (static_cast<int>(pdu.size()) < 2 + byteCount) {
        setCommErr(err, "数据长度不足");
        return false;
    }
    bits->clear();
    for (int i = 0; i < count; ++i) {
        const int byte = pdu[2 + (i / 8)];
        bits->push_back((byte & (1 << (i % 8))) ? 1 : 0);
    }
    return true;
}

// 从读寄存器应答里拆出各寄存器
bool parseRegs(const CommBytes& pdu, int count, std::vector<uint16_t>* regs, std::string* err)
{
    if (pdu.size() < 2) {
        setCommErr(err, "数据长度不足");
        return false;
    }
    const int byteCount = pdu[1];
    if (byteCount < count * 2 || static_cast<int>(pdu.size()) < 2 + byteCount) {
        setCommErr(err, "数据长度不足");
        return false;
    }
    regs->clear();
    for (int i = 0; i < count; ++i) {
        regs->push_back(static_cast<uint16_t>(read16(&pdu[2 + i * 2])));
    }
    return true;
}

} // namespace

// 主站读线圈或离散输入
bool modbusMasterReadBits(ICommChannel* ch, const std::string& peerId, bool tcp, uint16_t tid,
                          int slave, int fc, int address, int count,
                          std::vector<unsigned char>* bits, std::string* err)
{
    if (!bits || !rangeOk(address, count, 2000, err)) {
        if (!bits) {
            setCommErr(err, "参数无效");
        }
        return false;
    }
    CommBytes pdu;
    pdu.push_back(static_cast<unsigned char>(fc));
    append16(&pdu, address);
    append16(&pdu, count);
    CommBytes resp;
    if (!transact(ch, peerId, tcp, tid, slave, pdu, &resp, err)) {
        return false;
    }
    return parseBits(resp, count, bits, err);
}

// 主站读保持寄存器或输入寄存器
bool modbusMasterReadRegs(ICommChannel* ch, const std::string& peerId, bool tcp, uint16_t tid,
                          int slave, int fc, int address, int count,
                          std::vector<uint16_t>* regs, std::string* err)
{
    if (!regs || !rangeOk(address, count, 125, err)) {
        if (!regs) {
            setCommErr(err, "参数无效");
        }
        return false;
    }
    CommBytes pdu;
    pdu.push_back(static_cast<unsigned char>(fc));
    append16(&pdu, address);
    append16(&pdu, count);
    CommBytes resp;
    if (!transact(ch, peerId, tcp, tid, slave, pdu, &resp, err)) {
        return false;
    }
    return parseRegs(resp, count, regs, err);
}

// 主站写线圈，单个或连续多个
bool modbusMasterWriteBits(ICommChannel* ch, const std::string& peerId, bool tcp, uint16_t tid,
                           int slave, int address, const std::vector<unsigned char>& bits, std::string* err)
{
    const int count = static_cast<int>(bits.size());
    if (!rangeOk(address, count, 1968, err)) {
        return false;
    }
    CommBytes pdu;
    if (count == 1) {
        pdu.push_back(0x05);
        append16(&pdu, address);
        append16(&pdu, bits[0] ? 0xFF00 : 0x0000);
    } else {
        pdu.push_back(0x0F);
        append16(&pdu, address);
        append16(&pdu, count);
        const int byteCount = (count + 7) / 8;
        pdu.push_back(static_cast<unsigned char>(byteCount));
        for (int i = 0; i < byteCount; ++i) {
            unsigned char byte = 0;
            for (int b = 0; b < 8; ++b) {
                const int idx = i * 8 + b;
                if (idx < count && bits[static_cast<size_t>(idx)]) {
                    byte = static_cast<unsigned char>(byte | (1 << b));
                }
            }
            pdu.push_back(byte);
        }
    }
    CommBytes resp;
    return transact(ch, peerId, tcp, tid, slave, pdu, &resp, err);
}

// 主站写寄存器，单个或连续多个
bool modbusMasterWriteRegs(ICommChannel* ch, const std::string& peerId, bool tcp, uint16_t tid,
                           int slave, int address, const std::vector<uint16_t>& regs, std::string* err)
{
    const int count = static_cast<int>(regs.size());
    if (!rangeOk(address, count, 123, err)) {
        return false;
    }
    CommBytes pdu;
    if (count == 1) {
        pdu.push_back(0x06);
        append16(&pdu, address);
        append16(&pdu, regs[0]);
    } else {
        pdu.push_back(0x10);
        append16(&pdu, address);
        append16(&pdu, count);
        pdu.push_back(static_cast<unsigned char>(count * 2));
        for (int i = 0; i < count; ++i) {
            append16(&pdu, regs[static_cast<size_t>(i)]);
        }
    }
    CommBytes resp;
    return transact(ch, peerId, tcp, tid, slave, pdu, &resp, err);
}

// 分配四块映像
ModbusSlave::ModbusSlave()
    : m_ch(nullptr)
    , m_slaveId(1)
    , m_run(false)
{
    m_coils.assign(kMapSize, 0);
    m_discrete.assign(kMapSize, 0);
    m_holding.assign(kMapSize, 0);
    m_input.assign(kMapSize, 0);
}

// 析构时停止应答线程
ModbusSlave::~ModbusSlave()
{
    stop();
}

// 挂到通道上并开始收请求
void ModbusSlave::start(ICommChannel* channel, int slaveId)
{
    stop();
    m_ch = channel;
    m_slaveId = slaveId < 0 ? 1 : slaveId;
    m_run = true;
    m_thread = std::thread(&ModbusSlave::loop, this);
}

// 停线程并卸下通道
void ModbusSlave::stop()
{
    m_run = false;
    if (m_thread.joinable()) {
        m_thread.join();
    }
    m_ch = 0;
    m_rx.clear();
}

// 读本机线圈或离散输入映像
bool ModbusSlave::readBits(ModbusTable table, int address, int count, std::vector<unsigned char>* bits, std::string* err)
{
    if (!bits || (table != ModbusCoil && table != ModbusDiscrete) || !rangeOk(address, count, 2000, err)) {
        setCommErr(err, "参数无效");
        return false;
    }
    if (address + count > kMapSize) {
        setCommErr(err, "地址超出从站映像");
        return false;
    }
    const std::vector<unsigned char>& src = (table == ModbusCoil) ? m_coils : m_discrete;
    std::lock_guard<std::mutex> lock(m_mu);
    bits->assign(src.begin() + address, src.begin() + address + count);
    return true;
}

// 读本机保持或输入寄存器映像
bool ModbusSlave::readRegs(ModbusTable table, int address, int count, std::vector<uint16_t>* regs, std::string* err)
{
    if (!regs || (table != ModbusHolding && table != ModbusInput) || !rangeOk(address, count, 125, err)) {
        setCommErr(err, "参数无效");
        return false;
    }
    if (address + count > kMapSize) {
        setCommErr(err, "地址超出从站映像");
        return false;
    }
    const std::vector<uint16_t>& src = (table == ModbusHolding) ? m_holding : m_input;
    std::lock_guard<std::mutex> lock(m_mu);
    regs->assign(src.begin() + address, src.begin() + address + count);
    return true;
}

// 写本机线圈或离散输入映像
bool ModbusSlave::writeBits(ModbusTable table, int address, const std::vector<unsigned char>& bits, std::string* err)
{
    const int count = static_cast<int>(bits.size());
    if ((table != ModbusCoil && table != ModbusDiscrete) || !rangeOk(address, count, 2000, err)) {
        setCommErr(err, "参数无效");
        return false;
    }
    if (address + count > kMapSize) {
        setCommErr(err, "地址超出从站映像");
        return false;
    }
    std::vector<unsigned char>& dst = (table == ModbusCoil) ? m_coils : m_discrete;
    std::lock_guard<std::mutex> lock(m_mu);
    for (int i = 0; i < count; ++i) {
        dst[static_cast<size_t>(address + i)] = bits[static_cast<size_t>(i)] ? 1 : 0;
    }
    return true;
}

// 写本机保持或输入寄存器映像
bool ModbusSlave::writeRegs(ModbusTable table, int address, const std::vector<uint16_t>& regs, std::string* err)
{
    const int count = static_cast<int>(regs.size());
    if ((table != ModbusHolding && table != ModbusInput) || !rangeOk(address, count, 123, err)) {
        setCommErr(err, "参数无效");
        return false;
    }
    if (address + count > kMapSize) {
        setCommErr(err, "地址超出从站映像");
        return false;
    }
    std::vector<uint16_t>& dst = (table == ModbusHolding) ? m_holding : m_input;
    std::lock_guard<std::mutex> lock(m_mu);
    for (int i = 0; i < count; ++i) {
        dst[static_cast<size_t>(address + i)] = regs[static_cast<size_t>(i)];
    }
    return true;
}

// 按功能码改映像并组应答；广播不回包
CommBytes ModbusSlave::reply(const CommBytes& unitAndPdu)
{
    if (unitAndPdu.size() < 2) {
        return CommBytes();
    }
    const int unit = unitAndPdu[0];
    const int fc = unitAndPdu[1];
    const bool broadcast = (unit == 0);
    if (!broadcast && unit != m_slaveId) {
        return CommBytes();
    }
    const unsigned char* p = &unitAndPdu[0];
    const int n = static_cast<int>(unitAndPdu.size());
    CommBytes resp;
    resp.push_back(static_cast<unsigned char>(m_slaveId & 0xFF));

    std::lock_guard<std::mutex> lock(m_mu);
    const bool isWrite = (fc == 0x05 || fc == 0x06 || fc == 0x0F || fc == 0x10);
    if (n < 6 && fc != 0x05 && fc != 0x06) {
        resp.push_back(static_cast<unsigned char>(fc | 0x80));
        resp.push_back(0x03);
        return broadcast ? CommBytes() : resp;
    }

    if (fc == 0x01 || fc == 0x02) {
        if (n < 6) {
            resp.push_back(static_cast<unsigned char>(fc | 0x80));
            resp.push_back(0x03);
            return broadcast ? CommBytes() : resp;
        }
        const int addr = read16(p + 2);
        const int count = read16(p + 4);
        const std::vector<unsigned char>& src = (fc == 0x01) ? m_coils : m_discrete;
        if (count <= 0 || count > 2000 || addr < 0 || addr + count > kMapSize) {
            resp.push_back(static_cast<unsigned char>(fc | 0x80));
            resp.push_back(0x02);
            return broadcast ? CommBytes() : resp;
        }
        resp.push_back(static_cast<unsigned char>(fc));
        const int byteCount = (count + 7) / 8;
        resp.push_back(static_cast<unsigned char>(byteCount));
        for (int i = 0; i < byteCount; ++i) {
            unsigned char byte = 0;
            for (int b = 0; b < 8; ++b) {
                const int idx = addr + i * 8 + b;
                if (i * 8 + b < count && src[static_cast<size_t>(idx)]) {
                    byte = static_cast<unsigned char>(byte | (1 << b));
                }
            }
            resp.push_back(byte);
        }
        return broadcast ? CommBytes() : resp;
    }

    if (fc == 0x03 || fc == 0x04) {
        if (n < 6) {
            resp.push_back(static_cast<unsigned char>(fc | 0x80));
            resp.push_back(0x03);
            return broadcast ? CommBytes() : resp;
        }
        const int addr = read16(p + 2);
        const int count = read16(p + 4);
        const std::vector<uint16_t>& src = (fc == 0x03) ? m_holding : m_input;
        if (count <= 0 || count > 125 || addr < 0 || addr + count > kMapSize) {
            resp.push_back(static_cast<unsigned char>(fc | 0x80));
            resp.push_back(0x02);
            return broadcast ? CommBytes() : resp;
        }
        resp.push_back(static_cast<unsigned char>(fc));
        resp.push_back(static_cast<unsigned char>(count * 2));
        for (int i = 0; i < count; ++i) {
            append16(&resp, src[static_cast<size_t>(addr + i)]);
        }
        return broadcast ? CommBytes() : resp;
    }

    if (fc == 0x05) {
        if (n < 6) {
            resp.push_back(static_cast<unsigned char>(fc | 0x80));
            resp.push_back(0x03);
            return broadcast ? CommBytes() : resp;
        }
        const int addr = read16(p + 2);
        const int val = read16(p + 4);
        if (addr < 0 || addr >= kMapSize || (val != 0 && val != 0xFF00)) {
            resp.push_back(static_cast<unsigned char>(fc | 0x80));
            resp.push_back(0x03);
            return broadcast ? CommBytes() : resp;
        }
        m_coils[static_cast<size_t>(addr)] = val ? 1 : 0;
        if (broadcast) {
            return CommBytes();
        }
        resp.push_back(0x05);
        append16(&resp, addr);
        append16(&resp, val);
        return resp;
    }

    if (fc == 0x06) {
        if (n < 6) {
            resp.push_back(static_cast<unsigned char>(fc | 0x80));
            resp.push_back(0x03);
            return broadcast ? CommBytes() : resp;
        }
        const int addr = read16(p + 2);
        const int val = read16(p + 4);
        if (addr < 0 || addr >= kMapSize) {
            resp.push_back(static_cast<unsigned char>(fc | 0x80));
            resp.push_back(0x02);
            return broadcast ? CommBytes() : resp;
        }
        m_holding[static_cast<size_t>(addr)] = static_cast<uint16_t>(val);
        if (broadcast) {
            return CommBytes();
        }
        resp.push_back(0x06);
        append16(&resp, addr);
        append16(&resp, val);
        return resp;
    }

    if (fc == 0x0F) {
        if (n < 7) {
            resp.push_back(static_cast<unsigned char>(fc | 0x80));
            resp.push_back(0x03);
            return broadcast ? CommBytes() : resp;
        }
        const int addr = read16(p + 2);
        const int count = read16(p + 4);
        const int byteCount = p[6];
        if (count <= 0 || count > 1968 || addr < 0 || addr + count > kMapSize || n < 7 + byteCount) {
            resp.push_back(static_cast<unsigned char>(fc | 0x80));
            resp.push_back(0x02);
            return broadcast ? CommBytes() : resp;
        }
        for (int i = 0; i < count; ++i) {
            const int byte = p[7 + (i / 8)];
            m_coils[static_cast<size_t>(addr + i)] = (byte & (1 << (i % 8))) ? 1 : 0;
        }
        if (broadcast) {
            return CommBytes();
        }
        resp.push_back(0x0F);
        append16(&resp, addr);
        append16(&resp, count);
        return resp;
    }

    if (fc == 0x10) {
        if (n < 7) {
            resp.push_back(static_cast<unsigned char>(fc | 0x80));
            resp.push_back(0x03);
            return broadcast ? CommBytes() : resp;
        }
        const int addr = read16(p + 2);
        const int count = read16(p + 4);
        const int byteCount = p[6];
        if (count <= 0 || count > 123 || addr < 0 || addr + count > kMapSize || byteCount < count * 2 || n < 7 + byteCount) {
            resp.push_back(static_cast<unsigned char>(fc | 0x80));
            resp.push_back(0x02);
            return broadcast ? CommBytes() : resp;
        }
        for (int i = 0; i < count; ++i) {
            m_holding[static_cast<size_t>(addr + i)] = static_cast<uint16_t>(read16(p + 7 + i * 2));
        }
        if (broadcast) {
            return CommBytes();
        }
        resp.push_back(0x10);
        append16(&resp, addr);
        append16(&resp, count);
        return resp;
    }

    if (!isWrite && broadcast) {
        return CommBytes();
    }
    resp.push_back(static_cast<unsigned char>(fc | 0x80));
    resp.push_back(0x01);
    return broadcast ? CommBytes() : resp;
}

// 拆 MBAP 后交给应答，再套回事务号
CommBytes ModbusSlave::onTcp(const CommBytes& frame)
{
    if (frame.size() < 8) {
        return CommBytes();
    }
    const int tid = read16(&frame[0]);
    CommBytes req(frame.begin() + 6, frame.end());
    const CommBytes body = reply(req);
    if (body.empty()) {
        return CommBytes();
    }
    CommBytes out;
    append16(&out, tid);
    append16(&out, 0);
    append16(&out, static_cast<int>(body.size()));
    out.insert(out.end(), body.begin(), body.end());
    return out;
}

// 校验 CRC 后交给应答，再补 CRC
CommBytes ModbusSlave::onRtu(const CommBytes& frame)
{
    if (frame.size() < 4) {
        return CommBytes();
    }
    const uint16_t c = crc16(&frame[0], static_cast<int>(frame.size()) - 2);
    const uint16_t got = static_cast<uint16_t>(frame[frame.size() - 2]
                                                | (static_cast<uint16_t>(frame[frame.size() - 1]) << 8));
    if (c != got) {
        return CommBytes();
    }
    CommBytes req(frame.begin(), frame.end() - 2);
    CommBytes body = reply(req);
    if (body.empty()) {
        return CommBytes();
    }
    const uint16_t rc = crc16(&body[0], static_cast<int>(body.size()));
    body.push_back(static_cast<unsigned char>(rc & 0xFF));
    body.push_back(static_cast<unsigned char>((rc >> 8) & 0xFF));
    return body;
}

// 循环收请求并回包，直到停止
void ModbusSlave::loop()
{
    while (m_run) {
        if (!m_ch || !m_ch->isOpen()) {
            Sleep(50);
            continue;
        }
        if (m_ch->transport() == TransportTcpServer || m_ch->transport() == TransportTcpClient) {
            std::string peer;
            if (m_ch->transport() == TransportTcpServer) {
                peer = m_ch->waitIncoming(100);
                if (peer.empty()) {
                    continue;
                }
            }
            std::string err;
            const CommBytes chunk = m_ch->read(peer, 512, m_ch->transport() == TransportTcpServer ? 0 : 100, &err);
            if (chunk.empty()) {
                continue;
            }
            CommBytes& buf = m_rx[peer];
            buf.insert(buf.end(), chunk.begin(), chunk.end());
            while (buf.size() >= 6) {
                const int len = read16(&buf[4]);
                if (len < 2 || len > 260) {
                    buf.clear();
                    break;
                }
                if (static_cast<int>(buf.size()) < 6 + len) {
                    break;
                }
                CommBytes frame(buf.begin(), buf.begin() + 6 + len);
                buf.erase(buf.begin(), buf.begin() + 6 + len);
                const CommBytes resp = onTcp(frame);
                if (!resp.empty()) {
                    m_ch->write(peer, resp, 500, &err);
                }
            }
            continue;
        }
        std::string err;
        const CommBytes frame = m_ch->read(std::string(), 260, 100, &err);
        if (frame.empty()) {
            continue;
        }
        const CommBytes resp = onRtu(frame);
        if (!resp.empty()) {
            m_ch->write(std::string(), resp, 500, &err);
        }
    }
}

}
