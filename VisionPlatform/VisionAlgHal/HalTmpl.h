/**
 * @file HalTmpl.h
 * @brief NCC 与形状模板定位（Halcon）
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_HALCONTEMPLATE_H
#define VISION_HALCONTEMPLATE_H

#include "AlgToolBase.h"
#include "TolShot.h"
#include "ToolParaTmpl.hpp"
#include "HalconCpp.h"
#include <vector>

namespace Vision {

	/**
	 * @class HalTmpl
	 * @brief NCC 与形状模板定位（Halcon）
	 */
	class HalTmpl : public AlgToolBase
	{
	public://基础、骨架接口
		//构造
		HalTmpl();
		//析构
		~HalTmpl();

		/** @brief 克隆本实例 */
		AlgToolBase* clone() const override;

	public://描述信息、版本管控
		/** @brief 注册名 */
		std::string tool_name() const override;
		/** @brief 算法库 */
		AlgLibKind alg_lib() const override;
		/** @brief 工具版本 */
		std::string version() const override;
		/** @brief 版本变更，只追加 */
		std::vector<VerNote> version_notes() const override;

	public:
		/*********************************对外接口**************************************/
		/** @brief 自动或调试定位。不创建模板 */
		int execute(const AlgToolIn& in, AlgToolOut* out) override;
		/** @brief 按示教区创建临时模板并定位。正式句柄留到保存 */
		int teach(const AlgToolIn& in, AlgToolOut* out);
		/** @brief 查看轮廓 */
		int inspect_contour(const AlgToolIn& in, AlgToolOut* out);
		/** @brief 按动作 id 执行。teach.model 学习，insp.contour 看轮廓 */
		int action(const std::string& id, const AlgToolIn& in, AlgToolOut* out) override;

		/*****************************算法工具配置读写**********************************/
		/**
		 * @brief 把已保存的模型装进内存
		 * @param toolDataDir 本工具数据目录
		 */
		void init(const std::string& toolDataDir) override;
		/**
		 * @brief 把当前模型写入数据目录
		 * @param toolDataDir 本工具数据目录
		 */
		void save(const std::string& toolDataDir) override;

	private://内部接口
		int tool_reset();//工具复位
		int proc_input(const AlgToolIn& in);//输入检查+处理
		int build_model(const AlgToolIn& in);//按示教区创建临时模板
		int alg_run(const AlgToolIn& in);//算法运行：定位，并丢掉越界结果
		int judge_tol(const AlgToolIn& in) override;//偏移和个数做公差，结果留在 tol_
		int alg_rst_proc(const AlgToolIn& in, AlgToolOut* out);//结果处理：仿射区域并写出输出

		int read_model(HalconCpp::HTuple* model_id, const std::string& path, bool shape);//按建模方式加载，无效时写入 -1
		int write_model(const HalconCpp::HTuple* model_id, const std::string& path, bool shape);//按建模方式保存
		std::string lib_dir(const std::string& toolDataDir) const;//本算法库的缓存目录
		int ensure_model(const AlgToolIn& in, bool shape, const HalconCpp::HTuple** model);//正式模板读失败时按原图重建到临时句柄
		int locate(const AlgToolIn& in, const HalconCpp::HTuple& model, bool shape);//在当前 image_ 上定位
		int rebuild_from_org(const AlgToolIn& in, bool shape);//用工具保存的原图和当前示教区重建
		void keep_teach_snapshot();//记下创建时的模板图、示教区和原点，供调试文件使用
		bool read_numbers(const std::string& path, double* a, double* b, double* c);//按 Halcon 的 UTF-8 路径读三个数
		void write_numbers(const std::string& path, double a, double b, double c);//按 Halcon 的 UTF-8 路径写三个数
		void take_region(const AlgToolIn& in, const char* port, HalconCpp::HObject* region);//按口名收成一块区域

	private:
		/** @brief 一条匹配：分数和位姿 */
		struct MatchOne {
			double score_; ///< 匹配分数
			HsCommData::Pose2 pose_; ///< 匹配位姿，角度为弧度
			MatchOne() : score_(0) {}
		};

		/** @brief 金字塔层数。auto 或整数 */
		HalconCpp::HTuple create_levels(const NumOrString& levels) const;
		/** @brief 角度步长。auto 或度，交给 Halcon 时换成弧度 */
		HalconCpp::HTuple angle_step_of(const NumOrString& step) const;
		/** @brief 最小对比度。auto 或整数 */
		HalconCpp::HTuple min_contrast_of(const NumOrString& contrast) const;
		/** @brief NCC 只接受两种极性 */
		std::string ncc_metric(const std::string& metric) const;
		/** @brief NCC 亚像素收成 true 或 false */
		std::string ncc_sub_pixel(const std::string& sub) const;
		/** @brief 对比度收成 Halcon 的词、一个数，或 [下限, 上限] */
		HalconCpp::HTuple contrast_tuple(const NumOrString& contrast) const;

		HalconCpp::HObject image_; ///< 本拍灰度图
		HalconCpp::HObject image_teach_; ///< 示教区裁剪图，用于建模
		HalconCpp::HObject search_region_; ///< 搜索区域。没有搜索区时为整图
		HalconCpp::HObject teach_region_; ///< 示教区域
		HalconCpp::HObject follow_region_; ///< 跟随区域
		HalconCpp::HObject result_region_; ///< 本拍示教区仿射结果
		HalconCpp::HObject ref_region_; ///< 本拍跟随区仿射结果

		HalconCpp::HTuple shape_model_; ///< 自动运行shape模板ID
		HalconCpp::HTuple ncc_model_; ///< 自动运行ncc模板ID
		HalconCpp::HTuple tmp_shape_model_; ///< 临时运行shape模板ID
		HalconCpp::HTuple tmp_ncc_model_; ///< 临时运行ncc模板ID

		std::vector<MatchOne> matches_; ///< 本拍匹配，已去掉越界项
		TolShot tol_; ///< 本拍测值和公差。写出前不进 AlgToolOut
		double origin_row_; ///< 示教区中心行，仿射参考
		double origin_col_; ///< 示教区中心列，仿射参考
		int img_w_; ///< 图像宽
		int img_h_; ///< 图像高
		int shot_err_; ///< 本拍错误码。0 为成功
		std::string shot_msg_; ///< 本拍说明
		bool built_; ///< 本拍已创建临时模板
		HsCommData::Pose2 pose_org_; ///< 创建模板后在原图上的定位
		bool pose_org_ok_; ///< pose_org_ 已写入
		HalconCpp::HObject image_tpl_keep_; ///< 创建时的模板图。只用于调试保存
		HalconCpp::HObject region_teach_keep_; ///< 创建时的示教区域。只用于调试保存
		double origin_row_keep_; ///< 创建时的原点行
		double origin_col_keep_; ///< 创建时的原点列
		bool teach_keep_; ///< 已有创建时的调试快照

		std::string toolName() const override { return tool_name(); }
		AlgLibKind algLib() const override { return alg_lib(); }
		std::vector<VerNote> versionNotes() const override { return version_notes(); }
	};

}

#endif
