/**
 * @file HttpPreview.h
 * @brief 局域网 HTTP 查看：浏览器打开地址即可看当前画面
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_CONTROLLED_HTTP_PREVIEW_H
#define VISION_CONTROLLED_HTTP_PREVIEW_H

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace Vision {

class AgentSession;

/**
 * @class HttpPreview
 * @brief 在本机所有网卡上听一个端口，提供首页、单张 JPEG 和 MJPEG 流
 * @note 只读画面，不提供操作。默认不启动，由界面打开
 */
class HttpPreview {
public:
    /** @brief 未监听 */
    HttpPreview();
    /** @brief 停掉监听并等线程退出 */
    ~HttpPreview();

    /** @brief 画面从这条会话取。必须在 start 之前设置，且在 stop 之前一直有效 */
    void setSession(AgentSession* session);

    /**
     * @brief 开始监听 0.0.0.0:port
     * @return 端口被占用或套接字失败时返回 false，err 可空
     */
    bool start(int port, std::string* err);
    /** @brief 停掉监听并等线程退出 */
    void stop();
    /** @brief 正在监听 */
    bool running() const;
    /** @brief 当前端口，未监听时为 0 */
    int port() const;
    /** @brief 局域网地址，形如 http://192.168.1.8:19180 。没有网卡地址时给本机回环 */
    std::vector<std::string> urls() const;

private:
    /** @brief 一个浏览器连接 */
    struct Client {
        uintptr_t sock;             ///< 浏览器套接字
        bool streaming;             ///< 正在推 MJPEG
        uint64_t sentSeq;           ///< 已推过的画面序号
        uint64_t lastMs;            ///< 上次推帧时刻
        std::string pending;        ///< 还没发完的字节
        Client() : sock(0), streaming(false), sentSeq(0), lastMs(0) {}
    };

    /** @brief 接受连接、读请求、给正在看的连接推帧 */
    void serve();
    /** @brief 读完一个 HTTP 请求后回复首页、单帧或切到推流 */
    void handle(Client* client);
    /** @brief 给推流连接补一帧。没新画面或发送失败时返回 false 表示拆掉连接 */
    bool push(Client* client);
    /** @brief 把缓冲发完。对端断开返回 false */
    bool writeAll(uintptr_t sock, const char* data, int size);
    /** @brief 本机 IPv4，不含回环 */
    std::vector<std::string> lanAddresses() const;
    /** @brief 开机以来的毫秒 */
    static uint64_t nowMs();
    /** @brief 首页 HTML，图片指向 /stream */
    static const char* page();

    AgentSession* m_session;        ///< 画面来源。stop 之前有效
    std::thread m_thread;           ///< 服务线程
    std::atomic<bool> m_stop;       ///< 线程是否该退出
    uintptr_t m_listen;             ///< 监听套接字
    int m_port;                     ///< 当前端口，未监听为 0
    mutable std::mutex m_mu;        ///< 保护连接表。画面本身由会话加锁
};

}

#endif
