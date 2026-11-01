/**
 * @file HalFilter.h
 * @brief 图像滤波流水。增强、平滑和灰度形态学按步骤做（Halcon）
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_HALFILTER_H
#define VISION_HALFILTER_H

#include "AlgToolBase.h"
#include "TolShot.h"
#include "ToolParaFilter.hpp"
#include "HalconCpp.h"
#include <vector>

namespace Vision {

	/**
	 * @class HalFilter
	 * @brief 图像滤波流水（Halcon）
	 * @note 单拍状态存在成员里，同一实例不可并发 execute
	 */
	class HalFilter : public AlgToolBase
	{
	public:
		/** @brief 图像与步骤置空 */
		HalFilter();
		/** @brief 析构 */
		~HalFilter();

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

		/** @brief 按步骤做滤波 */
		int execute(const AlgToolIn& in, AlgToolOut* out) override;
		/** @brief 预览单步滤波等调试动作 */
		int action(const std::string& id, const AlgToolIn& in, AlgToolOut* out) override;

		/** @brief 打开时装入。滤波无模型文件 */
		void init(const std::string& toolDataDir) override;
		/** @brief 保存。滤波无模型文件 */
		void save(const std::string& toolDataDir) override;

	private:
		/** @brief 滤波算子：增强、平滑与灰度形态学 */
		enum class FilterOp {
			EquHisto = 0,
			Emphasize,
			Illuminate,
			Scale,
			Invert,
			Mean,
			Median,
			Gauss,
			Bilateral,
			GrayOpen,
			GrayClose,
			GrayErode,
			GrayDilate,
			TopHat,
			BottomHat
		};

		/** @brief 滤波管线一步 */
		struct FilterStep {
			FilterOp op; ///< 算子
			int maskW; ///< 掩膜宽
			int maskH; ///< 掩膜高
			double coeff; ///< 系数 0
			double coeff2; ///< 系数 1
			bool scaleMax; ///< Scale 是否拉伸到 0~255
			std::string shape; ///< 形态学结构元形状
			int roiIndex; ///< 步骤 ROI 索引。-1 整图
			/** @brief 缺省一步直方图均衡 */
			FilterStep();
		};

		/** @brief 清空本拍图像、步骤与错误码 */
		int tool_reset();
		/**
		 * @brief 取灰度图、分步 ROI 与滤波参数
		 * @return 无图返回 HS_ERR_IMAGE_EMPTY，参数类型不对返回 HS_ERR_TYPE
		 */
		int proc_input(const AlgToolIn& in);
		/** @brief 按步骤依次滤波 */
		int alg_run(const AlgToolIn& in);
		/** @brief 预处理无测值，直接通过 */
		int judge_tol(const AlgToolIn& in) override;
		/** @brief 写出滤波图 */
		int alg_rst_proc(const AlgToolIn& in, AlgToolOut* out);

		/** @brief 从运行参数构建步骤表 */
		int build_steps(const ToolParaFilter::Run& rp, std::vector<FilterStep>* steps, std::string* err);
		/** @brief 字符串转滤波算子 */
		bool filter_op_from_string(const std::string& v, FilterOp* out) const;
		/** @brief 按步骤解析形态学掩膜尺寸与形状 */
		void morph_mask(const FilterStep& st, int* maskH, int* maskW, const char** shape) const;
		/** @brief 滤波一步。失败时写 err */
		bool apply_filter_op(HalconCpp::HObject* img, const FilterStep& st, std::string* err) const;
		/** @brief 收集各步引用的 ROI */
		void collect_step_rois(const AlgToolIn& in);

		HalconCpp::HObject image_; ///< 本拍灰度图
		HalconCpp::HObject result_image_; ///< 本拍滤波结果
		std::vector<HalconCpp::HObject> step_rois_; ///< 各步引用的 ROI
		std::vector<FilterStep> steps_; ///< 本拍滤波步骤
		int img_w_; ///< 图像宽
		int img_h_; ///< 图像高
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
