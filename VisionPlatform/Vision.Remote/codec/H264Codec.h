/**
 * @file H264Codec.h
 * @brief 主码流 H.264：优先 Media Foundation 硬编/硬解，失败再 LoadLibrary 加载 OpenH264
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_REMOTE_H264_CODEC_H
#define VISION_REMOTE_H264_CODEC_H

#include <cstdint>
#include <vector>

namespace Vision {

/**
 * @class H264Codec
 * @brief 把 BGRA 编成 Annex-B，或把 Annex-B 解回 BGRA
 * @note 编码器和解码器各自记住分辨率，尺寸变了会重建。OpenH264 只在进程能加载 openh264.dll 时启用，不随安装包带 DLL
 */
class H264Codec {
public:
    H264Codec();
    ~H264Codec();

    /**
     * @brief 编码一帧。长边超过 longEdge 时先缩小，宽高取偶数
     * @param annexb 输出 Annex-B；key 为 1 表示这一帧是关键帧
     * @return 硬编和 OpenH264 都不可用时返回 false，调用方改走 JPEG
     */
    bool encode(const std::vector<unsigned char>& bgra, int width, int height, int longEdge, int fps, int bitrate,
        std::vector<unsigned char>* annexb, int* outW, int* outH, uint8_t* key);

    /**
     * @brief 解码一帧 Annex-B
     * @param bgra 输出自上而下 BGRA，长度 width*height*4
     */
    bool decode(const unsigned char* data, int size, std::vector<unsigned char>* bgra, int* width, int* height);

    /** @brief 下一帧强制成关键帧。订阅刚打开或共享源切换时，解码器没有参考帧 */
    void requestKey();
    /** @brief 关掉编码器，下次 encode 重建 */
    void closeEncoder();
    /** @brief 关掉解码器 */
    void closeDecoder();

private:
    /** @brief 缩到长边限制内，宽高向下取偶数 */
    bool fit(const unsigned char* src, int srcW, int srcH, int longEdge, std::vector<unsigned char>* dst, int* dstW, int* dstH) const;
    /** @brief BGRA 转 NV12，供 Media Foundation */
    void toNv12(const unsigned char* bgra, int width, int height, std::vector<unsigned char>* nv12) const;
    /** @brief BGRA 转 I420，供 OpenH264 */
    void toI420(const unsigned char* bgra, int width, int height, std::vector<unsigned char>* i420) const;
    /** @brief NV12 转 BGRA */
    void fromNv12(const unsigned char* nv12, int width, int height, std::vector<unsigned char>* bgra) const;
    /** @brief 打开 Media Foundation 同步或异步编码器 */
    bool openMf(int width, int height, int fps, int bitrate);
    /** @brief 打开已加载的 OpenH264。DLL 不在时返回 false */
    bool openOpen(int width, int height, int fps, int bitrate);
    /** @brief 送一帧给当前编码器 */
    bool encodeFrame(const unsigned char* bgra, int width, int height, bool forceKey, std::vector<unsigned char>* annexb);
    /** @brief 从样本里取出字节，长度前缀 NAL 补成起始码 */
    bool sampleBytes(void* sample, std::vector<unsigned char>* annexb) const;

    /** @brief OpenH264 2.x 编码器前几个虚函数，只在加载到 DLL 时用 */
    struct OpenParam {
        int usage;
        int width;
        int height;
        int bitrate;
        int rc;
        float fps;
    };
    struct OpenPicture {
        int format;
        int stride[4];
        unsigned char* data[4];
        int width;
        int height;
        long long stamp;
    };
    struct OpenLayer {
        unsigned char temporal;
        unsigned char spatial;
        unsigned char quality;
        int frameType;
        int nalCount;
        int* nalBytes;
        unsigned char* bits;
    };
    struct OpenFrame {
        int layers;
        OpenLayer layer[128];
        int frameType;
        int bytes;
        long long stamp;
    };
    /** @brief 只声明本程序会调到的前几个虚函数，顺序必须与 openh264 一致 */
    struct OpenEncoder {
        virtual int __stdcall Initialize(const OpenParam*) = 0;
        virtual int __stdcall InitializeExt(const void*) = 0;
        virtual int __stdcall GetDefaultParams(void*) = 0;
        virtual int __stdcall Uninitialize() = 0;
        virtual int __stdcall EncodeFrame(const OpenPicture*, OpenFrame*) = 0;
        virtual int __stdcall EncodeParameterSets(OpenFrame*) = 0;
        virtual int __stdcall ForceIntraFrame(bool) = 0;
    };
    typedef int(__stdcall* OpenCreate)(OpenEncoder**);
    typedef void(__stdcall* OpenDestroy)(OpenEncoder*);

    void* m_mf;             ///< IMFTransform 编码器
    void* m_mfEvents;       ///< 异步编码器的 IMFMediaEventGenerator，同步时为空
    void* m_dec;            ///< IMFTransform 解码器
    void* m_open;           ///< OpenH264 ISVCEncoder
    void* m_openLib;        ///< openh264.dll
    int m_encW;             ///< 编码器宽，变了就重建
    int m_encH;             ///< 编码器高
    int m_decW;             ///< 解码器宽
    int m_decH;             ///< 解码器高
    int m_fps;              ///< 编码帧率
    int m_bitrate;          ///< 编码码率，比特每秒
    int m_frames;           ///< 已编码帧数，用于周期性关键帧
    bool m_mfAsync;         ///< 硬件编码器走异步事件
    bool m_useOpen;         ///< 正在用 OpenH264，而不是 Media Foundation
    long long m_pts;        ///< 下一帧时间戳
};

}

#endif
