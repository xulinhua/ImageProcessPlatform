/**
 * @file HalFindLine.h
 * @brief 亚像素边缘拟合直线，支持搜索区（Halcon）
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_HALCONFINDLINE_H
#define VISION_HALCONFINDLINE_H

#include "AlgToolBase.h"
#include "TolShot.h"
#include "ToolParaFindLine.hpp"
#include "HalconCpp.h"
#include <string>
#include <vector>

namespace Vision {

	/**
	 * @class HalFindLine
	 * @brief 在搜索区内提边缘、拟合直线（Halcon）
	 * @note 单拍状态存在成员里，同一实例不可并发 execute
	 */
	class HalFindLine : public AlgToolBase
	{
	public:
		/** @brief 图像和区域置空 */
		HalFindLine();
		/** @brief 析构 */
		~HalFindLine();

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

		/** @brief 在搜索区上拟合直线 */
		int execute(const AlgToolIn& in, AlgToolOut* out) override;

	private:
		/** @brief 清空本拍结果 */
		int tool_reset();
		/**
		 * @brief 取灰度图并按口名收集搜索区；校验 ToolParaFindLine
		 * @return 无图返回 HS_ERR_IMAGE_EMPTY，参数类型不对返回 HS_ERR_TYPE
		 */
		int proc_input(const AlgToolIn& in);
		/** @brief 提边缘并拟合直线 */
		int alg_run(const AlgToolIn& in);
		/** @brief 个数、中点、角度和长度做公差。结果留在 tol_ */
		int judge_tol(const AlgToolIn& in) override;
		/** @brief 写出直线、区域和测值 */
		int alg_rst_proc(const AlgToolIn& in, AlgToolOut* out);

		/**
		 * @brief 按口名把普通区并起来再减去屏蔽区
		 * @note search 口同时收口名为空或 roi 的区域；没有普通区时 region 为空
		 */
		void take_region(const AlgToolIn& in, const char* port, HalconCpp::HObject* region);
		/** @brief 按线段长度从长到短排序 */
		static bool longer_line(const HsCommData::LineP& a, const HsCommData::LineP& b);

		HalconCpp::HObject image_; ///< 本拍灰度图
		HalconCpp::HObject search_region_; ///< 本拍搜索区
		std::vector<HsCommData::LineP> lines_; ///< 本拍直线
		TolShot tol_; ///< 本拍测值和公差。写出前不进 AlgToolOut
		int img_w_; ///< 图像宽
		int img_h_; ///< 图像高
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
