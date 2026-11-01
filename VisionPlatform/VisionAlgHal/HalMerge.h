/**
 * @file HalMerge.h
 * @brief 合并区域并统计数量和面积（Halcon）
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_HALCONMERGE_H
#define VISION_HALCONMERGE_H

#include "AlgToolBase.h"
#include "TolShot.h"
#include "ToolParaRstMerge.hpp"
#include "HalconCpp.h"
#include <HsCommData/HsObject.h>
#include <string>
#include <vector>

namespace Vision {

/**
 * @class HalMerge
 * @brief 合并上游区域、轮廓与 ROI，统计 count、area 并判定
 * @note 单拍状态存在成员里，同一实例不可并发 execute
 */
class HalMerge : public AlgToolBase {
public:
	/** @brief 测值与几何置空 */
	HalMerge();

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

	/** @brief 合并上游几何并输出统计 */
	int execute(const AlgToolIn& in, AlgToolOut* out) override;
	/** @brief 打开时装入。合并无模型文件 */
	void init(const std::string& toolDataDir) override;
	/** @brief 保存。合并无模型文件 */
	void save(const std::string& toolDataDir) override;

private:
	/** @brief 清空本拍几何、测值与错误码 */
	int tool_reset();
	/**
	 * @brief 收上游几何与运行参数
	 * @return 参数类型不对返回 HS_ERR_TYPE
	 */
	int proc_input(const AlgToolIn& in);
	/** @brief 合并几何并统计面积与个数 */
	int alg_run(const AlgToolIn& in);
	/** @brief 按个数与面积做公差，结果留在 tol_ */
	int judge_tol(const AlgToolIn& in) override;
	/** @brief 写出合并几何与测值 */
	int alg_rst_proc(const AlgToolIn& in, AlgToolOut* out);

	/** @brief 收上游 regions、contours、非空 rois */
	void collect_upstream(const AlgToolIn& in);
	/** @brief 把区域 run 并成一只 HObject */
	bool union_regions(HalconCpp::HObject* out) const;
	/** @brief 统计面积：区域并集 + 非区域外接框 */
	void measure_area();

	std::vector<HsCommData::HsObject> merged_; ///< 输出几何
	double area_; ///< 合并面积
	int count_; ///< 几何个数
	double min_area_; ///< 运行参数下限
	int max_count_; ///< 运行参数上限，0 不限
	bool param_ok_; ///< minArea/maxCount 运行判定
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
