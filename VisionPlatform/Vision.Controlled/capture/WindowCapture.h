/**
 * @file WindowCapture.h
 * @brief 抓目标窗口客户区并压成 JPEG
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_CONTROLLED_WINDOW_CAPTURE_H
#define VISION_CONTROLLED_WINDOW_CAPTURE_H

#include "Messages.h"
#include "WgcCapture.h"
#include <windows.h>

namespace Vision {

/**
 * @class WindowCapture
 * @brief 抓窗口客户区或整块显示器，按长边与 JPEG 质量缩放压缩
 * @note 无状态，只在被控端抓屏线程里用
 */
class WindowCapture {
public:
    /** @brief hwnd 为空或窗口最小化时，JPEG 为空，visible 为 0 */
    PictureMsg grab(HWND hwnd, bool operable, int jpegQuality = RemoteConst::kJpegQuality,
        int longEdge = RemoteConst::kLongEdge) const;
    /** @brief 把已抓到的 BGRA 压成 JPEG。monitor==255 的整屏拼接走这里 */
    PictureMsg fromBgra(CommBytes& bgra, int width, int height, int clientW, int clientH, bool operable,
        int jpegQuality, int longEdge) const;
    /** @brief 抓一块显示器。origin 是这块屏在虚拟屏幕上的左上角 */
    PictureMsg grabDesktop(int monitor, bool operable, int* originX, int* originY,
        int jpegQuality = RemoteConst::kJpegQuality, int longEdge = RemoteConst::kLongEdge) const;
    /** @brief 当前接着的显示器块数，至少 1 */
    static int monitorCount();
    /**
     * @brief 抓窗口像素。先试 Windows Graphics Capture，失败再用 PrintWindow
     * @return 拿到 BGRA 时返回 true
     */
    bool grabBgra(HWND hwnd, CommBytes* bgra, int* width, int* height) const;
    /** @brief 采样若干点，全接近黑色时返回 true。DXGI 首帧常是空的，这时改走 GDI */
    bool blank(const CommBytes& bgra, int width, int height) const;
    /** @brief 把一小块 BGRA 压成 PNG，给光标形状用 */
    bool pngFromBgra(const unsigned char* bgra, int width, int height, CommBytes* png) const;

private:
    /**
     * @brief 把窗口客户区画进 BGRA 缓冲：先 PrintWindow，失败再从窗口 DC BitBlt
     * @param bgra 输出 width*height*4 字节，自上而下
     * @return 画成功且取到像素时返回 true
     */
    bool captureClient(HWND hwnd, int width, int height, CommBytes* bgra) const;
    /**
     * @brief 填好尺寸与标志，visible 时把 BGRA 缩放后压成 JPEG
     * @param bgra 会被就地改写：alpha 全部置 255
     */
    PictureMsg encode(CommBytes& bgra, int width, int height, bool visible, bool operable,
        int jpegQuality, int longEdge) const;
    /** @brief EnumDisplayMonitors 回调：数到第 MonitorPick::want 块时记下它的矩形 */
    static BOOL CALLBACK onMonitor(HMONITOR monitor, HDC dc, LPRECT rect, LPARAM param);

    mutable WgcCapture m_wgc;   ///< 窗口捕获。换窗口或失败时退回 PrintWindow
};

}

#endif
