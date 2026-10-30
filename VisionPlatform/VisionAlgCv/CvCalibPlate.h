/**
 * @file CvCalibPlate.h
 * @brief CvCalibPlate头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_OPENCV_CALIB_PLATE_H
#define VISION_OPENCV_CALIB_PLATE_H

#include "AlgToolBase.h"

namespace Vision {

/**
 * @class CvCalibPlate
 * @brief 离线目录里找棋盘格或圆点板，再用 calibrateCamera 求内参和畸变
 */
class CvCalibPlate : public AlgToolBase {
public:
    /** @brief 挂到 calib.plate 工具壳 */
    std::string toolName() const { return "calib.plate"; }
    /** @brief 算法库：OpenCV */
    AlgLibKind algLib() const { return AlgLibCv; }
    /** @brief 算法版本号 */
    std::string version() const;
    /** @brief 版本变更，只追加 */
    std::vector<VerNote> versionNotes() const;
    /** @brief 调 run 求内参；未通过且没给错误码时记为 HS_ERR_OPERATOR */
    int execute(const AlgToolIn& in, AlgToolOut* out) override {
        if (!out) {
            return HS_ERR_NULL;
        }
        *out = run(in);
        if (out->ok) {
            out->errCode = HS_OK;
            return HS_OK;
        }
        if (out->errCode == HS_OK || out->errCode == 1) {
            out->errCode = HS_ERR_OPERATOR;
        }
        return out->errCode;
    }
    /**
     * @brief 读 in.run 的 rows/cols/spacingMm/plateKind/imageDir，逐张找板后标定
     * @return values 写 fx/fy/cx/cy/k1/k2/p1/p2/k3/rms/maxResidual/count/width/height；
     *         pointsOnly 非 0 时只取第一张找到板的图，写 points/width/height
     */
    AlgToolOut run(const AlgToolIn& in);
    /** @brief 克隆一份新实例 */
    AlgToolBase* clone() const { return new CvCalibPlate(); }
};

/**
 * @class CvCalibPlane
 * @brief 取一张能找到板的图，导出像素点与毫米点对，供 N 点标定
 */
class CvCalibPlane : public AlgToolBase {
public:
    /** @brief 挂到 calib.npoint 工具壳 */
    std::string toolName() const { return "calib.npoint"; }
    /** @brief 算法库：OpenCV */
    AlgLibKind algLib() const { return AlgLibCv; }
    /** @brief 算法版本号 */
    std::string version() const;
    /** @brief 版本变更，只追加 */
    std::vector<VerNote> versionNotes() const;
    /** @brief 调 run 导出点对；未通过且没给错误码时记为 HS_ERR_OPERATOR */
    int execute(const AlgToolIn& in, AlgToolOut* out) override {
        if (!out) {
            return HS_ERR_NULL;
        }
        *out = run(in);
        if (out->ok) {
            out->errCode = HS_OK;
            return HS_OK;
        }
        if (out->errCode == HS_OK || out->errCode == 1) {
            out->errCode = HS_ERR_OPERATOR;
        }
        return out->errCode;
    }
    /** @brief 强制 pointsOnly=1、minImages=1 后转给 CvCalibPlate::run */
    AlgToolOut run(const AlgToolIn& in);
    /** @brief 克隆一份新实例 */
    AlgToolBase* clone() const { return new CvCalibPlane(); }
};

}

#endif
