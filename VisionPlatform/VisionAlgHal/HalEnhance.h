/**
 * @file HalEnhance.h
 * @brief 图像增强流水（Halcon）
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_HALCONENHANCE_H
#define VISION_HALCONENHANCE_H

#include "AlgToolBase.h"
#include "TolShot.h"
#include "ToolParaEnhance.hpp"
#include "HalconCpp.h"
#include <vector>

namespace Vision {

	/**
	 * @class HalEnhance
	 * @brief 图像增强流水（Halcon）
	 * @note 单拍状态存在成员里，同一实例不可并发 execute
	 */
	class HalEnhance : public AlgToolBase
	{
	public:
		/** @brief 图像与步骤置空 */
		HalEnhance();
		/** @brief 析构 */
		~HalEnhance();

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

		/** @brief 按步骤做图像增强 */
		int execute(const AlgToolIn& in, AlgToolOut* out) override;

		/** @brief 打开时装入。增强无模型文件 */
		void init(const std::string& toolDataDir) override;
		/** @brief 保存。增强无模型文件 */
		void save(const std::string& toolDataDir) override;

	private:
		/** @brief 增强算子 */
		enum class EnhanceOp {
			EquHisto = 0,
			Emphasize,
			Illuminate,
			Scale,
			Invert
		};

		/** @brief 增强管线一步 */
		struct EnhanceStep {
			EnhanceOp op; ///< 算子
			int maskW; ///< 掩膜宽
			int maskH; ///< 掩膜高
			double coeff; ///< 系数 0
			double coeff2; ///< 系数 1
			bool scaleMax; ///< Scale 是否拉伸到 0~255
			/** @brief 缺省一步直方图均衡 */
			EnhanceStep();
		};

		/** @brief 清空本拍图像、步骤与错误码 */
		int tool_reset();
		/**
		 * @brief 取灰度图、ROI 与增强参数
		 * @return 无图返回 HS_ERR_IMAGE_EMPTY，参数类型不对返回 HS_ERR_TYPE
		 */
		int proc_input(const AlgToolIn& in);
		/** @brief 按步骤依次增强 */
		int alg_run(const AlgToolIn& in);
		/** @brief 预处理无测值，直接通过 */
		int judge_tol(const AlgToolIn& in) override;
		/** @brief 写出增强图 */
		int alg_rst_proc(const AlgToolIn& in, AlgToolOut* out);

		/** @brief 第 i 步参数键 */
		std::string step_key(int i, const char* suffix) const;
		/** @brief 整型转增强算子 */
		EnhanceOp enhance_op_from_int(int v) const;
		/** @brief 从运行参数解开增强步骤 */
		std::vector<EnhanceStep> steps_from_params(const ParamMap& p) const;
		/** @brief 增强一步。失败时写 err */
		bool apply_enhance_op(HalconCpp::HImage* img, const EnhanceStep& st, std::string* err) const;

		HalconCpp::HObject image_; ///< 本拍灰度图
		HalconCpp::HObject roi_region_; ///< 作用区域。空则整图
		HalconCpp::HObject result_image_; ///< 本拍增强结果
		std::vector<EnhanceStep> steps_; ///< 本拍增强步骤
		int img_w_; ///< 图像宽
		int img_h_; ///< 图像高
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
