/**
 * @file SharedFrames.h
 * @brief 本机三进程共享的命令、状态和画面槽
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 *
 * 名字是 Global\Vision.Ipc。会话 0 的服务和已登录用户的托盘映射同一块。
 * 画面只留最新一帧，编码直接写入槽，不在堆上再留一整份副本。
 */

#ifndef VISION_SHARED_FRAMES_H
#define VISION_SHARED_FRAMES_H
#include "VisionIpcExport.h"

#include <stdint.h>
#include <string>
#include <vector>

namespace Vision {

/** @brief 头部命令。托盘写入，Station 服务执行 */
enum IpcCommand {
    IpcCmdNone = 0,         ///< 无
    IpcCmdStartOwned = 1,   ///< 启动部署表里属于本机的工站
    IpcCmdStopOwned = 2,    ///< 只停止这些工站
    IpcCmdReload = 3,       ///< 工站已停止时重新读方案
    IpcCmdSetSubscribe = 4  ///< 打开或关闭接收其它工站画面
};

/** @brief 托盘图标用的总状态 */
enum IpcFault {
    IpcFaultNone = 0,     ///< 正常
    IpcFaultDown = 1,     ///< 该运行的服务还没起来
    IpcFaultLink = 2,     ///< 工站在跑，到服务端的链路断了
    IpcFaultRejected = 3, ///< Hello 被拒绝或工站报错
    IpcFaultClaim = 4     ///< 同一工站被两个节点认领
};

/** @brief 组网角色，与 node.json 的 role 一致 */
enum NodeRole {
    NodeRoleStandalone = 0, ///< 单机，不启动链路
    NodeRoleServer = 1,     ///< 服务端
    NodeRoleClient = 2      ///< 客户端
};

/**
 * @class IpcPreview
 * @brief 从槽里拷出的一帧显示数据。JPEG 已离开共享内存
 */
struct IpcPreview {
    std::string stationId; ///< 工站 id
    std::string nodeId;    ///< 所属节点
    std::string summary;   ///< 短文本
    std::vector<unsigned char> jpeg; ///< 显示图
    bool ok;               ///< 这一拍 OK
    bool offline;          ///< 对端已离线，画面是最后一帧
    bool remote;           ///< 来自远端槽
    int cycleMs;           ///< 节拍
    int total;             ///< 产量
    int pass;             ///< OK 数
    int fail;             ///< NG 数
    uint32_t frameSeq;     ///< 拍号
    /** @brief 标志清零、计数为 0 */
    IpcPreview()
        : ok(false)
        , offline(false)
        , remote(false)
        , cycleMs(0)
        , total(0)
        , pass(0)
        , fail(0)
        , frameSeq(0)
    {}
};

/**
 * @class SharedFrames
 * @brief 映射 Global\Vision.Ipc，并持有命令事件、新帧事件
 * @note 进程内一份。谁先 map 谁创建，后到的打开
 */
class VISION_IPC_API SharedFrames {
public:
    /************************************************** 容量 **************************************************/
    /** @brief 本机工站个数上限。槽数在创建时按这个上限定死 */
    static const int kMaxLocal = 8;
    /** @brief 远端工站个数上限 */
    static const int kMaxRemote = 16;
    /** @brief 每个本机工站的槽数。写的人避开正在读的那一槽 */
    static const int kLocalCopies = 3;
    /** @brief 每个远端工站的槽数 */
    static const int kRemoteCopies = 2;
    /** @brief 默认槽字节数，1 MB */
    static const uint32_t kDefaultSlotBytes = 1048576;

    /************************************************** 映射 **************************************************/
    /** @brief 进程内唯一映射 */
    static SharedFrames& instance();

    /**
     * @brief 创建或打开共享内存，并打开命令、新帧两个事件
     * @param slotBytes 每槽字节。小于 64KB 时抬到 64KB；只在新建时生效，打开已有对象时以头部记录为准
     * @param err 失败原因，可空
     * @return 映射和两个事件都成功才为 true；失败时不保留任何句柄
     */
    bool map(uint32_t slotBytes, std::string* err);
    /** @brief 是否已映射 */
    bool mapped() const;

    /************************************************** 命令 **************************************************/
    /** @brief 写入命令并触发 Global\Vision.Command。订阅命令额外写头部开关 */
    void writeCommand(IpcCommand cmd, uint32_t arg);
    /** @brief 堵到有命令或超时。超时返回 IpcCmdNone */
    IpcCommand waitCommand(int timeoutMs, uint32_t* arg, uint32_t* seq);
    /** @brief 打开或关闭接收其它工站画面 */
    void setSubscribe(bool on);
    /** @brief 运行画面是否打开着 */
    bool subscribe() const;

