/**
 * @file hs_ipc.h
 * @brief 本机进程通讯。视觉平台和运控软件链接同一份 DLL
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 *
 * 视觉平台和运控软件链接同一份 DLL，互不包含对方头文件。
 * 一条通道打开时选定一种传输，中途不换。四种传输收发同一种报文。
 *
 * 通道短名只允许字母、数字、点、下划线和减号。谁先 hs_ipc_open 谁创建对象。
 * 共享内存先放 Global\，当前用户没有创建全局段的权限时退回 Local\。
 * 要跨会话共用，由有权限的一方先打开。
 *
 * poll 填出的 name、text、path、data、stats 指向库内缓冲，
 * 保持到下一次 poll 或 hs_ipc_close，调用方不要 free。
 */

#ifndef HS_IPC_H
#define HS_IPC_H

#include <stdint.h>

#ifdef HS_IPC_EXPORTS
#define HS_IPC_API __declspec(dllexport)
#else
#define HS_IPC_API __declspec(dllimport)
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define HS_IPC_VERSION 1
/** 管道、消息队列、套接字上，图像和文件超过这个字节数就拆成多帧。 */
#define HS_IPC_CHUNK_BYTES (1024u * 1024u)

/** 打开通道时选定。四种都能传 Signal、Stats、Image、File。 */
enum HsIpcTransport {
    HS_IPC_SHARED_MEMORY = 1, /**< 信号进环，图像和文件进槽，大块不进内核 */
    HS_IPC_NAMED_PIPE = 2,    /**< 帧按顺序写进命名管道 */
    HS_IPC_MESSAGE_QUEUE = 3, /**< 库内命名队列，每条留到被取走，不安装 MSMQ */
    HS_IPC_SOCKET = 4         /**< TCP。本机 127.0.0.1，跨机器填 host */
};

enum HsIpcType {
    HS_IPC_HELLO = 1,     /**< 带库版本和传输种类，对端版本不同则 poll 返回协议错误 */
    HS_IPC_HEARTBEAT = 2, /**< 空体。超时由调用方用 hs_ipc_last_recv_ms 自己判断 */
    HS_IPC_SIGNAL = 3,    /**< OK/NG。用 name、result、code、text */
    HS_IPC_STATS = 4,     /**< 一组键值。共享内存上只保留最新一包 */
    HS_IPC_IMAGE = 5,     /**< 用 name、format、宽高、frameSeq、data */
    HS_IPC_FILE = 6,      /**< 有 path 时只传路径；path 为空时传内容 */
    HS_IPC_ACK = 7,       /**< 确认 ackSeq。图像默认不发确认 */
    HS_IPC_BYE = 8        /**< 主动关闭，空体 */
};

enum HsIpcFlag {
    HS_IPC_FLAG_CONT = 0x0001, /**< 后面还有分片 */
    HS_IPC_FLAG_SLOT = 0x0002  /**< 载荷在共享内存槽里，体里是槽号 */
};

enum HsIpcResult {
    HS_IPC_RESULT_UNKNOWN = 0,
    HS_IPC_RESULT_OK = 1,
    HS_IPC_RESULT_NG = 2
};

enum HsIpcValueType {
    HS_IPC_VALUE_INT = 1,
    HS_IPC_VALUE_FLOAT = 2
};

enum HsIpcImageFormat {
    HS_IPC_IMAGE_JPEG = 1,
    HS_IPC_IMAGE_RAW = 2
};

/** 0 成功。1 超时。负数为失败。 */
enum HsIpcCode {
    HS_IPC_OK = 0,
    HS_IPC_TIMEOUT = 1,
    HS_IPC_ERR_ARG = -1,
    HS_IPC_ERR_FULL = -2,
    HS_IPC_ERR_IO = -3,
    HS_IPC_ERR_PROTOCOL = -4,
    HS_IPC_ERR_SIZE = -5
};

typedef struct HsIpcStat {
    const char* key;
    int type;       /**< HS_IPC_VALUE_INT 或 HS_IPC_VALUE_FLOAT */
    int64_t i64;
    double f64;
} HsIpcStat;

typedef struct HsIpcConfig {
    int transport;             /**< HsIpcTransport */
    const char* name;          /**< 通道短名，例如 Hs.VisionMotion */
    const char* host;          /**< 套接字地址。空则 127.0.0.1 */
    int port;                  /**< 套接字端口。只有套接字使用 */
    int signalCapacity;        /**< 信号环或消息队列条数。0 则用 32 */
    uint32_t imageSlotBytes;   /**< 共享内存图像槽能放的像素字节。0 则 1MB */
    uint32_t fileSlotBytes;    /**< 共享内存文件槽字节。0 则 1MB */
} HsIpcConfig;

