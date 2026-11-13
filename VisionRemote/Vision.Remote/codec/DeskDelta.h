/**
 * @file DeskDelta.h
 * @brief 桌面差分。静止不传，整块平移只发搬移，新露出来的区域才编码
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_REMOTE_DESK_DELTA_H
#define VISION_REMOTE_DESK_DELTA_H

#include "Messages.h"

#include <mutex>

namespace Vision {

/**
 * @class DeskDelta
 * @brief 对比上一帧，把画面收成搬移、纯色和少量 JPEG 补丁
 * @note 编码在被控端，解码在中控台。两边各持有一份上一帧。JPEG、H.264、原图不走这里
 */
class DeskDelta {
public:
    DeskDelta();

    /** @brief 丢掉参考帧，下一帧会发整幅关键帧 */
    void reset();

    /**
     * @brief 由当前 BGRA 生成一帧负载
     * @param key 为 true 时负载是整幅 JPEG，中控台应换掉参考帧
     * @return 画面没有变化时返回 false
     */
    bool encode(const unsigned char* bgra, int width, int height, int quality, CommBytes* payload, bool* key);

    /**
     * @brief 把一帧负载贴回参考帧，并给出可显示的 BGRA
     * @return 还没有关键帧、或负载损坏时返回 false
     */
    bool apply(const CommBytes& payload, CommBytes* bgra, int* width, int* height);

private:
    bool ensureGdi();
    bool encodeJpeg(const unsigned char* bgra, int width, int height, int quality, CommBytes* jpeg);
    bool decodeJpeg(const unsigned char* jpeg, int bytes, CommBytes* bgra, int* width, int* height);
    bool sameBlock(const unsigned char* prev, const unsigned char* cur, int imgW, int imgH,
        int x, int y, int ox, int oy, int w, int h) const;
    int probe(const unsigned char* prev, const unsigned char* cur, int imgW, int imgH,
        int x, int y, int ox, int oy, int w, int h) const;
    bool locate(const unsigned char* prev, const unsigned char* cur, int imgW, int imgH,
        int x, int y, int w, int h, int* ox, int* oy) const;
    bool solidBlock(const unsigned char* cur, int imgW, int x, int y, int w, int h, uint32_t* color) const;
    bool writeKeyframe(const unsigned char* bgra, int width, int height, int quality, CommBytes* payload);
    static void putU8(CommBytes* out, uint8_t v);
    static void putU16(CommBytes* out, uint16_t v);
    static void putI16(CommBytes* out, int v);
    static void putU32(CommBytes* out, uint32_t v);
    static bool takeU8(const CommBytes& in, size_t* off, uint8_t* v);
    static bool takeU16(const CommBytes& in, size_t* off, uint16_t* v);
    static bool takeI16(const CommBytes& in, size_t* off, int* v);
    static bool takeU32(const CommBytes& in, size_t* off, uint32_t* v);
    void blit(const CommBytes& src, CommBytes* dst, int imgW, int imgH, int sx, int sy, int dx, int dy, int w, int h) const;
    void fill(CommBytes* dst, int imgW, int imgH, int x, int y, int w, int h, uint32_t color) const;

    std::mutex m_mu;
    CommBytes m_prev;
    int m_w;
    int m_h;
    bool m_gdi;
};

}

#endif
