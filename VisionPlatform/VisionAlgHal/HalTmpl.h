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
	 * @note 正式模板放在工具数据目录的 hal 子目录；示教只建临时模板，save 时才转正并落盘。
	 *       单拍状态存在成员里，同一实例不可并发 execute
	 */
	class HalTmpl : public AlgToolBase
	{
	public://基础、骨架接口
		/** @brief 模板句柄置 -1，图像和区域置空 */
		HalTmpl();
		/** @brief 释放正式和临时的形状、NCC 模板 */
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
		/************************************************** 对外接口 **************************************************/
		/** @brief 自动或调试定位。不创建模板 */
		int execute(const AlgToolIn& in, AlgToolOut* out) override;
		/** @brief 按示教区创建临时模板并定位。正式句柄留到保存 */
		int teach(const AlgToolIn& in, AlgToolOut* out);
		/** @brief 查看轮廓 */
		int inspect_contour(const AlgToolIn& in, AlgToolOut* out);
		/** @brief 按动作 id 执行。teach.model 学习，insp.contour 看轮廓 */
		int action(const std::string& id, const AlgToolIn& in, AlgToolOut* out) override;

		/************************************************** 模型读写 **************************************************/
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
		/************************************************** 单拍流程 **************************************************/
		/** @brief 清空本拍匹配、公差、结果区域、错误码和临时建模标记 */
		int tool_reset();
		/**
		 * @brief 取灰度图并按口名收集搜索区、示教区、跟随区；有示教区时裁出模板图并记原点
		 * @return 无图返回 HS_ERR_IMAGE_EMPTY，参数对象不是 ToolParaTmpl 返回 HS_ERR_TYPE
		 */
		int proc_input(const AlgToolIn& in);
		/** @brief 按示教区裁剪图创建临时模板，并记下调试快照；无示教图返回 HS_ERR_TMPL_NO_TEACH */
		int build_model(const AlgToolIn& in);
		/** @brief 取可用模板后定位，并丢掉越界结果 */
		int alg_run(const AlgToolIn& in);
		/** @brief 首个匹配相对原点（示教参数 refX/refY/refA 优先）的偏移和个数做公差，结果留在 tol_ */
		int judge_tol(const AlgToolIn& in) override;
		/** @brief 把本拍匹配、公差和示教区/跟随区的仿射结果写进 out，返回本拍错误码 */
		int alg_rst_proc(const AlgToolIn& in, AlgToolOut* out);

		/************************************************** 模板文件 **************************************************/
		/** @brief 按建模方式加载模板文件，失败或无效时写入 -1 并返回文件类错误码 */
		int read_model(HalconCpp::HTuple* model_id, const std::string& path, bool shape);
		/** @brief 按建模方式保存模板；句柄无效返回 HS_ERR_TMPL_NO_MODEL */
		int write_model(const HalconCpp::HTuple* model_id, const std::string& path, bool shape);
		/** @brief 本算法库的缓存目录：toolDataDir 下的 hal 子目录 */
		std::string lib_dir(const std::string& toolDataDir) const;
		/**
		 * @brief 选出本拍用的模板句柄
		 * @note 学习、测试或本拍已建模时优先临时模板；否则读正式模板（hal 子目录优先，再找旧位置）；
		 *       都没有且不是学习模式时，用 image_org.png 重建到临时句柄
		 */
		int ensure_model(const AlgToolIn& in, bool shape, const HalconCpp::HTuple** model);
		/** @brief 在当前 image_ 的搜索区内定位，丢掉图外和搜索区外的结果；一个都没有返回 HS_ERR_TMPL_NO_MATCH */
		int locate(const AlgToolIn& in, const HalconCpp::HTuple& model, bool shape);
		/** @brief 用工具保存的原图和当前示教区重建临时模板，并在原图上定位得到 pose_org_；结束后恢复本拍图像和搜索区 */
		int rebuild_from_org(const AlgToolIn& in, bool shape);
		/** @brief 记下创建时的模板图、示教区和原点，供调试文件使用 */
		void keep_teach_snapshot();
		/** @brief 按 Halcon 的 UTF-8 路径读首行三个数；文件不存在或格式不对返回 false */
		bool read_numbers(const std::string& path, double* a, double* b, double* c);
		/** @brief 按 Halcon 的 UTF-8 路径写一行三个数（覆盖写），Halcon 异常向上抛 */
		void write_numbers(const std::string& path, double a, double b, double c);
		/**
		 * @brief 按口名把普通区并起来再减去屏蔽区
		 * @note search 口同时收口名为空或 roi 的区域；没有普通区时 region 为空
		 */
		void take_region(const AlgToolIn& in, const char* port, HalconCpp::HObject* region);

	private:
		/** @brief 一条匹配：分数和位姿 */
		struct MatchOne {
			double score_; ///< 匹配分数
			HsCommData::Pose2 pose_; ///< 匹配位姿，角度为弧度
			/** @brief 分数清零 */
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

		/** @brief 转到 tool_name：locate.template */
		std::string toolName() const override { return tool_name(); }
		/** @brief 转到 alg_lib：Halcon */
		AlgLibKind algLib() const override { return alg_lib(); }
		/** @brief 转到 version_notes */
		std::vector<VerNote> versionNotes() const override { return version_notes(); }
	};

}

#endif