/**
 * @brief 一条报文。发送时只填本类型用到的字段，其余置 0
 *
 * seq、timeMs、flags 由库在发送时写入，poll 时带出来。
 * 共享内存的图像和文件带 HS_IPC_FLAG_SLOT，像素仍拷进 data，调用方不用自己映射。
 * 分片时除最后一帧外带 HS_IPC_FLAG_CONT，每帧的 data 只是这一片。
 */
typedef struct HsIpcMessage {
    uint16_t type;             /**< HsIpcType */
    uint16_t flags;            /**< HsIpcFlag，发送时忽略 */
    uint32_t seq;              /**< 该通道发送序号，从 1 递增 */
    uint64_t timeMs;           /**< 发送方 GetTickCount64 */
    const char* name;          /**< Signal / Image / File 的名字 */
    int32_t result;            /**< Signal：HsIpcResult */
    int32_t code;              /**< Signal：双方约定的整数码，没有则 0 */
    const char* text;          /**< Signal 短文本，可空 */
    int statCount;             /**< Stats 项数 */
    const HsIpcStat* stats;    /**< Stats 数组 */
    uint16_t format;           /**< Image：HsIpcImageFormat */
    uint32_t width;
    uint32_t height;
    uint32_t stride;           /**< 原始像素行步长。JPEG 填 0 */
    uint32_t frameSeq;         /**< 拍号 */
    const uint8_t* data;       /**< 图像像素或文件内容 */
    uint32_t dataBytes;
    uint64_t totalBytes;       /**< File 总长度 */
    uint32_t crc32;            /**< File 校验，没有则 0 */
    const char* path;          /**< File 双方都能读的绝对路径。空表示内容在 data 或槽里 */
    uint32_t ackSeq;           /**< Ack 被确认的序号 */
    int32_t ackResult;         /**< Ack：0 成功，非 0 为双方约定的失败码 */
} HsIpcMessage;

typedef struct HsIpcSession HsIpcSession;

/**
 * @brief 创建或打开通道
 * @param err 失败原因，可空。errBytes 含结尾 0
 * @return 失败返回空。套接字必须填 port
 */
HS_IPC_API HsIpcSession* hs_ipc_open(const HsIpcConfig* cfg, char* err, int errBytes);
/** @brief 关闭通道。session 为空时直接返回 */
HS_IPC_API void hs_ipc_close(HsIpcSession* session);
/**
 * @brief 发送一条报文
 * @return HS_IPC_OK，或 HS_IPC_ERR_FULL（环或队列或三个槽都满）、HS_IPC_ERR_SIZE、HS_IPC_ERR_ARG、HS_IPC_ERR_IO
 *
 * 共享内存上，图像和文件内容放进槽，环里只放槽号。有 path 的文件只走环，不占槽。
 * 其它三种传输上，dataBytes 大于 HS_IPC_CHUNK_BYTES 时拆成多帧。
 */
HS_IPC_API int hs_ipc_send(HsIpcSession* session, const HsIpcMessage* msg);
/**
 * @brief 取一条报文
 * @param timeoutMs 小于 0 一直等，0 只看已经到达的
 * @return HS_IPC_OK、HS_IPC_TIMEOUT，或 HS_IPC_ERR_PROTOCOL / HS_IPC_ERR_IO
 */
HS_IPC_API int hs_ipc_poll(HsIpcSession* session, HsIpcMessage* msg, int timeoutMs);
/**
 * @brief 上次成功 poll 的本机毫秒时间
 * @return 还没收到过则是 0。库不判断心跳超时
 */
HS_IPC_API uint64_t hs_ipc_last_recv_ms(HsIpcSession* session);

/**
 * @brief 一块由调用方自己排版的命名共享内存
 *
 * 短名规则与通道名相同。对象名是 Global\短名 或 Local\短名，不加 HsIpc. 前缀。
 * 先试 Global\，没有权限时退回 Local\。事件必须跟这块内存使用同一个命名空间。
 */
typedef struct HsIpcRegion HsIpcRegion;
/** @brief 自动复位的命名事件，用来叫醒等在共享内存上的另一方 */
typedef struct HsIpcNotify HsIpcNotify;

/**
 * @brief 创建或打开一块共享内存并映射
 * @param created 可空。本次新建为 1，打开已有对象为 0
 * @param isGlobal 可空。落在 Global\ 为 1，退回 Local\ 为 0
 */
HS_IPC_API HsIpcRegion* hs_ipc_region_open(const char* name, uint32_t bytes,
    int* created, int* isGlobal, char* err, int errBytes);
/** @brief 解除映射。region 为空时直接返回 */
HS_IPC_API void hs_ipc_region_close(HsIpcRegion* region);
/** @brief 映射基址。未打开时为空 */
HS_IPC_API void* hs_ipc_region_data(HsIpcRegion* region);
/** @brief 映射字节数 */
HS_IPC_API uint32_t hs_ipc_region_size(HsIpcRegion* region);

