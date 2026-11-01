/**
 * @file HalFindCircle.h
 * @brief 亚像素边缘拟合圆（Halcon）
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_HALCONFINDCIRCLE_H
#define VISION_HALCONFINDCIRCLE_H

#include "AlgToolBase.h"
#include "TolShot.h"
#include "ToolParaFindCircle.hpp"
#include "HalconCpp.h"
#include <string>

namespace Vision {

	/**
	 * @class HalFindCircle
	 * @brief 在搜索区内提边缘并拟合圆（Halcon）
	 * @note 单拍状态存在成员里，同一实例不可并发 execute
	 */
	class HalFindCircle : public AlgToolBase
	{
	public:
		/** @brief 图像和区域置空 */
		HalFindCircle();
		/** @brief 析构 */
		~HalFindCircle();

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

		/** @brief 在搜索区内找圆 */
		int execute(const AlgToolIn& in, AlgToolOut* out) override;

	private:
		/** @brief 清空本拍结果 */
		int tool_reset();
		/**
		 * @brief 取灰度图并按口名收集搜索区；校验 ToolParaFindCircle
		 * @return 无图返回 HS_ERR_IMAGE_EMPTY，参数类型不对返回 HS_ERR_TYPE
		 */
		int proc_input(const AlgToolIn& in);
		/** @brief 提边缘并拟合圆，保留半径范围内最大的一个 */
		int alg_run(const AlgToolIn& in);
		/** @brief 圆心、半径和标准位姿键做公差。测值已在 tol_ */
		int judge_tol(const AlgToolIn& in) override;
		/** @brief 写出圆和测值 */
		int alg_rst_proc(const AlgToolIn& in, AlgToolOut* out);

		/**
		 * @brief 按口名把普通区并起来再减去屏蔽区
		 * @note 没有普通区时 region 为空
		 */
		void take_region(const AlgToolIn& in, const char* port, HalconCpp::HObject* region);

		HalconCpp::HObject image_; ///< 本拍灰度图
		HalconCpp::HObject search_region_; ///< 本拍搜索区
		bool found_; ///< 本拍是否找到圆
		double cx_; ///< 圆心列
		double cy_; ///< 圆心行
		double radius_; ///< 半径
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
