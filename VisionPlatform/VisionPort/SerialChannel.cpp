/**
 * @file SerialChannel.cpp
 * @brief SerialChannel实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "SerialChannel.h"
#include "TextEncoding.h"
#include <cstdio>
#include <cstring>

namespace Vision {

namespace {

// COM10 及以上补设备路径前缀，否则打不开
std::string portPath(const std::string& name)
{
    if (name.size() >= 4 && name[0] == '\\') {
        return name;
    }
    return std::string("\\\\.\\") + name;
}

// 校验位文本转系统常量；无法识别则无校验
int parityFrom(const std::string& s)
{
    if (s == "even") {
        return EVENPARITY;
    }
    if (s == "odd") {
        return ODDPARITY;
    }
    return NOPARITY;
}

} // namespace

// 句柄置为未打开
SerialChannel::SerialChannel()
    : m_handle(INVALID_HANDLE_VALUE)
{
}

// 析构时关闭串口
SerialChannel::~SerialChannel()
{
    close();
}

// 从注册表枚举本机串口名
std::vector<std::string> SerialChannel::listPorts()
{
    std::vector<std::string> names;
    HKEY key = 0;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "HARDWARE\\DEVICEMAP\\SERIALCOMM", 0, KEY_READ, &key) != ERROR_SUCCESS) {
        return names;
    }
    for (DWORD i = 0;; ++i) {
        char valueName[256];
        BYTE data[256];
        DWORD nameLen = sizeof(valueName);
        DWORD dataLen = sizeof(data);
        DWORD type = 0;
        const LONG rc = RegEnumValueA(key, i, valueName, &nameLen, 0, &type, data, &dataLen);
        if (rc != ERROR_SUCCESS) {
            break;
        }
        if (type == REG_SZ && dataLen > 1) {
            names.push_back(reinterpret_cast<const char*>(data));
        }
    }
    RegCloseKey(key);
    return names;
}

// 按端口、波特率、数据位、校验和停止位打开
bool SerialChannel::open(const ParamMap& params, std::string* err)
{
    close();
    const std::string port = params.value("port").toString();
    if (port.empty()) {
        setCommErr(err, "未填写串口");
        return false;
    }
    // 数据位夹到 5~8，停止位只区分 1 和 2
    const int baud = params.value("baud").toInt(9600);
    int dataBits = params.value("dataBits").toInt(8);
    if (dataBits < 5) {
        dataBits = 5;
    }
    if (dataBits > 8) {
        dataBits = 8;
    }
    const std::string parity = params.value("parity").toString();
    int stopBits = params.value("stopBits").toInt(1);

    const std::string path = portPath(port);
    m_handle = CreateFileA(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, 0, 0);
    if (m_handle == INVALID_HANDLE_VALUE) {
        char buf[64];
        sprintf_s(buf, "串口打开失败（%lu）", GetLastError());
        setCommErr(err, buf);
        return false;
    }

    // 二进制模式，关闭硬件/软件流控，DTR/RTS 常开
    DCB dcb;
    std::memset(&dcb, 0, sizeof(dcb));
    dcb.DCBlength = sizeof(dcb);
    if (!GetCommState(m_handle, &dcb)) {
        setCommErr(err, "读取串口状态失败");
        close();
        return false;
    }
    dcb.BaudRate = static_cast<DWORD>(baud > 0 ? baud : 9600);
    dcb.ByteSize = static_cast<BYTE>(dataBits);
    dcb.Parity = static_cast<BYTE>(parityFrom(parity));
    dcb.StopBits = (stopBits >= 2) ? TWOSTOPBITS : ONESTOPBIT;
    dcb.fBinary = TRUE;
    dcb.fParity = (dcb.Parity != NOPARITY) ? TRUE : FALSE;
    dcb.fOutxCtsFlow = FALSE;
    dcb.fOutxDsrFlow = FALSE;
    dcb.fDtrControl = DTR_CONTROL_ENABLE;
    dcb.fDsrSensitivity = FALSE;
    dcb.fTXContinueOnXoff = FALSE;
    dcb.fOutX = FALSE;
    dcb.fInX = FALSE;
    dcb.fErrorChar = FALSE;
    dcb.fNull = FALSE;
    dcb.fRtsControl = RTS_CONTROL_ENABLE;
    dcb.fAbortOnError = FALSE;
    if (!SetCommState(m_handle, &dcb)) {
        setCommErr(err, "串口参数设置失败");
        close();
        return false;
    }
    SetupComm(m_handle, 4096, 4096);
    PurgeComm(m_handle, PURGE_RXCLEAR | PURGE_TXCLEAR);
    applyTimeouts(1000);
    return true;
}

// 关闭句柄
void SerialChannel::close()
{
    if (m_handle != INVALID_HANDLE_VALUE) {
        CloseHandle(m_handle);
        m_handle = INVALID_HANDLE_VALUE;
    }
}

// 句柄是否有效
bool SerialChannel::isOpen() const
{
    return m_handle != INVALID_HANDLE_VALUE;
}

// 已打开时返回一个空 id 的对端
std::vector<CommPeer> SerialChannel::peers() const
{
    std::vector<CommPeer> list;
    if (isOpen()) {
        CommPeer p;
        list.push_back(p);
    }
    return list;
}

// 按毫秒设置读写超时
void SerialChannel::applyTimeouts(int timeoutMs)
{
    if (m_handle == INVALID_HANDLE_VALUE) {
        return;
    }
    if (timeoutMs < 0) {
        timeoutMs = 0;
    }
    COMMTIMEOUTS to;
    if (timeoutMs == 0) {
        to.ReadIntervalTimeout = MAXDWORD;
        to.ReadTotalTimeoutMultiplier = 0;
        to.ReadTotalTimeoutConstant = 0;
    } else {
        to.ReadIntervalTimeout = 20;
        to.ReadTotalTimeoutMultiplier = 0;
        to.ReadTotalTimeoutConstant = static_cast<DWORD>(timeoutMs);
    }
    to.WriteTotalTimeoutMultiplier = 0;
    to.WriteTotalTimeoutConstant = static_cast<DWORD>(timeoutMs == 0 ? 1000 : timeoutMs);
    SetCommTimeouts(m_handle, &to);
}

// 写出整段字节
bool SerialChannel::write(const std::string&, const CommBytes& data, int timeoutMs, std::string* err)
{
    if (!isOpen()) {
        setCommErr(err, "通道未打开");
        return false;
    }
    if (data.empty()) {
        return true;
    }
    applyTimeouts(timeoutMs <= 0 ? 1000 : timeoutMs);
    DWORD wrote = 0;
    const BOOL ok = WriteFile(m_handle, &data[0], static_cast<DWORD>(data.size()), &wrote, 0);
    if (!ok || wrote != data.size()) {
        setCommErr(err, "写入不完整");
        return false;
    }
    return true;
}

// 按超时读一批
CommBytes SerialChannel::read(const std::string&, int maxBytes, int timeoutMs, std::string* err)
{
    CommBytes out;
    if (!isOpen()) {
        setCommErr(err, "通道未打开");
        return out;
    }
    if (maxBytes <= 0) {
        maxBytes = 4096;
    }
    applyTimeouts(timeoutMs);
    out.resize(static_cast<size_t>(maxBytes));
    DWORD got = 0;
    const BOOL ok = ReadFile(m_handle, &out[0], static_cast<DWORD>(maxBytes), &got, 0);
    if (!ok) {
        out.clear();
        setCommErr(err, "读失败");
        return out;
    }
    out.resize(got);
    if (got == 0 && timeoutMs > 0) {
        setCommErr(err, "读超时");
    }
    return out;
}

// 清空接收缓冲
void SerialChannel::drain(const std::string&)
{
    if (isOpen()) {
        PurgeComm(m_handle, PURGE_RXCLEAR);
    }
}

// 串口没有多客户端，不等待接入
std::string SerialChannel::waitIncoming(int)
{
    return std::string();
}

// 传输类型
CommTransport SerialChannel::transport() const
{
    return TransportSerial;
}



}
