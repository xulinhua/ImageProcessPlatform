/**
 * @file CvUndistort.h
 * @brief 按内参和畸变系数去畸变（OpenCV）
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_OPENCV_UNDISTORT_H
#define VISION_OPENCV_UNDISTORT_H

#include "AlgToolBase.h"
#include "ToolParaUndistort.hpp"
#include "opencvLib.h"
#include <string>
#include <vector>

namespace Vision {

/**
 * @class CvUndistort
 * @brief 按内参和畸变系数去畸变：initUndistortRectifyMap 加 remap
 * @note 映射表按「模型 stamp + 图像尺寸」缓存在成员里；单拍状态不可并发 execute
 */
class CvUndistort : public AlgToolBase {
public: // 骨架
	/** @brief 构造 */
	CvUndistort();
	/** @brief 析构 */
	~CvUndistort();

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

public: // 执行、持久化
	/** @brief 去畸变 */
	int execute(const AlgToolIn& in, AlgToolOut* out) override;
	/** @brief 打开时无需模型文件 */
	void init(const std::string& toolDataDir) override;
	/** @brief 保存时无需模型文件 */
	void save(const std::string& toolDataDir) override;

private: // 单拍流程
	/** @brief 清空本拍输出图与错误码 */
	int tool_reset();
	/**
	 * @brief 取输入图与内参畸变参数
	 * @return 无图返回 HS_ERR_IMAGE_EMPTY，参数不是 ToolParaUndistort 返回 HS_ERR_TYPE
	 */
	int proc_input(const AlgToolIn& in);
	/** @brief 建或复用映射表并 remap */
	int alg_run(const AlgToolIn& in);
	/** @brief 去畸变无测值，直接通过 */
	int judge_tol(const AlgToolIn& in) override;
	/** @brief 写出校正图 */
	int alg_rst_proc(const AlgToolIn& in, AlgToolOut* out);

	/** @brief 按 stamp 与尺寸确保映射表可用 */
	void ensure_maps(const cv::Size& size, const cv::Mat& K, const cv::Mat& dist, const std::string& stamp);

private:
	cv::Mat src_; ///< 本拍输入图（可彩色）
	cv::Mat dst_; ///< 本拍校正图
	cv::Mat map1_; ///< 映射表 1
	cv::Mat map2_; ///< 映射表 2
	std::string map_key_; ///< 当前映射表键
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
