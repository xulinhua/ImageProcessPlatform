/**
 * @file CvBlob.h
 * @brief 绝对、相对、动态阈值 Blob（OpenCV）
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_OPENCVBLOB_H
#define VISION_OPENCVBLOB_H

#include "AlgToolBase.h"
#include "TolShot.h"
#include "ToolParaBlob.hpp"
#include "opencvLib.h"
#include <string>
#include <vector>

namespace Vision {

/**
 * @class CvBlob
 * @brief 绝对、相对、动态阈值 Blob（OpenCV）
 * @note 单拍状态存在成员里，同一实例不可并发 execute
 */
class CvBlob : public AlgToolBase {
public: // 骨架
	/** @brief 构造。图像与掩膜置空 */
	CvBlob();
	/** @brief 析构 */
	~CvBlob();

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
	/** @brief 在搜索域里做阈值分割并输出连通域 */
	int execute(const AlgToolIn& in, AlgToolOut* out) override;
	/** @brief 打开时装入。斑点没有模型文件 */
	void init(const std::string& toolDataDir) override;
	/** @brief 保存。斑点没有模型文件 */
	void save(const std::string& toolDataDir) override;

private: // 单拍流程
	/** @brief 清空本拍测值、区域与错误码 */
	int tool_reset();
	/**
	 * @brief 取灰度图、搜索掩膜与参数对象
	 * @return 无图返回 HS_ERR_IMAGE_EMPTY，参数不是 ToolParaBlob 返回 HS_ERR_TYPE
	 */
	int proc_input(const AlgToolIn& in);
	/** @brief 阈值、形态学、连通域筛选 */
	int alg_run(const AlgToolIn& in);
	/** @brief 按数量与总面积做公差，结果留在 tol_ */
	int judge_tol(const AlgToolIn& in) override;
	/** @brief 把连通域、测值和公差写进 out */
	int alg_rst_proc(const AlgToolIn& in, AlgToolOut* out);

	/** @brief 按一步阈值参数二值化，并与搜索掩膜求交 */
	int threshold_bin(const ToolParaBlob::Step& th, cv::Mat* bin) const;
	/** @brief 开闭运算。半径 ≤0.4 跳过 */
	void morph_open_close(const cv::Mat& bin, double openR, double closeR, cv::Mat* out) const;
	/** @brief 连通、按面积/圆度筛选，记下个数与总面积，选中掩膜写入 result_mask_ */
	int pack_blobs(const cv::Mat& bin, double minArea, double maxArea, double minCirc, double maxCirc);

private:
	cv::Mat image_; ///< 本拍灰度图
	cv::Mat search_mask_; ///< 搜索掩膜。空则整图
	cv::Mat result_mask_; ///< 本拍选中的连通域掩膜
	int img_w_; ///< 图像宽
	int img_h_; ///< 图像高
	int blob_count_; ///< 连通域个数
	double area_sum_; ///< 面积和
	TolShot tol_; ///< 本拍测值和公差。写出前不进 AlgToolOut
	int shot_err_; ///< 本拍错误码。HS_OK 为成功
	std::string shot_msg_; ///< 本拍说明

	/** @brief 转到 tool_name：detect.blob */
	std::string toolName() const override { return tool_name(); }
	/** @brief 转到 alg_lib：OpenCV */
	AlgLibKind algLib() const override { return alg_lib(); }
	/** @brief 转到 version_notes */
	std::vector<VerNote> versionNotes() const override { return version_notes(); }
};

}

#endif
