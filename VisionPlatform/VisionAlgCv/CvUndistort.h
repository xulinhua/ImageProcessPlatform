/**
 * @file CvUndistort.h
 * @brief CvUndistort头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_OPENCV_UNDISTORT_H
#define VISION_OPENCV_UNDISTORT_H

#include "AlgToolBase.h"

namespace Vision {

/**
 * @class CvUndistort
 * @brief 按内参和畸变系数去畸变：initUndistortRectifyMap 加 remap
 * @note 映射表按「模型 stamp + 图像尺寸」缓存在进程级表里，加锁访问，不会淘汰
 */
class CvUndistort : public AlgToolBase {
public:
    /** @brief 挂到 preprocess.undistort 工具壳 */
    std::string toolName() const { return "preprocess.undistort"; }
    /** @brief 算法库：OpenCV */
    AlgLibKind algLib() const { return AlgLibCv; }
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
    /** @brief 读 in.run 的 fx/fy/cx/cy/k1/k2/p1/p2/k3/stamp，输出与输入同尺寸的校正图 */
    AlgToolOut run(const AlgToolIn& in);
    /** @brief 克隆一份新实例 */
    AlgToolBase* clone() const { return new CvUndistort(); }
};

}

#endif
