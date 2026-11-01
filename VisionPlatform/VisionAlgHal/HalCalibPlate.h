/**
 * @file HalCalibPlate.h
 * @brief 用 Halcon 标定板求相机内参
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_HALCON_CALIB_PLATE_H
#define VISION_HALCON_CALIB_PLATE_H

#include "AlgToolBase.h"
#include "TolShot.h"
#include "ToolParaCalibPlate.hpp"
#include "HalconCpp.h"
#include <string>
#include <vector>

namespace Vision {

/**
 * @class HalCalibPlate
 * @brief 在离线目录的标定板图上 find_calib_object + calibrate_cameras
 * @note 会在 imageDir 下生成 caltab.descr、caltab.ps；init/save 无内存模型
 */
class HalCalibPlate : public AlgToolBase {
public:
	/** @brief 路径与参数置空 */
	HalCalibPlate();

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

	/** @brief 在标定图目录上求内参 */
	int execute(const AlgToolIn& in, AlgToolOut* out) override;
	/** @brief 打开时装入。无内存模型 */
	void init(const std::string& toolDataDir) override;
	/** @brief 保存。无内存模型 */
	void save(const std::string& toolDataDir) override;

private:
	/** @brief 清空本拍路径、参数与错误码 */
	int tool_reset();
	/**
	 * @brief 取标定图目录与板参数
	 * @return 参数类型不对返回 HS_ERR_TYPE
	 */
	int proc_input(const AlgToolIn& in);
	/** @brief 生成描述文件、找标定对象并标定 */
	int alg_run(const AlgToolIn& in);
	/** @brief 标定误差等测值做公差，结果留在 tol_ */
	int judge_tol(const AlgToolIn& in) override;
	/** @brief 写出内参文本与测值 */
	int alg_rst_proc(const AlgToolIn& in, AlgToolOut* out);

	/** @brief 目录下枚举常见图像后缀 */
	std::vector<std::string> list_images(const std::string& dir) const;
	/** @brief 宽文件名是否为图像 */
	bool is_image_name(const std::wstring& name) const;
	/** @brief Halcon 参数 tuple 转成空格分隔文本 */
	std::string tuple_text(const HalconCpp::HTuple& t) const;

	std::vector<std::string> image_files_; ///< 本拍使用的标定图路径
	std::string image_dir_; ///< 标定图目录
	int rows_; ///< 标定板行数
	int cols_; ///< 标定板列数
	double spacing_mm_; ///< 圆/格点间距（毫米）
	int min_images_; ///< 最少可用标定图数
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
