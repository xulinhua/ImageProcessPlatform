/**
 * @file HsImage.h
 * @brief HsImage头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef HSCOMMDATA_HSIMAGE_H
#define HSCOMMDATA_HSIMAGE_H

#include <HsCommData/HsException.h>

#include <cstdint>
#include <string>
#include <vector>

namespace HsCommData {

/** @brief 像素样本类型。决定每个样本占几个字节及有无符号 */
enum PixelKind {
    PixelKindUnknown = 0, ///< 未指定
    PixelByte,            ///< 8bit 无符号
    PixelInt16,           ///< 16bit 有符号
    PixelUInt16,          ///< 16bit 无符号
    PixelInt32,           ///< 32bit 有符号
    PixelReal32,          ///< 32bit 浮点
    PixelReal64,          ///< 64bit 浮点
    PixelComplex64,        ///< 64bit 复数。须配合 EncodeMono
    PixelKindMax ///< 个数，不是合法值
};

/** @brief 像素排布。决定通道顺序与平面布局，与 PixelKind 组合使用 */
enum PixelEncoding {
    EncodeUnknown = 0, ///< 未指定
    EncodeMono,        ///< 单通道
    EncodeRgb,         ///< R,G,B 交错
    EncodeBgr,         ///< B,G,R 交错
    EncodeRgba,        ///< R,G,B,A 交错
    EncodeBgra,        ///< B,G,R,A 交错
    EncodeYuyv,        ///< Y0,U,Y1,V。仅 PixelByte。每两像素 4 字节
    EncodeUyvy,        ///< U,Y0,V,Y1。仅 PixelByte
    EncodeYvyu,        ///< Y0,V,Y1,U。仅 PixelByte
    EncodeYuv444,      ///< Y,U,V 交错
    EncodeUyv,         ///< U,Y,V。PFNC YUV8_UYV
    EncodeYuv411,      ///< U,Y,Y,V,Y,Y。每四像素 6 字节
    EncodeBayerRG,     ///< Bayer 起始 R G
    EncodeBayerGR,     ///< Bayer 起始 G R
    EncodeBayerGB,     ///< Bayer 起始 G B
    EncodeBayerBG,     ///< Bayer 起始 B G
    EncodeRgb565,      ///< RGB 5-6-5。仅 PixelByte
    EncodeBgr565,      ///< BGR 5-6-5。仅 PixelByte
    EncodeNv12,        ///< Y 平面 + UV 交错
    EncodeNv21,        ///< Y 平面 + VU 交错
    EncodeI420,        ///< Y、U、V 三分量
    EncodeYv12,         ///< Y、V、U 三分量
    EncodeMax ///< 个数，不是合法值
};

/** @brief 存图文件格式。由 save 的枚举决定编码器，不单靠扩展名 */
enum ImageFileFormat {
    ImageFormatBmp = 0, ///< .bmp
    ImageFormatPng,     ///< .png
    ImageFormatJpeg,    ///< .jpg / .jpeg
    ImageFormatTiff     ///< .tif / .tiff
};

/**
 * @class HsImage
 * @brief 图像缓冲。kind 管样本宽度，encoding 管通道布局。不依赖 Qt
 */
class HSCOMMDATA_EXPORT HsImage {
public:
    int64_t frameId; ///< 帧号
    int width;       ///< 宽，像素
    int height;      ///< 高，像素
    int stride;      ///< 行字节数
    PixelKind kind;  ///< 样本类型
    PixelEncoding encoding; ///< 通道排布
    std::vector<unsigned char> pixels; ///< 像素。4:2:0 等可多平面紧挨

    HsImage(); ///< 空图
    /** @brief 深拷像素与元数据 */
    HsImage(const HsImage& other);
    /** @brief 深拷赋值 */
    HsImage& operator=(const HsImage& other);

    /** @brief 宽高非法或无缓冲 */
    bool isNull() const;
    /** @brief 每像素字节数。422/565 为 2，411 等按布局；未知为 0 */
    int bytesPerPixel() const;
    /** @brief 第 y 行只读指针。越界抛 HsException */
    const unsigned char* scanLine(int y) const;
    /** @brief 第 y 行可写指针。越界抛 HsException */
    unsigned char* scanLine(int y);

    /** @brief 单样本字节数。未知 kind 返回 0 */
    static int sampleBytes(PixelKind kind);
    /** @brief kind 与 encoding 组合后的每像素字节数。非法为 0 */
    static int bytesPerPixel(PixelKind kind, PixelEncoding encoding);
    /**
     * @brief 分配空图像（像素填零）
     * @param frameId 帧号，调用方可自定
     */
    static HsImage make(int width, int height, PixelKind kind, PixelEncoding encoding, int64_t frameId);
    /**
     * @brief 从原始缓冲拷贝
     * encoding 如 mono8/8uc1、rgb8/bgr8/rgba8/bgra8、yuyv/uyvy/yvyu/yuv444/uyv/yuv411/bayerrg8/rgb565/nv12/nv21/i420/yv12、
     * mono10/12/14/rgb16/bgr16/mono16、mono16s、mono32s、real32/mono32f、real64/mono64f/complex64、rgb32f
     * @return 失败时为空图，不抛异常
     */
    static HsImage fromRaw(const unsigned char* data, int width, int height, int strideBytes,
                           const char* encoding, int64_t frameId);

    /**
     * @brief 取灰度。byte 按 BT.601 压到 0-255；YUV 取 Y；int/real 直接转 double
     * @param gray 输出，不可空
     */
    bool sampleGray(int x, int y, double* gray) const;
    /** @brief 双线性取灰度。越界返回 false */
    bool sampleGray(double x, double y, double* gray) const;
    /**
     * @brief 取 RGB。单通道则三通道相同；Bayer 现场解到 0-255
     * @param r 红。int/real 按样本转 int
     * @param g 绿
     * @param b 蓝
     */
    bool sampleRgb(int x, int y, int* r, int* g, int* b) const;
    /** @brief 双线性取 RGB */
    bool sampleRgb(double x, double y, int* r, int* g, int* b) const;

    /**
     * @brief 存成常见图像文件。路径为 UTF-8
     * 无扩展名时按 format 补上。有扩展名时必须和 format 一致，jpg/jpeg、tif/tiff 视为同一种
     * @param jpegQuality 仅 Jpeg 使用，1 到 100
     */
    bool save(const std::string& utf8Path, ImageFileFormat format, int jpegQuality = 95) const;
};

}

#endif