    /************************************************** 心跳状态 **************************************************/
    /** @brief 服务每秒写一次，托盘用它判断进程还在不在 */
    void beatStation();
    /** @brief 链路服务每秒写一次 */
    void beatLink();
    /** @brief 距上次工站心跳的毫秒。还没写过返回 -1 */
    int stationBeatAgeMs() const;
    /** @brief 距上次链路心跳的毫秒。还没写过返回 -1 */
    int linkBeatAgeMs() const;

    /** @brief 写拒绝原因和故障码，托盘读了改颜色 */
    void setFault(IpcFault fault, const std::string& reason);
    /** @brief 当前故障码 */
    IpcFault fault() const;
    /** @brief 拒绝或报错文本 */
    std::string faultText() const;
    /** @brief 记下本节点角色，托盘配色用 */
    void setRole(NodeRole role);
    /** @brief 本节点角色 */
    NodeRole role() const;

    /************************************************** 画面槽 **************************************************/
    /**
     * @brief 登记本机工站在槽表里的下标
     * @return 下标；满了返回 -1
     */
    int bindLocal(const std::string& stationId, const std::string& nodeId);
    /** @brief 登记远端工站。已有则返回原下标 */
    int bindRemote(const std::string& stationId, const std::string& nodeId);

    /**
     * @brief 把 JPEG 写入本机三个槽里当前没被发布的那一个
     * @return 放得下为 true。超出槽大小时不发布，原图应由调用方已经落盘
     */
    bool publishLocal(int index, uint32_t frameSeq, bool ok, int cycleMs,
                      int total, int pass, int fail, const std::string& summary,
                      const std::vector<unsigned char>& jpeg);
    /** @brief 链路把收到的画面写入远端槽 */
    bool publishRemote(int index, uint32_t frameSeq, bool ok, int cycleMs,
                       int total, int pass, int fail, bool offline,
                       const std::string& summary,
                       const std::vector<unsigned char>& jpeg);

    /** @brief 拷出本机最新一帧。没有新数据返回 false */
    bool readLocal(int index, IpcPreview* out) const;
    /** @brief 拷出远端最新一帧 */
    bool readRemote(int index, IpcPreview* out) const;
    /** @brief 已登记的本机工站数 */
    int localCount() const;
    /** @brief 已登记的远端工站数 */
    int remoteCount() const;
    /** @brief 按下标读本机工站 id */
    std::string localStationId(int index) const;
    /** @brief 按下标读远端工站 id */
    std::string remoteStationId(int index) const;

    /************************************************** 新帧事件 **************************************************/
    /** @brief 有新的本机帧时被触发。Link 没启动时 Station 仍然 SetEvent，不等待 */
    void signalFrame();
    /** @brief 等到新帧或超时 */
    bool waitFrame(int timeoutMs);

private:
    /** @brief 只允许 instance 构造 */
    SharedFrames();
    /** @brief 解除映射 */
    ~SharedFrames();
    /** @brief 禁止拷贝 */
    SharedFrames(const SharedFrames&);
    /** @brief 禁止赋值 */
    SharedFrames& operator=(const SharedFrames&);

    /** @brief 共享内存头部：命令、心跳、故障、角色、工站表与发布序号，布局见 .cpp */
    struct Head;
    /** @brief 单个画面槽：定长前缀（拍号、统计、摘要）后接 JPEG，布局见 .cpp */
    struct Slot;

    /** @brief 槽的起始地址。local 为真时 index 是本机工站，copy 是第几个副本 */
    Slot* slotAt(bool local, int index, int copy) const;
    /** @brief 发布到指定副本。seq 先改成奇数，写完再改成偶数 */
    bool publishAt(bool local, int index, int copies, uint32_t frameSeq, bool ok,
                   int cycleMs, int total, int pass, int fail, bool offline,
                   const std::string& summary, const std::vector<unsigned char>& jpeg);
    /** @brief 读最新偶数序号的副本 */
    bool readAt(bool local, int index, int copies, IpcPreview* out) const;

    /** @brief 拷进定长字符数组，保证有结尾 0 */
    void copyText(char* dst, int cap, const std::string& src) const;
    /** @brief 毫秒计时，心跳用 */
    uint64_t nowMs() const;
    /** @brief 头部加全部槽的总字节数；超出 32 位返回 0 */
    uint32_t layoutBytes(uint32_t slotBytes) const;

    /** @brief IpcBase 的共享内存与命令、新帧两个事件，隐藏 Windows 头 */
    struct Transport;

    /**
     * @brief map 失败收尾：释放未完成的共享内存与事件，写失败原因
     * @param transport 可空
     * @param reason 为空时保留 err 里已有的原因
     * @return 固定 false
     */
    bool failMap(Transport* transport, std::string* err, const char* reason) const;

    void* m_view;       ///< 映射基址，由基础库持有
    Head* m_head;       ///< 头部
    Transport* m_transport; ///< 共享内存和事件，IpcBase
    uint32_t m_slotBytes; ///< 每槽字节
};

}

#endif
