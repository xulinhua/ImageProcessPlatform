/**
 * @file PclTmpl.h
 * @brief 点云模板 ICP 定位（PCL）
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_PCL_TMPL_H
#define VISION_PCL_TMPL_H

#include "AlgToolBase.h"
#include "TolShot.h"
#include "ToolParaLocate3dTmpl.hpp"
#include "PclCloudOps.h"
#include <HsCommData/HsCloud.h>
#include <string>
#include <vector>

namespace Vision {

/**
 * @class PclTmpl
 * @brief 点云模板 ICP。正式模型在 toolDataDir/pcl；示教只建临时，save 才转正落盘
 * @note 单拍状态存在成员里，同一实例不可并发 execute
 */
class PclTmpl : public AlgToolBase {
public:
	/** @brief 点云与模板标记置空 */
	PclTmpl();
	/** @brief 析构 */
	~PclTmpl() override;

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

	/** @brief 自动或调试定位。不创建模板 */
	int execute(const AlgToolIn& in, AlgToolOut* out) override;
	/** @brief 按示教点云创建临时模板并定位。正式模型留到保存 */
	int teach(const AlgToolIn& in, AlgToolOut* out);
	/** @brief 按动作 id 执行。teach.model 学习 */
	int action(const std::string& id, const AlgToolIn& in, AlgToolOut* out) override;

	/** @brief 把已保存的模型装进内存 */
	void init(const std::string& toolDataDir) override;
	/** @brief 把当前模型写入数据目录 */
	void save(const std::string& toolDataDir) override;
	/** @brief 清空目录下的模型文件与内存句柄 */
	void clear(const std::string& toolDataDir) override;

private:
	/** @brief 清空本拍匹配、公差与临时建模标记 */
	int tool_reset();
	/**
	 * @brief 取场景点云与示教点云；校验参数对象
	 * @return 无点云或参数类型不对时返回对应错误码
	 */
	int proc_input(const AlgToolIn& in);
	/** @brief 按示教点云创建临时模板，并记下调试快照 */
	int build_model(const AlgToolIn& in);
	/** @brief 取可用模板后 ICP 定位 */
	int alg_run(const AlgToolIn& in);
	/** @brief 位姿与 fitness 做公差，结果留在 tol_ */
	int judge_tol(const AlgToolIn& in) override;
	/** @brief 写出位姿、测值与公差 */
	int alg_rst_proc(const AlgToolIn& in, AlgToolOut* out);

	/** @brief 本算法库的缓存目录：toolDataDir 下的 pcl 子目录 */
	std::string lib_dir(const std::string& toolDataDir) const;
	/** @brief 读模板点云文件 */
	int read_model(HsCommData::Cloud3* cloud, const std::string& path);
	/** @brief 写模板点云文件 */
	int write_model(const HsCommData::Cloud3& cloud, const std::string& path);
	/**
	 * @brief 选出本拍用的模板点云
	 * @note 学习、测试或本拍已建模时优先临时模板；否则读正式模板；
	 *       都没有且不是学习模式时返回错误
	 */
	int ensure_model(const AlgToolIn& in, const HsCommData::Cloud3** model);
	/** @brief 记下创建时的模板点云，供调试保存 */
	void keep_teach_snapshot();
	/** @brief 体素下采样。leaf≤0 原样返回 */
	HsCommData::Cloud3 downsample(const HsCommData::Cloud3& cloud, double leaf) const;
	/** @brief 场景相对模板做 ICP，结果写入 matches_ */
	int locate(const AlgToolIn& in, const HsCommData::Cloud3& model);

	/** @brief 一条匹配：fitness 和位姿 */
	struct MatchOne {
		double fitness_; ///< ICP fitness
		HsCommData::Pose3 pose_; ///< 匹配位姿
		/** @brief fitness 清零 */
		MatchOne() : fitness_(0) {}
	};

	PclCloudOps ops_; ///< 点云互转与 IO
	HsCommData::Cloud3 scene_; ///< 本拍场景点云
	HsCommData::Cloud3 teach_cloud_; ///< 本拍示教点云
	HsCommData::Cloud3 model_; ///< 正式模板
	HsCommData::Cloud3 tmp_model_; ///< 临时模板
	HsCommData::Cloud3 model_keep_; ///< 创建时的调试快照
	bool model_ok_; ///< 正式模板有效
	bool tmp_ok_; ///< 临时模板有效
	bool teach_keep_; ///< 已有创建时的调试快照
	bool built_; ///< 本拍已创建临时模板
	HsCommData::Pose3 pose_org_; ///< 创建模板后在原场景上的定位
	bool pose_org_ok_; ///< pose_org_ 已写入
	std::vector<MatchOne> matches_; ///< 本拍匹配
	TolShot tol_; ///< 本拍测值和公差。写出前不进 AlgToolOut
	int shot_err_; ///< 本拍错误码。0 为成功
	std::string shot_msg_; ///< 本拍说明
	double fitness_; ///< 本拍最佳 fitness

	/** @brief 转到 tool_name */
	std::string toolName() const override { return tool_name(); }
	/** @brief 转到 alg_lib：PCL */
	AlgLibKind algLib() const override { return alg_lib(); }
	/** @brief 转到 version_notes */
	std::vector<VerNote> versionNotes() const override { return version_notes(); }
};

}

#endif
