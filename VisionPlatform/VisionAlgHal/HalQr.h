/**
 * @file HalQr.h
 * @brief 二维码识别（Halcon）
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_HALCONQR_H
#define VISION_HALCONQR_H

#include "AlgToolBase.h"
#include "TolShot.h"
#include "ToolParaQr.hpp"
#include "HalconCpp.h"
#include <string>
#include <vector>

namespace Vision {

	/**
	 * @class HalQr
	 * @brief 二维码识别（Halcon）
	 * @note 单拍状态存在成员里，同一实例不可并发 execute
	 */
	class HalQr : public AlgToolBase
	{
	public://基础、骨架接口
		/** @brief 区域置空 */
		HalQr();
		/** @brief 析构 */
		~HalQr();

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
		/** @brief 在搜索区里读二维码，写出文本、区域和位姿 */
		int execute(const AlgToolIn& in, AlgToolOut* out) override;

		/************************************************** 模型读写 **************************************************/
		/** @brief 打开时装入正式数据。二维码没有模型文件 */
		void init(const std::string& toolDataDir) override;
		/** @brief 保存。二维码没有模型文件 */
		void save(const std::string& toolDataDir) override;

	private://内部接口
		/************************************************** 单拍流程 **************************************************/
		/** @brief 清本拍结果 */
		int tool_reset();
		/**
		 * @brief 取灰度图并按口名收集搜索区；校验 ToolParaQr
		 * @return 无图返回 HS_ERR_IMAGE_EMPTY，参数对象不是 ToolParaQr 返回 HS_ERR_TYPE
		 */
		int proc_input(const AlgToolIn& in);
		/** @brief 在搜索区里读码 */
		int alg_run(const AlgToolIn& in);
		/** @brief 文本和个数做公差。结果留在 tol_ */
		int judge_tol(const AlgToolIn& in) override;
		/** @brief 写出文本、区域和位姿，返回本拍错误码 */
		int alg_rst_proc(const AlgToolIn& in, AlgToolOut* out);

		/**
		 * @brief 按口名把普通区并起来再减去屏蔽区
		 * @note 没有普通区时 region 为空
		 */
		void take_region(const AlgToolIn& in, const char* port, HalconCpp::HObject* region);
		/** @brief 极性。非法值按 any */
		std::string polarity_of(const std::string& raw) const;

	private:
		/** @brief 二维码模型句柄。出作用域时释放 */
		struct QrModel {
			HalconCpp::HTuple id; ///< 模型句柄
			bool open;            ///< 已创建
			/** @brief 未创建状态 */
			QrModel();
			/** @brief 已创建时清除二维码模型 */
			~QrModel();
		};

		HalconCpp::HObject image_;         ///< 本拍灰度图
		HalconCpp::HObject search_region_; ///< 搜索区域
		HalconCpp::HObject symbol_region_; ///< 本拍码区并集
		std::vector<std::string> codes_;   ///< 解码文本
		std::vector<HsCommData::Pose2> poses_; ///< 码中心位姿，角度为弧度
		TolShot tol_;                      ///< 本拍测值和公差。写出前不进 AlgToolOut
		int img_w_;                        ///< 图像宽
		int img_h_;                        ///< 图像高
		int shot_err_;                     ///< 本拍错误码。0 为成功
		std::string shot_msg_;             ///< 本拍说明

		/** @brief 转到 tool_name：identify.qr */
		std::string toolName() const override { return tool_name(); }
		/** @brief 转到 alg_lib：Halcon */
		AlgLibKind algLib() const override { return alg_lib(); }
		/** @brief 转到 version_notes */
		std::vector<VerNote> versionNotes() const override { return version_notes(); }
	};

}

#endif
