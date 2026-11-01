/**
 * @file HalCaliper.h
 * @brief 沿卡尺采样找边（Halcon）
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_HALCONCALIPER_H
#define VISION_HALCONCALIPER_H

#include "AlgToolBase.h"
#include "TolShot.h"
#include "ToolParaCaliper.hpp"
#include "HalconCpp.h"
#include <string>
#include <vector>

namespace Vision {

	/**
	 * @class HalCaliper
	 * @brief 沿卡尺采样找边或测宽（Halcon）
	 * @note 单拍状态存在成员里，同一实例不可并发 execute
	 */
	class HalCaliper : public AlgToolBase
	{
	public:
		/** @brief 图像置空 */
		HalCaliper();
		/** @brief 析构 */
		~HalCaliper();

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

		/** @brief 在卡尺 ROI 上采样找边或测宽 */
		int execute(const AlgToolIn& in, AlgToolOut* out) override;

	private:
		/** @brief 清空本拍结果 */
		int tool_reset();
		/**
		 * @brief 取灰度图并校验 ToolParaCaliper
		 * @return 无图返回 HS_ERR_IMAGE_EMPTY，参数类型不对返回 HS_ERR_TYPE
		 */
		int proc_input(const AlgToolIn& in);
		/** @brief 在各卡尺 ROI 上测量 */
		int alg_run(const AlgToolIn& in);
		/** @brief 个数、宽度和标准位姿键做公差。测值已在 tol_ */
		int judge_tol(const AlgToolIn& in) override;
		/** @brief 写出几何和测值，返回本拍错误码 */
		int alg_rst_proc(const AlgToolIn& in, AlgToolOut* out);

		/** @brief 卡尺 ROI 转 Halcon 测量矩形参数 */
		bool caliper_rect(const HsCommData::HsObject& roi, double* row, double* col, double* phi,
			double* l1, double* l2) const;

		HalconCpp::HObject image_; ///< 本拍灰度图
		std::vector<HsCommData::HsObject> objects_; ///< 本拍输出几何（十字或边对连线）
		TolShot tol_; ///< 本拍测值和公差。写出前不进 AlgToolOut
		int img_w_; ///< 图像宽
		int img_h_; ///< 图像高
		int found_; ///< 本拍测到的结果总数
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
