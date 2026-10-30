/**
 * @file Mp4Record.h
 * @brief 用 Media Foundation Sink Writer 把画面写成 H.264 MP4
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_CONTROL_MP4_RECORD_H
#define VISION_CONTROL_MP4_RECORD_H

#include <string>

class QImage;

namespace Vision {

/**
 * @class Mp4Record
 * @brief 会话录屏。start 失败时调用方改写 MJPEG AVI
 * @note 只在界面线程调用。宽高在 start 时固定，后续帧尺寸不一致则跳过
 */
class Mp4Record {
public:
    Mp4Record();
    ~Mp4Record();

    /** @brief 创建文件并开始写。fps 默认 5，与原来的 AVI 节奏一致 */
    bool start(const std::wstring& path, int width, int height, int fps);
    /** @brief 写入一帧。距上一帧不足一帧间隔时丢弃 */
    bool write(const QImage& image);
    /** @brief 收尾并关闭文件 */
    void stop();
    /** @brief 正在写 */
    bool active() const;

private:
    void* m_writer;             ///< IMFSinkWriter
    unsigned long m_stream;     ///< 视频流序号
    int m_width;                ///< 画面宽，偶数
    int m_height;               ///< 画面高，偶数
    int m_fps;                  ///< 帧率
    long long m_pts;            ///< 下一帧时间戳，100 纳秒
    long long m_lastMs;         ///< 上一帧写入时刻
    bool m_on;                  ///< 正在写
};

}

#endif
