/**
 * @file CvFindCircle.h
 * @brief 用 Canny 拟合圆（OpenCV）
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_OPENCVFINDCIRCLE_H
#define VISION_OPENCVFINDCIRCLE_H

#include "AlgToolBase.h"
#include "TolShot.h"
#include "ToolParaFindCircle.hpp"
#include "opencvLib.h"
#include <string>
#include <vector>

namespace Vision {

/**
 * @class CvFindCircle
 * @brief 用 Canny 拟合圆（OpenCV）
 * @note 单拍状态存在成员里，同一实例不可并发 execute
 */
class CvFindCircle : public AlgToolBase {
public: // 骨架
	/** @brief 构造 */
	CvFindCircle();
	/** @brief 析构 */
	~CvFindCircle();

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
	/** @brief 搜索区内 Canny 找圆 */
	int execute(const AlgToolIn& in, AlgToolOut* out) override;

private: // 单拍流程
	/** @brief 清空本拍圆结果与错误码 */
	int tool_reset();
	/**
	 * @brief 取灰度图、搜索掩膜与找圆参数
	 * @return 无图返回 HS_ERR_IMAGE_EMPTY，参数不是 ToolParaFindCircle 返回 HS_ERR_TYPE
	 */
	int proc_input(const AlgToolIn& in);
	/** @brief Canny 轮廓、圆度筛选，取半径最大的外接圆 */
	int alg_run(const AlgToolIn& in);
	/** @brief 圆心与半径做公差，结果留在 tol_ */
	int judge_tol(const AlgToolIn& in) override;
	/** @brief 把圆、测值和公差写进 out */
	int alg_rst_proc(const AlgToolIn& in, AlgToolOut* out);

private:
	cv::Mat image_; ///< 本拍灰度图
	cv::Mat search_mask_; ///< 搜索掩膜
	int img_w_; ///< 图像宽
	int img_h_; ///< 图像高
	bool found_; ///< 本拍是否找到圆
	double cx_; ///< 圆心列
	double cy_; ///< 圆心行
	double radius_; ///< 半径
	TolShot tol_; ///< 本拍测值和公差
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
