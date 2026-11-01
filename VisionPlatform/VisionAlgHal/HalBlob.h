/**
 * @file HalBlob.h
 * @brief 绝对、相对、动态阈值 Blob（Halcon）
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_HALCONBLOB_H
#define VISION_HALCONBLOB_H

#include "AlgToolBase.h"
#include "TolShot.h"
#include "ToolParaBlob.hpp"
#include "HalconCpp.h"
#include <vector>

namespace Vision {

	/**
	 * @class HalBlob
	 * @brief 绝对、相对、动态阈值 Blob（Halcon）
	 * @note 单拍状态存在成员里，同一实例不可并发 execute
	 */
	class HalBlob : public AlgToolBase
	{
	public:
		/** @brief 区域与计数置空 */
		HalBlob();
		/** @brief 析构 */
		~HalBlob();

		/** @brief 克隆本实例 */
		AlgToolBase* clone() const override;

	public:
		/** @brief 注册名 */
		std::string tool_name() const override;
		/** @brief 算法库 */
		AlgLibKind alg_lib() const override;
		/** @brief 工具版本 */
		std::string version() const override;
		/** @brief 版本变更，只追加 */
		std::vector<VerNote> version_notes() const override;

	public:
		/** @brief 在检测区里做阈值分割，结果再和搜索区求交 */
		int execute(const AlgToolIn& in, AlgToolOut* out) override;
		/** @brief 预览阈值流水线。preview.thresh */
		int action(const std::string& id, const AlgToolIn& in, AlgToolOut* out) override;

		/** @brief 打开时装入。斑点没有模型文件 */
		void init(const std::string& toolDataDir) override;
		/** @brief 保存。斑点没有模型文件 */
		void save(const std::string& toolDataDir) override;

	private:
		/** @brief 清空本拍区域、测值与错误码 */
		int tool_reset();
		/**
		 * @brief 取灰度图并收集检测区、搜索区与分步 ROI
		 * @return 无图返回 HS_ERR_IMAGE_EMPTY，参数不是 ToolParaBlob 返回 HS_ERR_TYPE
		 */
		int proc_input(const AlgToolIn& in);
		/** @brief 按步骤阈值、形态学与筛选得到结果区域 */
		int alg_run(const AlgToolIn& in);
		/** @brief 按个数与面积做公差，结果留在 tol_ */
		int judge_tol(const AlgToolIn& in) override;
		/** @brief 把区域、测值与公差写进 out */
		int alg_rst_proc(const AlgToolIn& in, AlgToolOut* out);

		/** @brief 在 domain 内按一步阈值参数二值化 */
		int segment_domain(const ToolParaBlob::Step& st, const HalconCpp::HObject& image,
		                   const HalconCpp::HObject& domain, HalconCpp::HObject* binary);
		/** @brief 跑完整步骤流水，得到连通域并集 */
		int run_steps(const ToolParaBlob::Run& rp, HalconCpp::HObject* region);
		/** @brief 对区域做一步形态学 */
		int step_morph(const ToolParaBlob::Step& st, HalconCpp::HObject* region);
		/** @brief 按面积等特征筛选连通域 */
		int step_select(const ToolParaBlob::Step& st, HalconCpp::HObject* region);
		/** @brief 按步索引取 ROI；无效索引返回 false */
		bool step_domain(int roiIndex, HalconCpp::HObject* roi) const;

		/**
		 * @brief 按口名把普通区并起来再减去屏蔽区
		 * @note 没有普通区时 region 为空
		 */
		void take_region(const AlgToolIn& in, const char* port, HalconCpp::HObject* region);
		/** @brief 收集各步引用的 ROI 区域 */
		void collect_step_rois(const AlgToolIn& in);
		/** @brief 把 one 并进 acc；首次写入时置 have */
		void append_region(HalconCpp::HObject* acc, bool* have, const HalconCpp::HObject& one) const;
		/** @brief 按 ROI 把区域拆成区内、区外两部分 */
		void split_region_by_roi(const HalconCpp::HObject& region, const HalconCpp::HObject& roi,
		                         HalconCpp::HObject* inside, bool* hasInside,
		                         HalconCpp::HObject* outside, bool* hasOutside) const;
		/** @brief 按步骤参数对 src 做形态学，写入 dst */
		bool apply_morph_op(const ToolParaBlob::Step& st, const HalconCpp::HObject& src,
		                    HalconCpp::HObject* dst) const;

		HalconCpp::HObject image_; ///< 本拍灰度图
		HalconCpp::HObject detect_region_; ///< 检测区域。阈值作用域
		HalconCpp::HObject search_region_; ///< 搜索区域。与结果求交
		std::vector<HalconCpp::HObject> step_rois_; ///< 各步引用的 ROI
		HalconCpp::HObject result_region_; ///< 本拍结果连通域
		HalconCpp::HObject max_region_; ///< 面积最大的连通域
		int img_w_; ///< 图像宽
		int img_h_; ///< 图像高
		int blob_count_; ///< 连通域个数
		double area_sum_; ///< 面积和
		double area_max_; ///< 最大连通域面积
		HalconCpp::HTuple area_; ///< 各连通域面积
		TolShot tol_; ///< 本拍测值和公差。写出前不进 AlgToolOut
		int shot_err_; ///< 本拍错误码。0 为成功
		std::string shot_msg_; ///< 本拍说明

		/** @brief 转到 tool_name：detect.blob */
		std::string toolName() const override { return tool_name(); }
		/** @brief 转到 alg_lib：Halcon */
		AlgLibKind algLib() const override { return alg_lib(); }
		/** @brief 转到 version_notes */
		std::vector<VerNote> versionNotes() const override { return version_notes(); }
	};

}

#endif
