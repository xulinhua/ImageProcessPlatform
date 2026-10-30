/**
 * @file DxgiCapture.h
 * @brief DXGI 桌面复制。没有变化时不拷像素
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_CONTROLLED_DXGI_CAPTURE_H
#define VISION_CONTROLLED_DXGI_CAPTURE_H

#include "Messages.h"

namespace Vision {

/**
 * @class DxgiCapture
 * @brief 按显示器序号做桌面复制。失败或超时由调用方改走 GDI
 */
class DxgiCapture {
public:
    DxgiCapture();
    ~DxgiCapture();

    /**
     * @brief 取一帧 BGRA。没有新画面返回 false，且 *changed 为 false
     * @param monitor 从 0 起；不支持 255（整屏拼接走 GDI）
     */
    bool grab(int monitor, CommBytes* bgra, int* width, int* height, int* originX, int* originY, bool* changed);

    /** @brief 丢掉桌面复制，下次 grab 重新打开 */
    void reset();

private:
    struct Impl;
    Impl* m_impl;               ///< 输出复制与保留帧
};

}

#endif
