/**
 * @file AgentApp.h
 * @brief Ubuntu 被控端会话
 *
 * 直连听 19102，也可以按参数反连中控台。中控台无论哪一种都是 TLS 服务端，
 * 这里只做 TLS 客户端。同一时间只保留一路会话。画面在收到订阅后才发。
 */

#ifndef REMOTE_CLIENT_AGENT_APP_H
#define REMOTE_CLIENT_AGENT_APP_H

#include "Desk.h"
#include "FileBox.h"
#include "Identity.h"
#include "TlsLink.h"
#include "Beacon.h"
#include "Frame.h"

#include <atomic>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace Vision {

class AgentApp {
public:
    /**
     * @brief 打开桌面、生成身份、开始听端口
     * @param argc argv[1] 是中控台地址时额外反连；argv[2] 是反连端口，默认 19100
     * @return 0 正常；身份、桌面或监听失败返回 1
     */
    int run(int argc, char** argv);

    /**
     * @brief 状态变化时回调。四个字符串依次是设备号、指纹、桌面、状态
     * @note 会在会话线程调用。界面要自己切回界面线程
     */
    void setStatusHandler(const std::function<void(const std::string&, const std::string&, const std::string&, const std::string&)>& handler);
    /**
     * @brief 送出一帧后回调。jpeg 是这一帧的 JPEG，宽高为 0 表示当前没有在送
     * @note 会在会话线程调用，大约每 400 毫秒一帧，避免把界面队列塞满
     */
    void setPreviewHandler(const std::function<void(const std::string&, int, int)>& handler);
    /**
     * @brief 窗口列表变化时回调。每一项是「WM_CLASS\\tWM_NAME」
     * @note 会在会话线程调用
     */
    void setWindowHandler(const std::function<void(const std::vector<std::string>&)>& handler);
    /**
     * @brief 中控台改了共享范围时回调。desktop 为 true 表示整屏
     * @note 会在会话线程调用
     */
    void setShareHandler(const std::function<void(bool, const std::string&, const std::string&)>& handler);
    /**
     * @brief 收到或发出一条聊天时回调。名字、正文、类型（1 表示弹窗）
     * @note 会在会话线程调用
     */
    void setChatHandler(const std::function<void(const std::string&, const std::string&, int)>& handler);
    /** @brief 文件状态变化时回调。会在会话线程调用 */
    void setFileHandler(const std::function<void(const std::string&)>& handler);
    /**
     * @brief 一次性验证码变化时回调。六位数字
     * @note 会在会话线程或界面线程调用
     */
    void setCodeHandler(const std::function<void(const std::string&)>& handler);
    /** @brief 换一个六位验证码。10 分钟到期或认证成功时也会换 */
    std::string refreshCode();
    /**
     * @brief 界面选了整屏或某一个窗口
     * @note 可以在界面线程调用，下一轮会话循环才真正切换
     */
    void chooseShare(bool desktop, const std::string& process, const std::string& title);
    /**
     * @brief 界面发出一条聊天。还没连上时只在界面上说一声
     * @note 可以在界面线程调用
     */
    void submitChat(const std::string& text);

private:
    /** @brief /proc 里这个进程的可执行文件路径。读不到返回空 */
    std::string exeOf(int pid) const;
    /** @brief 结束已经在跑的同一份被控端，避免再占 19102 */
    void stopPrevious();
    /** @brief 发给 Hello 和局域网发现的同一份身份 */
    HelloMsg helloMsg() const;
    /** @brief 通知界面。handler 为空时不做事 */
    void note(const std::string& state);
    /** @brief 在全部网卡上听直连端口 */
    bool listenOn(int port, std::string* err);
    /** @brief 等一个直连。超时返回 -1 */
    int acceptOne(int timeoutMs);
    /** @brief 连中控台。失败返回 -1，调用方下一轮再试 */
    int dial(const std::string& host, int port);
    /** @brief 握手、发 Hello，然后读包并发画面，直到断开 */
    void serve(int fd);
    /** @brief 处理一包。认证请求直接同意；未取得操作权时不注入键鼠 */
    void handle(const Frame::Packet& packet);
    /** @brief 上报主机名、设备号、系统和证书指纹 */
    void sendHello();
    /** @brief 到点就抓一帧 JPEG，并每秒发一次心跳 */
    void sendPictures();
    /** @brief 把刚送出的 JPEG 交给界面。空串表示清掉预览 */
    void showPreview(const CommBytes& jpeg, int w, int h);
    /** @brief 写出已经组好的 VRMT 字节 */
    bool sendPacked(const CommBytes& bytes);
    /**
     * @brief 按订阅或配置更新帧率、JPEG 质量和长边
     * @param thumb true 时改缩略图长边，false 时改主画面
     */
    void tune(uint16_t fps, uint8_t quality, uint16_t longEdge, bool thumb);
    /** @brief 把界面上选的共享范围套到桌面。变了就重新发 Hello */
    void takeShare();
    /** @brief 验证码过了 10 分钟就换一个 */
    void touchCode();
    /** @brief 每隔两秒把窗口列表推给界面 */
    void refreshWindows();
    /** @brief 把排队的聊天发出去 */
    void drainChat();
    /** @brief 发出文件通道回包，并在状态变了时通知界面 */
    void flushFile(const std::vector<CommBytes>& frames);
    /** @brief 中控台改了共享范围。整屏和窗口来回切时会放开操作权 */
    void applyConfig(const ConfigMsg& cfg);

