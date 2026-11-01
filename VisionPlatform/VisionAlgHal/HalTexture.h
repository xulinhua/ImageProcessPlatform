/**
 * @file HalTexture.h
 * @brief 大核背景差纹理缺陷（Halcon）
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_HALCONTEXTURE_H
#define VISION_HALCONTEXTURE_H

#include "AlgToolBase.h"
#include "TolShot.h"
#include "ToolParaTexture.hpp"
#include "HalconCpp.h"

namespace Vision {

	/**
	 * @class HalTexture
	 * @brief 大核背景差找纹理缺陷（Halcon）
	 * @note 单拍状态存在成员里，同一实例不可并发 execute
	 */
	class HalTexture : public AlgToolBase
	{
	public:
		/** @brief 图像与区域置空 */
		HalTexture();
		/** @brief 析构 */
		~HalTexture();

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

		/** @brief 在 ROI 内做背景差并输出缺陷区域 */
		int execute(const AlgToolIn& in, AlgToolOut* out) override;

		/** @brief 打开时装入。纹理无模型文件 */
		void init(const std::string& toolDataDir) override;
		/** @brief 保存。纹理无模型文件 */
		void save(const std::string& toolDataDir) override;

	private:
		/** @brief 清空本拍区域、测值与错误码 */
		int tool_reset();
		/**
		 * @brief 取灰度图并收集 ROI；校验 ToolParaTexture
		 * @return 无图返回 HS_ERR_IMAGE_EMPTY，参数类型不对返回 HS_ERR_TYPE
		 */
		int proc_input(const AlgToolIn& in);
		/** @brief 大核平滑背景差、阈值与连通域 */
		int alg_run(const AlgToolIn& in);
		/** @brief 按个数与面积做公差，结果留在 tol_ */
		int judge_tol(const AlgToolIn& in) override;
		/** @brief 写出缺陷区域与测值 */
		int alg_rst_proc(const AlgToolIn& in, AlgToolOut* out);

		/**
		 * @brief 按口名把普通区并起来再减去屏蔽区
		 * @note 没有普通区时 region 为空
		 */
		void take_region(const AlgToolIn& in, const char* port, HalconCpp::HObject* region);

		HalconCpp::HObject image_; ///< 本拍灰度图
		HalconCpp::HObject roi_region_; ///< 检测区域
		HalconCpp::HObject result_region_; ///< 本拍缺陷连通域
		int img_w_; ///< 图像宽
		int img_h_; ///< 图像高
		int blob_count_; ///< 缺陷个数
		double blob_area_; ///< 缺陷面积和
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
