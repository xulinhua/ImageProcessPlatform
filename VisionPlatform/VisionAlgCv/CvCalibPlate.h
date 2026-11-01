/**
 * @file CvCalibPlate.h
 * @brief 离线目录标定板求内参，或导出点对（OpenCV）
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_OPENCV_CALIB_PLATE_H
#define VISION_OPENCV_CALIB_PLATE_H

#include "AlgToolBase.h"
#include "TolShot.h"
#include "ToolParaCalibPlate.hpp"
#include "opencvLib.h"
#include <string>
#include <vector>

namespace Vision {

/**
 * @class CvCalibPlate
 * @brief 离线目录里找棋盘格或圆点板，再用 calibrateCamera 求内参和畸变
 * @note 单拍状态存在成员里，同一实例不可并发 execute
 */
class CvCalibPlate : public AlgToolBase {
public: // 骨架
	/** @brief 构造 */
	CvCalibPlate();
	/** @brief 析构 */
	~CvCalibPlate();

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
	/** @brief 求内参，或只取点对 */
	int execute(const AlgToolIn& in, AlgToolOut* out) override;
	/** @brief 打开时无需模型文件 */
	void init(const std::string& toolDataDir) override;
	/** @brief 保存时无需模型文件 */
	void save(const std::string& toolDataDir) override;

private: // 单拍流程
	/** @brief 清空本拍测值与错误码 */
	int tool_reset();
	/**
	 * @brief 读板规格与离线目录
	 * @return 规格非法返回 HS_ERR_PARAM，参数不是 ToolParaCalibPlate 返回 HS_ERR_TYPE
	 */
	int proc_input(const AlgToolIn& in);
	/** @brief 逐张找板后标定，或 pointsOnly 导出点对 */
	int alg_run(const AlgToolIn& in);
	/** @brief 按 rms/maxResidual 等可测值做公差 */
	int judge_tol(const AlgToolIn& in) override;
	/** @brief 把内参或点对写进 out */
	int alg_rst_proc(const AlgToolIn& in, AlgToolOut* out);

	/** @brief 文件名是否像图片 */
	bool is_image_name(const std::wstring& name) const;
	/** @brief 列出目录下图片路径 */
	std::vector<std::string> list_images(const std::string& dir) const;
	/** @brief 生成板平面物点网格（毫米） */
	void object_grid(int rows, int cols, double spacing, std::vector<cv::Point3f>* obj) const;
	/** @brief 在灰度图上找板角点 */
	bool find_board(const cv::Mat& gray, int rows, int cols, int plateKind, std::vector<cv::Point2f>* corners) const;
	/** @brief 像素点与毫米点打成文本 */
	std::string pack_points(const std::vector<cv::Point2f>& img, const std::vector<cv::Point3f>& obj) const;
	/** @brief 把 K、畸变和残差写入 tol_ */
	void put_model(const cv::Mat& K, const cv::Mat& dist, double rms, double maxRes, int count, int w, int h);

private:
	int rows_; ///< 板行数
	int cols_; ///< 板列数
	double spacing_; ///< 点间距，毫米
	int plate_kind_; ///< 板类型。0 棋盘，1/2 圆点
	int min_images_; ///< 最少有效图数
	bool points_only_; ///< 只导出点对
	std::string image_dir_; ///< 离线图片目录
	std::string points_text_; ///< pointsOnly 时的点对文本
	int found_w_; ///< 找到板时的图宽
	int found_h_; ///< 找到板时的图高
	int found_count_; ///< 找到板的图数
	TolShot tol_; ///< 本拍测值与公差
	int shot_err_; ///< 本拍错误码
	std::string shot_msg_; ///< 本拍说明

	/** @brief 转到 tool_name */
	std::string toolName() const override { return tool_name(); }
	/** @brief 转到 alg_lib */
	AlgLibKind algLib() const override { return alg_lib(); }
	/** @brief 转到 version_notes */
	std::vector<VerNote> versionNotes() const override { return version_notes(); }
};

/**
 * @class CvCalibPlane
 * @brief 取一张能找到板的图，导出像素点与毫米点对，供 N 点标定
 */
class CvCalibPlane : public AlgToolBase {
public: // 骨架
	/** @brief 构造 */
	CvCalibPlane();
	/** @brief 析构 */
	~CvCalibPlane();

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
	/** @brief 强制 pointsOnly 后转给内部 CvCalibPlate 编排 */
	int execute(const AlgToolIn& in, AlgToolOut* out) override;

private: // 单拍流程
	/** @brief 清空本拍错误码 */
	int tool_reset();
	/** @brief 检查参数类型 */
	int proc_input(const AlgToolIn& in);
	/** @brief 调 plate_ 跑 pointsOnly */
	int alg_run(const AlgToolIn& in);
	/** @brief 无额外公差 */
	int judge_tol(const AlgToolIn& in) override;
	/** @brief 写出 plate_ 结果 */
	int alg_rst_proc(const AlgToolIn& in, AlgToolOut* out);

private:
	CvCalibPlate plate_; ///< 复用标定板逻辑
	AlgToolOut plate_out_; ///< plate_ 本拍输出
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
