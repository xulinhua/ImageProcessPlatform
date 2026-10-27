/**
 * @file ConvertBench.h
 * @brief ConvertBench头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef HSIMAGE_CONVERT_BENCH_H
#define HSIMAGE_CONVERT_BENCH_H

#include <QImage>
#include <QString>

#ifdef HSIMAGE_HAS_OPENCV
#include <opencv2/core.hpp>
#endif

/**
 * @brief 彩色像素排列差异下的转换实现（计时口径见 benchColorConvert）
 */
enum ColorConvertMethod {
    ColorMethodForLoop = 0,      ///< 交错→平面：Pointer + for
    ColorMethodInterleave = 1,   ///< GenImageInterleaved / InterleaveChannels+memcpy
    ColorMethodGenImage3 = 2,    ///< 拆平面 + GenImage3 / Decompose3 + 拼交错
    ColorMethodOpenCvMerge = 3   ///< 市面常见：cv::split+GenImage3 / Pointer3+cv::merge
};

/**
 * @class ConvertBenchRst
 * @brief 一次转换计时结果（接口段分开：buffer->H / H->buffer）
 */
struct ConvertBenchRst {
    bool ok;          ///< 是否成功
    QString error;    ///< 失败原因
    double msToH;     ///< buffer -> Halcon（毫秒）；未测为 -1
    double msFromH;   ///< Halcon -> buffer（毫秒）；未测为 -1
    double ms;        ///< 本次已测段合计（msToH + 有效的 msFromH）
    QString detail;   ///< 补充说明（尺寸、方式）
    QString stages;   ///< 分阶段耗时（方式2等）；空表示无
    QImage outImage;  ///< H->buffer 结果图；仅->H 时为空
    ConvertBenchRst()
        : ok(false)
        , msToH(-1.0)
        , msFromH(-1.0)
        , ms(0.0)
    {}
};

/** @brief 本进程是否链接 Halcon */
bool convertBenchHasHalcon();
/** @brief 本进程是否链接 OpenCV */
bool convertBenchHasOpenCv();

/**
 * @brief 生成约 500 万像素灰度图（默认 2448×2048）
 * @param width 宽
 * @param height 高
 */
QImage makeBenchGrayImage(int width = 2448, int height = 2048);
/**
 * @brief 生成约 500 万像素彩色图（RGB888 交错）
 * @param width 宽
 * @param height 高
 */
QImage makeBenchColorImage(int width = 2448, int height = 2048);

#ifdef HSIMAGE_HAS_OPENCV
/** @brief QImage 灰度/RGB → cv::Mat（彩图为 BGR） */
cv::Mat qImageToMat(const QImage& img);
/** @brief cv::Mat → QImage（BGR→RGB） */
QImage matToQImage(const cv::Mat& mat);
#endif

/**
 * @brief 灰度接口计时：分别统计 buffer->H 与 H->buffer（不含深拷所有权）
 * @param src 灰度图
 * @param toHalconOnly 真则只测 buffer->H
 */
ConvertBenchRst benchGrayRoundTrip(const QImage& src, bool toHalconOnly);

/**
 * @brief 彩色接口计时：分别统计 buffer->H 与 H->buffer（不含 CopyImage / QImage<->Mat）
 * @param src RGB888 或 RGB32 图
 * @param method 三种实现之一
 * @param toHalconOnly 真则只测 buffer->H
 */
ConvertBenchRst benchColorConvert(const QImage& src, ColorConvertMethod method, bool toHalconOnly);

#endif
