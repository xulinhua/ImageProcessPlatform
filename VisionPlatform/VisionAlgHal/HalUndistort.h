/**
 * @file HalUndistort.h
 * @brief HalUndistort头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_HALCON_UNDISTORT_H
#define VISION_HALCON_UNDISTORT_H

#include "AlgToolBase.h"

namespace Vision {

/**
 * @class HalUndistort
 * @brief 按 Halcon 相机参数文本做径向畸变校正（change_radial_distortion_image）
 */
class HalUndistort : public AlgToolBase {
public:
    /** @brief 挂到 preprocess.undistort 工具壳 */
    std::string toolName() const { return "preprocess.undistort"; }
    /** @brief 算法库：Halcon */
    AlgLibKind algLib() const { return AlgLibHal; }
    /** @brief 算法版本号 */
    std::string version() const;
    /** @brief 版本变更，只追加 */
    std::vector<VerNote> versionNotes() const;
    /** @brief 调 run 去畸变；未通过且没给错误码时记为 HS_ERR_OPERATOR */
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
    /** @brief 读 in.run 的 halconCam（首项模型名，其余为数值，空格分隔），校正到无畸变（kappa=0）相机 */
    AlgToolOut run(const AlgToolIn& in);
    /** @brief 克隆一份新实例 */
    AlgToolBase* clone() const { return new HalUndistort(); }
};

}

#endif
