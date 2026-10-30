/**
 * @file WgcCapture.h
 * @brief Windows Graphics Capture 抓窗口，失败时调用方改走 PrintWindow
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_CONTROLLED_WGC_CAPTURE_H
#define VISION_CONTROLLED_WGC_CAPTURE_H

#include "Messages.h"
#include <windows.h>

namespace Vision {

/**
 * @class WgcCapture
 * @brief 用 Windows.Graphics.Capture 抓一个窗口的客户区
 * @note 系统不支持、窗口无法捕获或还没有第一帧时 grab 返回 false
 */
class WgcCapture {
public:
    WgcCapture();
    ~WgcCapture();

    /**
     * @brief 取这个窗口最近一帧
     * @return 还没有帧或这个窗口不能用 WGC 时返回 false
     */
    bool grab(HWND hwnd, CommBytes* bgra, int* width, int* height);
    /** @brief 停掉当前捕获会话 */
    void reset();

private:
    struct Impl;
    Impl* m_impl;               ///< WinRT 捕获会话。失败过的窗口不再重试
};

}

#endif
