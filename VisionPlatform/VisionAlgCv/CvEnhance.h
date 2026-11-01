/**
 * @file CvEnhance.h
 * @brief 图像增强流水（OpenCV）
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_OPENCVENHANCE_H
#define VISION_OPENCVENHANCE_H

#include "AlgToolBase.h"
#include "ToolParaEnhance.hpp"
#include "opencvLib.h"
#include <string>
#include <vector>

namespace Vision {

/**
 * @class CvEnhance
 * @brief 图像增强流水（OpenCV）
 * @note 单拍状态存在成员里，同一实例不可并发 execute
 */
class CvEnhance : public AlgToolBase {
public: // 骨架
	/** @brief 构造 */
	CvEnhance();
	/** @brief 析构 */
	~CvEnhance();

	/** @brief 克隆本实例 */
	AlgToolBase* clone() const override;

public: // 描述、版本
	/** @brief 注册名 */
	std::string tool_name() const override;
	/** @brief 算法库 */
	AlgLibKind alg_lib() const override;
	/** @brief 工具版本 */
	std::string version() const override;
	/** @brief 版本变更，只追加 */
	std::vector<VerNote> version_notes() const override;

public: // 执行
	/** @brief 按步骤做图像增强 */
	int execute(const AlgToolIn& in, AlgToolOut* out) override;

private: // 单拍流程
	/** @brief 清空本拍工作图与错误码 */
	int tool_reset();
	/**
	 * @brief 取灰度图、搜索掩膜与增强参数
	 * @return 无图返回 HS_ERR_IMAGE_EMPTY，参数不是 ToolParaEnhance 返回 HS_ERR_TYPE
	 */
	int proc_input(const AlgToolIn& in);
	/** @brief 按步骤依次增强工作图 */
	int alg_run(const AlgToolIn& in);
	/** @brief 预处理无测值，直接通过 */
	int judge_tol(const AlgToolIn& in) override;
	/** @brief 仅搜索区内像素写回输出图 */
	int alg_rst_proc(const AlgToolIn& in, AlgToolOut* out);

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
		EnhanceStep()
			: op(EnhanceOp::EquHisto)
			, maskW(7)
			, maskH(7)
			, coeff(1.0)
			, coeff2(0.0)
			, scaleMax(false)
		{}
	};

	/** @brief 第 i 步参数键 */
	std::string step_key(int i, const char* suffix) const;
	/** @brief 整型转增强算子 */
	EnhanceOp enhance_op_from_int(int v) const;
	/** @brief 从运行参数解开增强步骤。无步骤时返回空 */
	std::vector<EnhanceStep> enhance_steps_from_params(const ParamMap& p) const;
	/** @brief 增强一步。失败时写 err */
	bool apply_enhance_op(cv::Mat* img, const EnhanceStep& st, std::string* err) const;

private:
	cv::Mat image_; ///< 本拍灰度原图
	cv::Mat work_; ///< 本拍工作图
	cv::Mat roi_mask_; ///< 搜索掩膜
	int shot_err_; ///< 本拍错误码
	std::string shot_msg_; ///< 本拍说明

	/** @brief 转到 tool_name */
	std::string toolName() const override { return tool_name(); }
	/** @brief 转到 alg_lib */
	AlgLibKind algLib() const override { return alg_lib(); }
	/** @brief 转到 version_notes */
	std::vector<VerNote> versionNotes() const override { return version_notes(); }
};

}

#endif
