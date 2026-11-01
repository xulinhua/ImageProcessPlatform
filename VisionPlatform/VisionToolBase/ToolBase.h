/**
 * @file ToolBase.h
 * @brief ToolBase头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_TOOLBASE_H
#define VISION_TOOLBASE_H

#include "ParamField.h"
#include "ToolSpec.h"
#include "ToolParaBase.h"
#include "ToolEnums.h"
#include "ToolExport.h"
#include "ToolInstanceData.h"
#include "ToolIo.h"
#include "VisionVersion.h"
#include "ToolRegistry.h"
#include <map>
#include <string>
#include <vector>

namespace Vision {

class AlgToolBase;
struct AlgToolIn;
struct AlgToolOut;

/**
 * @class ToolDesc
 * @brief 工具箱条目。type 是大类，toolName 是具体工具
 */
struct ToolDesc {
    std::string toolName;               ///< 具体工具注册名
    ToolType type;                      ///< 所属 ToolType
    std::string displayName;            ///< 界面显示名
    std::string description;            ///< 说明
    std::string version;                ///< 工具版本 major.minor.patch
    std::vector<PortDesc> ports;        ///< 端口表
    std::vector<std::string> paramKeys; ///< 参数名
    /** @brief 大类未定义的空条目 */
    ToolDesc() : type(ToolType::Undefined) {}
};


/**
 * @class ToolBase
 * @brief 工具实例基类：四件套成员 + 种类元数据 + execute + clone/destroy
 * @note 注册表持原型；Job 持 clone 出的实例。平台只见本头，不见子类头
 */
class VISION_TOOL_API ToolBase {
public:
    /** @brief 未挂算法 */
    ToolBase();
    /** @brief 释放挂着的算法实例 */
    virtual ~ToolBase();

    /************************************************** 实例数据 **************************************************/
    /** @brief 身份（落盘） */
    ToolInfo& info();
    /** @brief 身份（落盘） */
    const ToolInfo& info() const;
    /** @brief 写入身份 */
    void setInfo(const ToolInfo& value);
    /** @brief 输入连线（落盘） */
    std::vector<ToolInputLink>& links();
    /** @brief 输入连线（落盘） */
    const std::vector<ToolInputLink>& links() const;
    /** @brief 写入输入连线 */
    void setLinks(const std::vector<ToolInputLink>& value);
    /** @brief 参数（落盘） */
    ToolParam& param();
    /** @brief 参数（落盘） */
    const ToolParam& param() const;
    /** @brief 写入参数 */
    void setPara(const ToolParam& value);
    /** @brief 本拍 IO（仅运行态） */
    ToolIo& io();
    /** @brief 本拍 IO（仅运行态） */
    const ToolIo& io() const;
    /** @brief 写入本拍 IO */
    void setIo(const ToolIo& value);
    /** @brief 运行态数值（不落盘） */
    ParamMap& runtime();
    /** @brief 运行态数值（不落盘） */
    const ParamMap& runtime() const;
    /** @brief 写入运行态数值 */
    void setRuntime(const ParamMap& value);


    /************************************************** 身份 **************************************************/
    /** @brief 注册名，如 locate.find_circle */
    virtual std::string toolName() const = 0;
    /** @brief 所属工具大类 */
    virtual ToolType type() const = 0;
    /** @brief 界面显示名 */
    virtual std::string displayName() const = 0;
    /** @brief 工具说明；默认空 */
    virtual std::string description() const;
    /** @brief 工具版本；默认 1.0.0.0 */
    virtual std::string version() const;
    /** @brief 版本变更。只追加，不删旧行 */
    virtual std::vector<VerNote> versionNotes() const;

    /************************************************** 能力 **************************************************/
    /** @brief 新建实例时的缺省参数 */
    virtual ParamMap defaultParams() const;
    /** @brief 参数说明，界面据此生成表单 */
    virtual std::vector<ParamField> paramFields() const;
    /** @brief 输入输出端口表 */
    virtual std::vector<PortDesc> ports() const;
    /** @brief 端口表。默认转到 ports */
    virtual std::vector<PortDesc> cfg_port() const;
    /** @brief 画布 ROI 等能力 */
    virtual ToolCaps capabilities() const;
    /** @brief 组装工具箱条目 */
    virtual ToolDesc desc() const;
    /** @brief 注册名。默认转到 toolName */
    virtual std::string tool_name() const;
    /** @brief 界面显示名。默认转到 displayName */
    virtual std::string display_name() const;
    /** @brief 版本变更。默认转到 versionNotes */
    virtual std::vector<VerNote> version_notes() const;
    /** @brief 缺省参数。默认转到 defaultParams */
    virtual ParamMap default_params() const;
    /** @brief 运行参数说明。默认转到 paramFields */
    virtual std::vector<ParamField> cfg_param() const;
    /** @brief 按当前方案生成参数说明。默认转到 cfg_param */
    virtual std::vector<ParamField> cfg_param(const ToolParam& current) const;
    /** @brief 示教参数说明。没有示教数据的工具返回空 */
    virtual std::vector<ParamField> cfg_teach() const;
    /** @brief 本工具参数对象。默认转到 toolParam */
    virtual const ToolParaBase* tool_param() const;
    /** @brief load 之后核对参数。失败时把说明写入 detail */
    virtual int check_param(std::string* detail) const;
    /** @brief 输出项。默认空 */
    virtual OutputSpec cfg_output() const;
    /** @brief 公差项。默认空 */
    virtual TolSpec cfg_tol() const;
    /** @brief 显示项。默认空 */
    virtual DisplaySpec cfg_display() const;
    /** @brief 画布 ROI。默认转到 capabilities */
    virtual ToolCaps cfg_roi() const;
    /** @brief 调试页按钮 */
    virtual std::vector<ToolAction> cfg_action() const;
    /** @brief 各工具自己的一拍实现。结果写入 out */
    virtual int execute(const ToolIn& in, ToolOut* out);
    /** @brief 按动作 id 执行。未知动作失败 */
    virtual int action(const std::string& id, const ToolIn& in, ToolOut* out);
    /** @brief 把本工具要留下的视觉数据写入目录。返回错误码 */
    virtual int save();
    /** @brief 工具已删除时清掉本工具数据 */
    virtual void clear(const std::string& toolDataDir);

