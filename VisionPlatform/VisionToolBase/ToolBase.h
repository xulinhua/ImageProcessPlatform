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

/** @class ToolDesc @brief 工具箱条目。type 是大类，toolName 是具体工具 */
struct ToolDesc {
    std::string toolName;               ///< 具体工具注册名
    ToolType type;                      ///< 所属 ToolType
    std::string displayName;            ///< 界面显示名
    std::string description;            ///< 说明
    std::string version;                ///< 工具版本 major.minor.patch
    std::vector<PortDesc> ports;        ///< 端口表
    std::vector<std::string> paramKeys; ///< 参数名
    ToolDesc() : type(ToolType::Undefined) {}
};


/**
 * @class ToolBase
 * @brief 工具实例基类：四件套成员 + 种类元数据 + execute + clone/destroy
 * @note 注册表持原型；Job 持 clone 出的实例。平台只见本头，不见子类头
 */
class VISION_TOOL_API ToolBase {
public:
    ToolBase();
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
    /** @brief 画布 ROI 等能力 */
    virtual ToolCaps capabilities() const;
    /** @brief 组装工具箱条目 */
    virtual ToolDesc desc() const;

    /************************************************** 运行 **************************************************/
    /** @brief 执行一次并返回输出 */
    virtual ToolOut execute(const ToolIn& in) = 0;
    /** @brief 示教一次。默认空结果 */
    virtual ToolOut teach(const ToolIn& in);
    /** @brief 打断长时间运行。默认空 */
    virtual void stop();

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
    bool paramOk() const;
    const std::string& paramBlock() const;
    /** @brief 当前工具的参数对象，没有则为 0 */
    virtual const ToolParaBase* toolParam() const;
private:
    ToolInfo m_info;                    ///< 身份（落盘）
    std::vector<ToolInputLink> m_links; ///< 输入连线（落盘）
    ToolParam m_param;                   ///< 参数（落盘）
    ToolIo m_io;                        ///< 本拍 IO（仅运行态）
    ParamMap m_runtime;                 ///< 运行态数值（不落盘）
    bool m_paramOk = true;               ///< 参数适配是否通过
    std::string m_paramBlock;            ///< 参数被拦住时的原因
};

/** @brief 按工具大类给出默认端口表 */
VISION_TOOL_API std::vector<PortDesc> defaultPortsForType(ToolType type);



}

#endif
