/**
 * @file HalUndistort.h
 * @brief 按 Halcon 相机参数做径向畸变校正
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_HALCON_UNDISTORT_H
#define VISION_HALCON_UNDISTORT_H

#include "AlgToolBase.h"
#include "TolShot.h"
#include "ToolParaUndistort.hpp"
#include "HalconCpp.h"
#include <cstdint>
#include <string>

namespace Vision {

/**
 * @class HalUndistort
 * @brief 按 Halcon 相机参数文本做径向畸变校正（change_radial_distortion_image）
 * @note 相机参数由工具层写入 run.halconCam；init/save 无持久模型
 */
class HalUndistort : public AlgToolBase {
public:
	/** @brief 图像与参数置空 */
	HalUndistort();

	/** @brief 克隆本实例 */
	AlgToolBase* clone() const override;

	/** @brief 注册名 */
	std::string tool_name() const override;
	/** @brief 算法库 */
	AlgLibKind alg_lib() const override;
	/** @brief 工具版本 */
	std::string version() const override;
	/** @brief 版本变更，只追加 */
	std::vector<VerNote> version_notes() const override;

	/** @brief 按相机参数做畸变校正 */
	int execute(const AlgToolIn& in, AlgToolOut* out) override;
	/** @brief 打开时装入。无持久模型 */
	void init(const std::string& toolDataDir) override;
	/** @brief 保存。无持久模型 */
	void save(const std::string& toolDataDir) override;

private:
	/** @brief 清空本拍图像、参数与错误码 */
	int tool_reset();
	/**
	 * @brief 取灰度图与相机参数文本
	 * @return 无图返回 HS_ERR_IMAGE_EMPTY，参数类型不对返回 HS_ERR_TYPE
	 */
	int proc_input(const AlgToolIn& in);
	/** @brief 解析相机参数并校正 */
	int alg_run(const AlgToolIn& in);
	/** @brief 预处理类无专用公差域，仍按 judge 表透传 */
	int judge_tol(const AlgToolIn& in) override;
	/** @brief 写出校正图 */
	int alg_rst_proc(const AlgToolIn& in, AlgToolOut* out);

	/** @brief 解析 Halcon 相机参数文本：首 token 为模型名，其余为数值 */
	HalconCpp::HTuple parse_cam_par(const std::string& text) const;

	HalconCpp::HObject image_; ///< 本拍输入灰度图
	HalconCpp::HObject fixed_; ///< 校正结果
	int64_t frame_id_; ///< 输出 frameId
	std::string cam_text_; ///< 本拍相机参数原文
	TolShot tol_; ///< 本拍测值和公差。写出前不进 AlgToolOut
	int shot_err_; ///< 本拍错误码。0 为成功
	std::string shot_msg_; ///< 本拍说明

	/** @brief 转到 tool_name */
	std::string toolName() const override { return tool_name(); }
	/** @brief 转到 alg_lib：Halcon */
	AlgLibKind algLib() const override { return alg_lib(); }
	/** @brief 转到 version_notes */
	std::vector<VerNote> versionNotes() const override { return version_notes(); }
};

}

#endif
