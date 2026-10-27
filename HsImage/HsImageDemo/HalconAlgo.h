/**
 * @file HalconAlgo.h
 * @brief HalconAlgo头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef HSIMAGE_DEMO_HALCONALGO_H
#define HSIMAGE_DEMO_HALCONALGO_H

#include <HsCommData/HsCommData.h>

#include <QImage>
#include <QString>

/**
 * @class ThresholdRst
 * @brief 灰度阈值一次运行的结果与耗时。彩色图先 toLuma8，与 HUD Gray 一致
 */
struct ThresholdRst {
    HsCommData::HsRegion rle; ///< 前景行程
    bool ok;                ///< 是否成功
    bool usedHalcon;        ///< true 表示走 Halcon，否则本地扫描
    QString error;          ///< 失败原因
    qint64 thresholdUs;     ///< 阈值本身耗时（微秒）
    qint64 convertUs;       ///< HsCommData::HsRegion → HsCommData::HsRegion 转换耗时（微秒）
};

/**
 * @brief 准备阈值输入图（BT.601 灰度，可选生成 Halcon HImage）
 * @param src 原图
 * @param error 失败时写入原因
 */
bool prepareThresholdImage(const QImage& src, QString* error);
/**
 * @brief 对已准备的灰度图做阈值
 * @param tMin 下限 0–255
 * @param tMax 上限 0–255
 */
ThresholdRst runGrayThreshold(int tMin, int tMax);
/** @brief 本进程是否链接了 Halcon */
bool hasHalconEngine();
/** @brief 引擎显示名（Halcon 版本或本地扫描） */
QString halconEngineName();

#endif
