/**
 * @file Desk.h
 * @brief 当前登录会话的 X11 桌面
 * @note 抓整屏，按长边缩小后压 JPEG。键鼠用 XTest 注入，虚拟键码按 Windows VK 对照。
 *       登录界面和锁屏不在这个显示里，注入不到。
 */

#ifndef REMOTE_CLIENT_DESK_H
#define REMOTE_CLIENT_DESK_H

#include "Messages.h"

#include <X11/Xlib.h>
#include <X11/extensions/XShm.h>

#include <string>
#include <utility>
#include <vector>

namespace Vision {

class Desk {
public:
    Desk();
    ~Desk();

    /** @brief 打开当前用户的 X11。已有 DISPLAY 就用它，否则用登录会话的 :1 或 :0 */
    bool open(std::string* err);
    /** @brief 显示已经打开 */
    bool ready() const;
    /** @brief 例如「:1 1280x960」。还没打开时为空 */
    std::string summary() const;

    /**
     * @brief 抓整屏，按长边缩小后压成 JPEG
     * @param longEdge RemoteConst::kNativeEdge（65535）表示不缩小
     * @param quality JPEG 质量 1–100
     */
    bool grabJpeg(int longEdge, int quality, CommBytes* jpeg, int* imageW, int* imageH, std::string* err);
    /**
     * @brief 切换抓整屏还是某一个窗口
     * @param desktop true 抓整屏。false 时用进程名（WM_CLASS）和标题（WM_NAME）包含匹配
     * @note 只在会话线程调用
     */
    void setShare(bool desktop, const std::string& process, const std::string& title);
    /** @brief 当前是整屏 */
    bool desktop() const;
    /** @brief Hello 里的来源：桌面、进程名或窗口标题 */
    std::string source() const;
    /** @brief 当前窗口匹配用的进程名。整屏时为空 */
    std::string process() const;
    /** @brief 当前窗口匹配用的标题。整屏时为空 */
    std::string title() const;
    /**
     * @brief 当前桌面上能看见的窗口。每一项是 WM_CLASS 和 WM_NAME
     * @note 只在会话线程调用
     */
    std::vector<std::pair<std::string, std::string>> windows();
    /**
     * @brief 把图像坐标的键鼠送进桌面
     * @param imageW imageH 是最近一次发给中控台的画面尺寸，用来把坐标换回屏幕像素
     */
    void inject(const InputMsg& msg, int imageW, int imageH);
    /** @brief 光标在最近一帧图像里的坐标。图像尺寸为 0 时返回 false */
    bool pointer(int imageW, int imageH, int* x, int* y) const;

private:
    /** @brief 按当前桌面尺寸挂上共享内存图像，失败则留给 XGetImage */
    bool attachShm(std::string* err);
    /** @brief 卸下共享内存图像 */
    void releaseShm();
    /** @brief 把根窗口读进 m_image。尺寸变了会重建共享内存 */
    bool readRoot(std::string* err);
    /** @brief 按当前共享范围读整屏或某一个窗口，结果在 m_pixels */
    bool readFrame(std::string* err);
    /** @brief 按进程名和标题找面积最大的那个窗口。没有返回 0 */
    Window findWindow() const;
    /** @brief 递归收集可见窗口 */
    void collect(Window window, int depth, std::vector<std::pair<std::string, std::string>>* out);
    /** @brief ASCII 小写，用来做包含匹配 */
    static std::string lower(const std::string& text);
    /** @brief needle 为空算匹配上，否则 hay 里要包含它 */
    static bool hasText(const std::string& hay, const std::string& needle);
    /** @brief 读窗口的 WM_CLASS 和 WM_NAME。读不到就留空 */
    void windowName(Window window, std::string* process, std::string* title) const;
    /** @brief Windows 虚拟键码换成 X11 KeySym。不认识返回 0 */
    static unsigned long keySym(uint16_t vk);
    /** @brief 吃掉 XShmAttach 的 BadAccess，避免默认处理把进程直接杀掉 */
    static int onXError(Display* display, XErrorEvent* event);

    static int m_xError;        ///< 最近一次被接住的 X 错误码，0 表示没有

    Display* m_dpy;             ///< X11 连接
    Window m_root;              ///< 根窗口
    int m_rootW;                ///< 桌面宽
    int m_rootH;                ///< 桌面高
    XImage* m_image;            ///< 整屏那一帧，共享内存或 XGetImage
    XImage* m_winImage;         ///< 窗口那一帧，普通 XGetImage
    XImage* m_pixels;           ///< 这一帧真正拿去压缩的图像
    XShmSegmentInfo m_shm;      ///< 共享内存段
    bool m_shmOn;               ///< 正在用共享内存
    bool m_desktop;             ///< true 抓整屏
    std::string m_process;      ///< 窗口匹配：WM_CLASS
    std::string m_title;        ///< 窗口匹配：WM_NAME
    int m_srcW;                 ///< 这一帧的宽，窗口或整屏
    int m_srcH;                 ///< 这一帧的高
    int m_capX;                 ///< 这一帧在屏幕上的左上角
    int m_capY;                 ///< 这一帧在屏幕上的左上角
    std::vector<unsigned char> m_rgb; ///< 缩小后的 RGB，送给 libjpeg
};

}

#endif