    /************************************************** 运行 **************************************************/
    /** @brief 执行一次并返回输出 */
    virtual ToolOut execute(const ToolIn& in) = 0;
    /** @brief 示教一次。默认空结果 */
    virtual ToolOut teach(const ToolIn& in);
    /** @brief 打断长时间运行。默认空 */
    virtual void stop();
    /** @brief 按数据目录挂上本工具自己的算法实例。目录不变则沿用 */
    void attachAlg(const std::string& toolDataDir);
    /** @brief 当前挂着的算法。没有则为 0 */
    AlgToolBase* attachedAlg() const;
    /** @brief 让算法把模型写入目录 */
    int saveAlg();
    /** @brief 丢掉算法并清掉工具数据目录 */
    void clearAlg(const std::string& toolDataDir);

    /************************************************** 生命周期 **************************************************/
    /**
     * @brief 克隆本实例（子类在本 DLL 内 new）
     * @return 新对象，由调用方 destroy
     */
    virtual ToolBase* clone() const = 0;
    /** @brief 在创建本对象的模块内释放（默认 delete this） */
    virtual void destroy();


    /************************************************** 实例数据 **************************************************/
    /** @brief 从另一实例抄四件套与 runtime（不含种类元数据） */
    void copyInstanceDataFrom(const ToolBase& other);
    /** @brief 写入身份/连线/参数/runtime（打开方案或重建时） */
    void applyInstanceData(const ToolInfo& i,
                           const std::vector<ToolInputLink>& l,
                           const ToolParam& p,
                           const ParamMap& rt);
    /** @brief 清空运行态 io 与 runtime */
    void clearRuntime();

    /************************************************** 参数 **************************************************/
    /** @brief 用落盘参数刷新运行时参数对象 */
    virtual bool initToolParam(ToolParam& src);
    /** @brief 按版本号连续适配。升或降由两个版本决定 */
    bool adaptParam(ToolParam& src, const std::string& oldVer, const std::string& newVer, std::string* detail);
    /** @brief 最近一次参数适配/装载是否通过；未通过时工具不应运行 */
    bool paramOk() const;
    /** @brief 参数被拦住的原因，如参数版本比工具新、需人工确认；通过时为空 */
    const std::string& paramBlock() const;
    /** @brief 当前工具的参数对象，没有则为 0 */
    virtual const ToolParaBase* toolParam() const;
protected:
    /** @brief 按参数说明拼缺省参数。设备项不进 ParamMap */
    ParamMap defaultsFrom(const std::vector<ParamField>& fields) const;
    /** @brief 把 ToolIn 收成算法输入。defaultRoiPort 非空时给未命名 ROI 填 port */
    void build_alg_in(const ToolIn& in, AlgToolIn* ain, const std::string& defaultRoiPort = std::string()) const;
    /** @brief 把算法输出抄到 ToolOut */
    void apply_alg_out(const ToolIn& in, const AlgToolOut& algOut, bool passImage, ToolOut* out) const;
    /** @brief 已 attachAlg 且 ain 填好后，调挂着的算法跑一拍或动作 */
    int run_attached_shot(const ToolIn& in, const std::string& actionId, AlgToolIn* ain, bool passImage, ToolOut* out);
    /** @brief 收口之后写结果日志。默认空 */
    virtual void log_result(ToolOut* out) const;
    /** @brief 图元生成后再补工具自己的图形 */
    virtual void disp_extra(const ToolIn& in, ToolOut* out) const;
private:
    ToolInfo m_info;                    ///< 身份（落盘）
    std::vector<ToolInputLink> m_links; ///< 输入连线（落盘）
    ToolParam m_param;                   ///< 参数（落盘）
    ToolIo m_io;                        ///< 本拍 IO（仅运行态）
    ParamMap m_runtime;                 ///< 运行态数值（不落盘）
    bool m_paramOk = true;               ///< 参数适配是否通过
    std::string m_paramBlock;            ///< 参数被拦住时的原因
    AlgToolBase* m_alg;                  ///< 本工具挂着的算法，自己拥有
    std::string m_algDir;                ///< 当前算法的数据目录
};

/** @brief 按工具大类给出默认端口表 */
VISION_TOOL_API std::vector<PortDesc> defaultPortsForType(ToolType type);



}

#endif