/**
 * @brief 创建或打开自动复位事件
 * @param isGlobal 与对应共享内存的 isGlobal 相同，避免服务和界面各等各的
 */
HS_IPC_API HsIpcNotify* hs_ipc_notify_open(int isGlobal, const char* name, char* err, int errBytes);
/** @brief 关闭事件。notify 为空时直接返回 */
HS_IPC_API void hs_ipc_notify_close(HsIpcNotify* notify);
/** @brief 唤醒一个等待者 */
HS_IPC_API void hs_ipc_notify_signal(HsIpcNotify* notify);
/**
 * @brief 等到被唤醒或超时
 * @return HS_IPC_OK、HS_IPC_TIMEOUT，或 HS_IPC_ERR_IO
 */
HS_IPC_API int hs_ipc_notify_wait(HsIpcNotify* notify, int timeoutMs);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
#include <string>

namespace HsIpc {

/**
 * @brief C++ 工厂。不从 DLL 导出，只转调 C 接口，避免两边编译器虚表不一致
 * @note 不可拷贝。析构时关闭通道。
 */
class Session {
public:
    static Session open(const HsIpcConfig& cfg, std::string* err)
    {
        Session s;
        char buf[256];
        buf[0] = 0;
        s.m_session = hs_ipc_open(&cfg, buf, (int)sizeof(buf));
        if (!s.m_session && err) {
            *err = buf;
        }
        return s;
    }

    Session() : m_session(0) {}
    ~Session() { hs_ipc_close(m_session); }

    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    Session(Session&& other) : m_session(other.m_session) { other.m_session = 0; }
    Session& operator=(Session&& other)
    {
        if (this != &other) {
            hs_ipc_close(m_session);
            m_session = other.m_session;
            other.m_session = 0;
        }
        return *this;
    }

    bool opened() const { return m_session != 0; }
    int send(const HsIpcMessage& msg) { return hs_ipc_send(m_session, &msg); }
    int poll(HsIpcMessage* msg, int timeoutMs) { return hs_ipc_poll(m_session, msg, timeoutMs); }
    uint64_t lastRecvMs() const { return hs_ipc_last_recv_ms(m_session); }

private:
    HsIpcSession* m_session;
};

/**
 * @brief 一块由调用方自己排版的共享内存。不从 DLL 导出，只转调 C 接口
 * @note 不可拷贝。析构时解除映射。
 */
class Region {
public:
    static Region open(const char* name, uint32_t bytes, int* created, int* isGlobal, std::string* err)
    {
        Region region;
        char buf[256];
        buf[0] = 0;
        region.m_region = hs_ipc_region_open(name, bytes, created, isGlobal, buf, (int)sizeof(buf));
        if (!region.m_region && err) {
            *err = buf;
        }
        return region;
    }

    Region() : m_region(0) {}
    ~Region() { hs_ipc_region_close(m_region); }

    Region(const Region&) = delete;
    Region& operator=(const Region&) = delete;

    Region(Region&& other) : m_region(other.m_region) { other.m_region = 0; }
    Region& operator=(Region&& other)
    {
        if (this != &other) {
            hs_ipc_region_close(m_region);
            m_region = other.m_region;
            other.m_region = 0;
        }
        return *this;
    }

    bool opened() const { return m_region != 0; }
    void* data() const { return hs_ipc_region_data(m_region); }
    uint32_t size() const { return hs_ipc_region_size(m_region); }

private:
    HsIpcRegion* m_region;
};

/**
 * @brief 自动复位的命名事件。不从 DLL 导出，只转调 C 接口
 * @note 不可拷贝。析构时关闭。
 */
class Notify {
public:
    static Notify open(int isGlobal, const char* name, std::string* err)
    {
        Notify notify;
        char buf[256];
        buf[0] = 0;
        notify.m_notify = hs_ipc_notify_open(isGlobal, name, buf, (int)sizeof(buf));
        if (!notify.m_notify && err) {
            *err = buf;
        }
        return notify;
    }

    Notify() : m_notify(0) {}
    ~Notify() { hs_ipc_notify_close(m_notify); }

    Notify(const Notify&) = delete;
    Notify& operator=(const Notify&) = delete;

    Notify(Notify&& other) : m_notify(other.m_notify) { other.m_notify = 0; }
    Notify& operator=(Notify&& other)
    {
        if (this != &other) {
            hs_ipc_notify_close(m_notify);
            m_notify = other.m_notify;
            other.m_notify = 0;
        }
        return *this;
    }

    bool opened() const { return m_notify != 0; }
    void signal() { hs_ipc_notify_signal(m_notify); }
    int wait(int timeoutMs) { return hs_ipc_notify_wait(m_notify, timeoutMs); }

private:
    HsIpcNotify* m_notify;
};

}
#endif

#endif