    Identity m_id;              ///< 设备号和证书
    Desk m_desk;                ///< X11 桌面
    FileBox m_files;            ///< 文件通道
    TlsLink m_link;             ///< 当前这一路 TLS
    Frame m_frame;              ///< 半包拼装
    std::string m_consoleHost;  ///< 反连地址，空则只等直连
    int m_consolePort;          ///< 反连端口，默认 19100
    uint64_t m_nextDial;        ///< 反连失败后，下一次再试的单调时钟毫秒
    int m_listenFd;             ///< 直连监听套接字
    bool m_mainOn;              ///< 中控台订了主码流
    bool m_thumbOn;             ///< 中控台订了缩略图
    bool m_operating;           ///< 已同意操作，才注入键鼠
    uint32_t m_perms;           ///< 这一路认证同意的权限
    int m_mainEdge;             ///< 主画面长边
    int m_thumbEdge;            ///< 缩略图长边
    int m_quality;              ///< JPEG 质量
    int m_fps;                  ///< 帧率上限，最高按 30 发
    int m_imageW;               ///< 最近一帧发给中控台的宽
    int m_imageH;               ///< 最近一帧发给中控台的高
    uint64_t m_nextFrame;       ///< 下一帧的单调时钟毫秒
    uint64_t m_nextBeat;        ///< 下一次心跳的单调时钟毫秒
    uint64_t m_nextPreview;     ///< 下一张送出预览的单调时钟毫秒
    uint64_t m_nextWindows;     ///< 下一次刷新窗口列表的单调时钟毫秒
    Beacon m_beacon;            ///< UDP 19101 发现应答
    std::string m_deskLine;     ///< 界面上的桌面描述
    std::string m_fileNote;     ///< 上次推给界面的文件状态
    std::string m_windowSnap;   ///< 上次推给界面的窗口列表，没变就不刷
    std::atomic<bool> m_up;     ///< 这一路会话还在
    bool m_sharePending;        ///< 界面改过共享范围，会话线程还没套上
    bool m_shareDesktop;        ///< 界面要的是整屏
    std::string m_shareProcess; ///< 界面要的进程名
    std::string m_shareTitle;   ///< 界面要的窗口标题
    std::mutex m_shareMu;       ///< 保护界面写来的共享范围
    std::deque<std::string> m_chatOut; ///< 还没发出去的聊天
    std::mutex m_chatMu;        ///< 保护聊天队列
    std::function<void(const std::string&, const std::string&, const std::string&, const std::string&)> m_status;
    std::function<void(const std::string&, int, int)> m_preview;
    std::function<void(const std::vector<std::string>&)> m_windows;
    std::function<void(bool, const std::string&, const std::string&)> m_shareUi;
    std::function<void(const std::string&, const std::string&, int)> m_chat;
    std::function<void(const std::string&)> m_fileUi;
    std::function<void(const std::string&)> m_codeUi; ///< 验证码变化时通知界面
    std::string m_code;         ///< 当前六位验证码
    uint64_t m_codeAt;          ///< 验证码生成时刻
    std::mutex m_codeMu;        ///< 保护验证码
};

}

#endif
