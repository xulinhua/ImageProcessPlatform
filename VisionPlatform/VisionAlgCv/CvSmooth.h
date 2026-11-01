/**
 * @file CvSmooth.h
 * @brief 图像平滑流水（OpenCV）
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_OPENCVSMOOTH_H
#define VISION_OPENCVSMOOTH_H

#include "AlgToolBase.h"
#include "ToolParaSmooth.hpp"
#include "opencvLib.h"
#include <string>
#include <vector>

namespace Vision {

/**
 * @class CvSmooth
 * @brief 图像平滑流水（OpenCV）
 * @note 单拍状态存在成员里，同一实例不可并发 execute
 */
class CvSmooth : public AlgToolBase {
public: // 骨架
	/** @brief 构造 */
	CvSmooth();
	/** @brief 析构 */
	~CvSmooth();

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
	/** @brief 按步骤做图像平滑 */
	int execute(const AlgToolIn& in, AlgToolOut* out) override;

private: // 单拍流程
	/** @brief 清空本拍工作图与错误码 */
	int tool_reset();
	/**
	 * @brief 取灰度图、搜索掩膜与平滑参数
	 * @return 无图返回 HS_ERR_IMAGE_EMPTY，参数不是 ToolParaSmooth 返回 HS_ERR_TYPE
	 */
	int proc_input(const AlgToolIn& in);
	/** @brief 按步骤依次平滑工作图 */
	int alg_run(const AlgToolIn& in);
	/** @brief 预处理无测值，直接通过 */
	int judge_tol(const AlgToolIn& in) override;
	/** @brief 仅搜索区内像素写回输出图 */
	int alg_rst_proc(const AlgToolIn& in, AlgToolOut* out);

	/** @brief 平滑算子 */
	enum class SmoothOp {
		Mean = 0,
		Median,
		Gauss,
		Bilateral
	};

	/** @brief 平滑管线一步 */
	struct SmoothStep {
		SmoothOp op; ///< 算子
		int maskW; ///< 掩膜宽或核半径相关
		int maskH; ///< 掩膜高
		double sigmaRange; ///< 双边滤波值域 sigma
		double sigmaSpace; ///< 双边滤波空间 sigma
		/** @brief 缺省一步中值滤波 */
		SmoothStep()
			: op(SmoothOp::Median)
			, maskW(3)
			, maskH(3)
			, sigmaRange(20.0)
			, sigmaSpace(3.0)
		{}
	};

	/** @brief 第 i 步参数键 */
	std::string step_key(int i, const char* suffix) const;
	/** @brief 整型转平滑算子 */
	SmoothOp smooth_op_from_int(int v) const;
	/** @brief 从运行参数解开平滑步骤。无步骤时返回空 */
	std::vector<SmoothStep> smooth_steps_from_params(const ParamMap& p) const;
	/** @brief 平滑一步。失败时写 err */
	bool apply_smooth_op(cv::Mat* img, const SmoothStep& st, std::string* err) const;

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
