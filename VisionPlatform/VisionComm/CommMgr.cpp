/**
 * @file CommMgr.cpp
 * @brief CommMgr实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "CommMgr.h"
#include "TextEncoding.h"
#include "HttpClient.h"
#include "SerialChannel.h"
#include "TcpClientChannel.h"
#include "TcpServerChannel.h"
#include "UdpChannel.h"
#include "WebSocketClient.h"
#include <map>
#include <memory>
#include <mutex>

namespace Vision {

namespace {

struct Session {
    ICommChannel* channel; ///< 传输通道，拥有
    ModbusSlave* slave; ///< 从站；主站为空
    bool modbus; ///< 是否走 MODBUS
    bool master; ///< 主站为真，从站为假
    uint16_t tid; ///< 下一事务号
    std::mutex mu; ///< 同一通道串行收发

    Session()
        : channel(nullptr)
        , slave(nullptr)
        , modbus(false)
        , master(true)
        , tid(0)
    {}
};

// 按通道类型创建传输对象
ICommChannel* createChannel(CommChannelType type)
{
    if (type == CommTcpClient) {
        return new TcpClientChannel();
    }
    if (type == CommTcpServer) {
        return new TcpServerChannel();
    }
    if (type == CommUdp) {
        return new UdpChannel();
    }
    if (type == CommHttp) {
        return new HttpClient();
    }
    if (type == CommWebSocket) {
        return new WebSocketClient();
    }
    return new SerialChannel();
}

// 服务端必须指定客户端，其它通道忽略对端 id
bool usePeer(ICommChannel* ch, const std::string& peerId, std::string* out, std::string* err)
{
    if (ch->transport() == TransportTcpServer) {
        if (peerId.empty()) {
            setCommErr(err, "未选择客户端");
            return false;
        }
        *out = peerId;
        return true;
    }
    *out = std::string();
    return true;
}

} // namespace

struct CommMgr::Impl {
    std::mutex mu; ///< 保护会话表
    std::map<std::string, std::shared_ptr<Session> > sessions; ///< 通道 id 到会话
};

// 返回进程内唯一实例
CommMgr& CommMgr::instance()
{
    static CommMgr hub;
    return hub;
}

// 创建空的会话表
CommMgr::CommMgr()
    : m_impl(new Impl)
{
}

// 关闭全部通道并释放会话表
CommMgr::~CommMgr()
{
    std::vector<std::string> ids;
    {
        std::lock_guard<std::mutex> lock(m_impl->mu);
        for (std::map<std::string, std::shared_ptr<Session> >::iterator it = m_impl->sessions.begin();
             it != m_impl->sessions.end(); ++it) {
            ids.push_back(it->first);
        }
    }
    for (size_t i = 0; i < ids.size(); ++i) {
        closeChannel(ObjectId(ids[i]));
    }
    if (m_impl) {
        delete m_impl;
        m_impl = nullptr;
    }
}

// 枚举本机串口名
std::vector<std::string> CommMgr::listSerialPorts() const
{
    return SerialChannel::listPorts();
}

// 按台账打开通道；已开则先关再开
bool CommMgr::openChannel(const CommChannel& ch, std::string* err)
{
    closeChannel(ch.id);
    const std::string protocol = ch.params.value("protocol").toString();
    const bool modbus = (protocol == "modbus");
    const bool master = ch.params.value("role").toString() != "slave";
    if (modbus && ch.type == CommUdp) {
        setCommErr(err, "UDP 不支持 MODBUS");
        return false;
    }
    if (modbus && ch.type == CommHttp) {
        setCommErr(err, "HTTP 不支持 MODBUS");
        return false;
    }
    if (modbus && ch.type == CommWebSocket) {
        setCommErr(err, "WebSocket 不支持 MODBUS");
        return false;
    }
    ICommChannel* channel = createChannel(ch.type);
    if (!channel->open(ch.params, err)) {
        if (channel) {
            delete channel;
            channel = nullptr;
        }
        return false;
    }
    std::shared_ptr<Session> session(new Session());
    session->channel = channel;
    session->modbus = modbus;
    session->master = master;
    if (modbus && !master) {
        session->slave = new ModbusSlave();
        session->slave->start(channel, ch.params.value("slaveId").toInt(1));
    }
    std::lock_guard<std::mutex> lock(m_impl->mu);
    m_impl->sessions[ch.id.toString()] = session;
    return true;
}

// 停掉从站并关闭传输
void CommMgr::closeChannel(const ObjectId& id)
{
    std::shared_ptr<Session> session;
    {
        std::lock_guard<std::mutex> lock(m_impl->mu);
        std::map<std::string, std::shared_ptr<Session> >::iterator it = m_impl->sessions.find(id.toString());
        if (it == m_impl->sessions.end()) {
            return;
        }
        session = it->second;
        m_impl->sessions.erase(it);
    }
    {
        std::lock_guard<std::mutex> lock(session->mu);
    }
    if (session->slave) {
        session->slave->stop();
        delete session->slave;
        session->slave = nullptr;
    }
    if (session->channel) {
        session->channel->close();
        delete session->channel;
        session->channel = nullptr;
    }
}

// 通道是否在会话表里且传输已打开
bool CommMgr::isOpen(const ObjectId& id) const
{
    std::lock_guard<std::mutex> lock(m_impl->mu);
    std::map<std::string, std::shared_ptr<Session> >::const_iterator it = m_impl->sessions.find(id.toString());
    return it != m_impl->sessions.end() && it->second->channel && it->second->channel->isOpen();
}

// 取当前对端；未打开则空
std::vector<CommPeer> CommMgr::peers(const ObjectId& id) const
{
    std::lock_guard<std::mutex> lock(m_impl->mu);
    std::map<std::string, std::shared_ptr<Session> >::const_iterator it = m_impl->sessions.find(id.toString());
    if (it == m_impl->sessions.end() || !it->second->channel) {
        return std::vector<CommPeer>();
    }
    return it->second->channel->peers();
}

// 向已打开通道写原始字节
bool CommMgr::writeBytes(const ObjectId& id, const std::string& peerId, const CommBytes& data, std::string* err)
{
    std::shared_ptr<Session> session;
    {
        std::lock_guard<std::mutex> lock(m_impl->mu);
        std::map<std::string, std::shared_ptr<Session> >::iterator it = m_impl->sessions.find(id.toString());
        if (it == m_impl->sessions.end()) {
            setCommErr(err, "通道未打开");
            return false;
        }
        session = it->second;
    }
    std::lock_guard<std::mutex> lock(session->mu);
    std::string peer;
    if (!usePeer(session->channel, peerId, &peer, err)) {
        return false;
    }
    return session->channel->write(peer, data, 1000, err);
}

// 从已打开通道读原始字节
CommBytes CommMgr::readBytes(const ObjectId& id, const std::string& peerId, int maxBytes, int timeoutMs, std::string* err)
{
    std::shared_ptr<Session> session;
    {
        std::lock_guard<std::mutex> lock(m_impl->mu);
        std::map<std::string, std::shared_ptr<Session> >::iterator it = m_impl->sessions.find(id.toString());
        if (it == m_impl->sessions.end()) {
            setCommErr(err, "通道未打开");
            return CommBytes();
        }
        session = it->second;
    }
    std::lock_guard<std::mutex> lock(session->mu);
    std::string peer;
    if (!usePeer(session->channel, peerId, &peer, err)) {
        return CommBytes();
    }
    return session->channel->read(peer, maxBytes, timeoutMs, err);
}

namespace {

// 按通道 id 取出会话；没有则报未打开
std::shared_ptr<Session> takeSession(std::map<std::string, std::shared_ptr<Session> >& sessions, std::mutex& mu,
                                         const std::string& key, std::string* err)
{
    std::lock_guard<std::mutex> lock(mu);
    std::map<std::string, std::shared_ptr<Session> >::iterator it = sessions.find(key);
    if (it == sessions.end() || !it->second->channel) {
        setCommErr(err, "通道未打开");
        return std::shared_ptr<Session>();
    }
    return it->second;
}

// 递增事务号，跳过 0
uint16_t nextTid(Session* session)
{
    session->tid = static_cast<uint16_t>(session->tid + 1);
    if (session->tid == 0) {
        session->tid = 1;
    }
    return session->tid;
}

} // namespace

// 读位：从站读映像，主站发总线请求
bool CommMgr::modbusReadBits(const ObjectId& id, const std::string& peerId, int slave, ModbusTable table,
                             int address, int count, std::vector<unsigned char>* bits, std::string* err)
{
    if (table != ModbusCoil && table != ModbusDiscrete) {
        setCommErr(err, "该区不是位");
        return false;
    }
    std::shared_ptr<Session> session = takeSession(m_impl->sessions, m_impl->mu, id.toString(), err);
    if (!session) {
        return false;
    }
    std::lock_guard<std::mutex> lock(session->mu);
    if (!session->modbus) {
        setCommErr(err, "当前不是 MODBUS 通道");
        return false;
    }
    if (!session->master) {
        return session->slave->readBits(table, address, count, bits, err);
    }
    std::string peer;
    if (!usePeer(session->channel, peerId, &peer, err)) {
        return false;
    }
    const bool tcp = session->channel->transport() != TransportSerial;
    const int fc = (table == ModbusCoil) ? 0x01 : 0x02;
    return modbusMasterReadBits(session->channel, peer, tcp, nextTid(session.get()), slave, fc, address, count, bits, err);
}

// 读寄存器：从站读映像，主站发总线请求
bool CommMgr::modbusReadRegs(const ObjectId& id, const std::string& peerId, int slave, ModbusTable table,
                             int address, int count, std::vector<uint16_t>* regs, std::string* err)
{
    if (table != ModbusHolding && table != ModbusInput) {
        setCommErr(err, "该区不是寄存器");
        return false;
    }
    std::shared_ptr<Session> session = takeSession(m_impl->sessions, m_impl->mu, id.toString(), err);
    if (!session) {
        return false;
    }
    std::lock_guard<std::mutex> lock(session->mu);
    if (!session->modbus) {
        setCommErr(err, "当前不是 MODBUS 通道");
        return false;
    }
    if (!session->master) {
        return session->slave->readRegs(table, address, count, regs, err);
    }
    std::string peer;
    if (!usePeer(session->channel, peerId, &peer, err)) {
        return false;
    }
    const bool tcp = session->channel->transport() != TransportSerial;
    const int fc = (table == ModbusHolding) ? 0x03 : 0x04;
    return modbusMasterReadRegs(session->channel, peer, tcp, nextTid(session.get()), slave, fc, address, count, regs, err);
}

// 写位：从站写映像，主站只允许线圈
bool CommMgr::modbusWriteBits(const ObjectId& id, const std::string& peerId, int slave, ModbusTable table,
                              int address, const std::vector<unsigned char>& bits, std::string* err)
{
    if (table != ModbusCoil && table != ModbusDiscrete) {
        setCommErr(err, "该区不是位");
        return false;
    }
    std::shared_ptr<Session> session = takeSession(m_impl->sessions, m_impl->mu, id.toString(), err);
    if (!session) {
        return false;
    }
    std::lock_guard<std::mutex> lock(session->mu);
    if (!session->modbus) {
        setCommErr(err, "当前不是 MODBUS 通道");
        return false;
    }
    if (!session->master) {
        return session->slave->writeBits(table, address, bits, err);
    }
    if (table != ModbusCoil) {
        setCommErr(err, "主站不能写该区");
        return false;
    }
    std::string peer;
    if (!usePeer(session->channel, peerId, &peer, err)) {
        return false;
    }
    const bool tcp = session->channel->transport() != TransportSerial;
    return modbusMasterWriteBits(session->channel, peer, tcp, nextTid(session.get()), slave, address, bits, err);
}

// 写寄存器：从站写映像，主站只允许保持寄存器
bool CommMgr::modbusWriteRegs(const ObjectId& id, const std::string& peerId, int slave, ModbusTable table,
                              int address, const std::vector<uint16_t>& regs, std::string* err)
{
    if (table != ModbusHolding && table != ModbusInput) {
        setCommErr(err, "该区不是寄存器");
        return false;
    }
    std::shared_ptr<Session> session = takeSession(m_impl->sessions, m_impl->mu, id.toString(), err);
    if (!session) {
        return false;
    }
    std::lock_guard<std::mutex> lock(session->mu);
    if (!session->modbus) {
        setCommErr(err, "当前不是 MODBUS 通道");
        return false;
    }
    if (!session->master) {
        return session->slave->writeRegs(table, address, regs, err);
    }
    if (table != ModbusHolding) {
        setCommErr(err, "主站不能写该区");
        return false;
    }
    std::string peer;
    if (!usePeer(session->channel, peerId, &peer, err)) {
        return false;
    }
    const bool tcp = session->channel->transport() != TransportSerial;
    return modbusMasterWriteRegs(session->channel, peer, tcp, nextTid(session.get()), slave, address, regs, err);
}

void CommMgr::closePeer(const ObjectId& id, const std::string& peerId)
{
    std::string err;
    std::shared_ptr<Session> session = takeSession(m_impl->sessions, m_impl->mu, id.toString(), &err);
    if (!session || !session->channel) {
        return;
    }
    std::lock_guard<std::mutex> lock(session->mu);
    session->channel->closePeer(peerId);
}

bool CommMgr::httpExchange(const ObjectId& id, const std::string& method, const std::string& path,
                           const std::string& contentType, const std::string& body,
                           int* status, std::string* response, std::string* err)
{
    std::shared_ptr<Session> session = takeSession(m_impl->sessions, m_impl->mu, id.toString(), err);
    if (!session || !session->channel) {
        return false;
    }
    HttpClient* http = dynamic_cast<HttpClient*>(session->channel);
    if (!http) {
        setCommErr(err, "当前不是 HTTP 通道");
        return false;
    }
    std::lock_guard<std::mutex> lock(session->mu);
    return http->exchange(method, path, contentType, body, status, response, err);
}

bool CommMgr::wsSend(const ObjectId& id, const CommBytes& data, bool text, std::string* err)
{
    std::shared_ptr<Session> session = takeSession(m_impl->sessions, m_impl->mu, id.toString(), err);
    if (!session || !session->channel) {
        return false;
    }
    WebSocketClient* ws = dynamic_cast<WebSocketClient*>(session->channel);
    if (!ws) {
        setCommErr(err, "当前不是 WebSocket 通道");
        return false;
    }
    std::lock_guard<std::mutex> lock(session->mu);
    return ws->send(data, text, err);
}

CommBytes CommMgr::wsRecv(const ObjectId& id, int timeoutMs, bool* text, std::string* err)
{
    std::shared_ptr<Session> session = takeSession(m_impl->sessions, m_impl->mu, id.toString(), err);
    if (!session || !session->channel) {
        return CommBytes();
    }
    WebSocketClient* ws = dynamic_cast<WebSocketClient*>(session->channel);
    if (!ws) {
        setCommErr(err, "当前不是 WebSocket 通道");
        return CommBytes();
    }
    std::lock_guard<std::mutex> lock(session->mu);
    return ws->recv(timeoutMs, text, err);
}

}
