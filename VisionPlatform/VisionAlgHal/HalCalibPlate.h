/**
 * @file HalCalibPlate.h
 * @brief HalCalibPlate头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_HALCON_CALIB_PLATE_H
#define VISION_HALCON_CALIB_PLATE_H

#include "AlgToolBase.h"

namespace Vision {

/**
 * @class HalCalibPlate
 * @brief 用 Halcon 标定板描述文件在离线图片里找标记，再 calibrate_cameras 求内参
 * @note 会在图片目录里生成 caltab.descr、caltab.ps
 */
class HalCalibPlate : public AlgToolBase {
public:
    /** @brief 挂到 calib.plate 工具壳 */
    std::string toolName() const { return "calib.plate"; }
    /** @brief 算法库：Halcon */
    AlgLibKind algLib() const { return AlgLibHal; }
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
     * @brief 读 in.run 的 rows/cols/spacingMm/minImages/imageDir 标定
     * @return values 写 fx/fy/cx/cy/k1~k3/p1/p2/rms/maxResidual/count/width/height，
     *         halconCam 为 Halcon 相机参数原文
     */
    AlgToolOut run(const AlgToolIn& in);
    /** @brief 克隆一份新实例 */
    AlgToolBase* clone() const { return new HalCalibPlate(); }
};

}

#endif
