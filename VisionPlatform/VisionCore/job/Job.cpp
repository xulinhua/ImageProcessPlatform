/**
 * @file Job.cpp
 * @brief Job实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "Job.h"
#include <HsCommData/HsError.h>
#include "AppPaths.h"
#include "CompositeDef.h"
#include "Solution.h"
#include "SolutionDataPaths.h"
#include "TextEncoding.h"
#include "ParamKeys.h"
#include "StageTimer.h"
#include "ToolBase.h"
#include "Translate.h"
#include "VpUtil.h"
#include <algorithm>
#include <cstdio>
#include <map>
#include <string>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace Vision {
static thread_local Job* t_runningJob = 0;

// 拷贝共享图像。空指针返回空图
static HsCommData::HsImage copyImage(const std::shared_ptr<HsCommData::HsImage>& p)
{
    if (!p) {
        return HsCommData::HsImage();
    }
    return *p;
}



// 绑定流程配置，并按配置创建工具实例
Job::Job(JobCfg* config)
    : m_config(config)
    , m_listener(nullptr)
    , m_executing(false)
    , m_autoPrepared(false)
    , m_recipeJob(nullptr)
    , m_recipeTool(nullptr)
{
    rebuildTools();
}

// 释放下属工具实例
Job::~Job()
{
    for (size_t i = 0; i < m_tools.size(); ++i) {
        if (m_tools[i]) {
            m_tools[i]->destroy();
            m_tools[i] = nullptr;
        }
    }
    m_tools.clear();
}

// 清空本拍缓存，帧号回到 1
void JobRuntime::clear()
{
    images.clear();
    overlays.clear();
    ok.clear();
    values.clear();
    io.clear();
    elapsedMs.clear();
    source.reset();
    lastOut = ToolOut();
    frameSeq = 1;
    loopIndex = -1;
    loopCount = 0;
}

// 换绑配置并重建工具实例
void Job::setConfig(JobCfg* config)
{
    m_config = config;
    rebuildTools();
}

// 同 id 同类型的实例留下，缺的创建，多的销毁
void Job::rebuildTools()
{
    std::map<std::string, ToolBase*> kept;
    for (size_t i = 0; i < m_tools.size(); ++i) {
        ToolBase* inst = m_tools[i];
        if (!inst) {
            continue;
        }
        const std::string id = inst->info().id.toString();
        if (id.empty() || kept.find(id) != kept.end()) {
            inst->destroy();
            continue;
        }
        kept[id] = inst;
    }
    m_tools.clear();
    if (!m_config) {
        for (std::map<std::string, ToolBase*>::iterator it = kept.begin(); it != kept.end(); ++it) {
            it->second->destroy();
        }
        m_paramStamp.clear();
        return;
    }
    for (size_t i = 0; i < m_config->tools.size(); ++i) {
        const ToolCfg& config = m_config->tools[i];
        const std::string id = config.id.toString();
        ToolBase* inst = 0;
        std::map<std::string, ToolBase*>::iterator old = kept.find(id);
        if (old != kept.end() && old->second->toolName() == config.toolName) {
            inst = old->second;
            kept.erase(old);
        } else if (old != kept.end()) {
            old->second->destroy();
            kept.erase(old);
            m_paramStamp.erase(id);
        }
        if (!inst) {
            inst = ToolRegistry::instance().create(config.toolName);
            if (!inst) {
                ToolBase* proto = ToolRegistry::instance().findByType(config.type);
                if (proto) {
                    inst = proto->clone();
                }
            }
            m_paramStamp.erase(id);
        }
        if (!inst) {
            emitLog(RunLogError, Translate::format("tool is not registered: {0}", config.toolName), config.id, -1);
            continue;
        }
        applyToolConfig(inst, config);
        m_tools.push_back(inst);
    }
    for (std::map<std::string, ToolBase*>::iterator it = kept.begin(); it != kept.end(); ++it) {
        m_paramStamp.erase(it->first);
        it->second->destroy();
    }
}
void Job::syncToolsToConfig()
{
    if (!m_config) {
        return;
    }
    for (size_t i = 0; i < m_tools.size(); ++i) {
        ToolBase* inst = m_tools[i];
        if (!inst) {
            continue;
        }
        ToolCfg* config = m_config->findTool(inst->info().id);
        if (config) {
            harvestToolConfig(inst, config);
        }
    }
}

// 按 id 查找工具实例
ToolBase* Job::findTool(const ObjectId& id) const
{
    for (size_t i = 0; i < m_tools.size(); ++i) {
        if (m_tools[i] && m_tools[i]->info().id == id) {
            return m_tools[i];
        }
    }
    return 0;
}

// 按下标取工具
ToolBase* Job::toolAt(int index) const
{
    if (index < 0 || index >= static_cast<int>(m_tools.size())) {
        return 0;
    }
    return m_tools[static_cast<size_t>(index)];
}

// 设置日志监听，不拥有
void Job::setListener(IRunListener* listener)
{
    m_listener = listener;
}

// 设置日志用的工站 id，不是 Station 指针
void Job::setStationId(const ObjectId& stationId)
{
    m_stationId = stationId;
}

// 写一行 Info 日志
void Job::emitLog(const std::string& line)
{
    emitLog(RunLogInfo, line);
}

// 写结构化运行日志
void Job::emitLog(RunLogLevel level, const std::string& message,
                  const ObjectId& toolId, int elapsedMs)
{
    RunLogEvent ev;
    ev.level = level;
    ev.message = message;
    ev.toolId = toolId;
    ev.elapsedMs = elapsedMs;
    log(ev);
}

// 写一行 Info 日志
void Job::log(const std::string& line)
{
    emitLog(RunLogInfo, line);
}

// 补全工站和流程 id 后交给监听器
void Job::log(const RunLogEvent& ev)
{
    if (!m_listener) {
        return;
    }
    RunLogEvent out = ev;
    if (out.stationId.isNull()) {
        out.stationId = m_stationId;
    }
    if (out.jobId.isNull() && m_config) {
        out.jobId = m_config->id;
    }
    m_listener->onRunLog(out);
}

// 清空工具缓存、源图，并清掉各工具运行态
void Job::clearRuntime()
{
    m_runtime.clear();
    publishRuntime();
    for (size_t i = 0; i < m_tools.size(); ++i) {
        if (m_tools[i]) {
            m_tools[i]->clearRuntime();
        }
    }
}

// 某工具上次输出图
HsCommData::HsImage Job::cachedImage(const ObjectId& toolId) const
{
    RtView hold = viewRuntime();
    if (!hold.rt) {
        return HsCommData::HsImage();
    }
    std::map<ObjectId, std::shared_ptr<HsCommData::HsImage> >::const_iterator it = hold.rt->images.find(toolId);
    if (it == hold.rt->images.end()) {
        return HsCommData::HsImage();
    }
    return copyImage(it->second);
}

// 某工具上次结果几何
std::vector<HsCommData::HsObject> Job::cachedOverlay(const ObjectId& toolId) const
{
    RtView hold = viewRuntime();
    if (!hold.rt) {
        return std::vector<HsCommData::HsObject>();
    }
    std::map<ObjectId, std::shared_ptr<std::vector<HsCommData::HsObject> > >::const_iterator it = hold.rt->overlays.find(toolId);
    if (it == hold.rt->overlays.end() || !it->second) {
        return std::vector<HsCommData::HsObject>();
    }
    return *it->second;
}

// 是否已有该工具的输出图
bool Job::hasCache(const ObjectId& toolId) const
{
    RtView hold = viewRuntime();
    if (!hold.rt) {
        return false;
    }
    return hold.rt->images.count(toolId) != 0;
}

// 该工具是否在本周期内跑过
bool Job::hasToolRun(const ObjectId& toolId) const
{
    RtView hold = viewRuntime();
    if (!hold.rt) {
        return false;
    }
    return hold.rt->ok.count(toolId) != 0;
}

// 该工具最近一次 OK 或 NG。未跑过返回 false
bool Job::cachedToolOk(const ObjectId& toolId, bool* ok) const
{
    RtView hold = viewRuntime();
    if (!hold.rt) {
        return false;
    }
    std::map<ObjectId, bool>::const_iterator it = hold.rt->ok.find(toolId);
    if (it == hold.rt->ok.end()) {
        return false;
    }
    if (ok) {
        *ok = it->second;
    }
    return true;
}

// 该工具最近一次耗时。未跑过返回 -1
int Job::cachedElapsedMs(const ObjectId& toolId) const
{
    RtView hold = viewRuntime();
    if (!hold.rt) {
        return -1;
    }
    std::map<ObjectId, int>::const_iterator it = hold.rt->elapsedMs.find(toolId);
    if (it == hold.rt->elapsedMs.end()) {
        return -1;
    }
    return it->second;
}

// 按连线解析的输入图
HsCommData::HsImage Job::cachedInputImage(const JobCfg& job, const ObjectId& toolId) const
{
    return inputImage(job, toolId);
}

// 按连线解析的上游位姿
bool Job::cachedInputPose(const JobCfg& job, const ObjectId& toolId, HsCommData::Pose2* pose) const
{
    return inputPose(job, toolId, pose);
}

// 某工具上次输出数值
ParamMap Job::cachedValues(const ObjectId& toolId) const
{
    RtView hold = viewRuntime();
    if (!hold.rt) {
        return ParamMap();
    }
    std::map<ObjectId, std::shared_ptr<ParamMap> >::const_iterator it = hold.rt->values.find(toolId);
    if (it == hold.rt->values.end() || !it->second) {
        return ParamMap();
    }
    return *it->second;
}

// 某工具上次分类输出。没有 io 时用旧缓存拼
ToolIo Job::cachedIo(const ObjectId& toolId) const
{
    RtView hold = viewRuntime();
    if (hold.rt) {
        std::map<ObjectId, std::shared_ptr<ToolIo> >::const_iterator it = hold.rt->io.find(toolId);
        if (it != hold.rt->io.end() && it->second) {
            return *it->second;
        }
    }
    return ioFromLegacyCache(toolId);
}

// 旧缓存没有分类输出时，用图、几何、数值和 OK 拼一份
ToolIo Job::ioFromLegacyCache(const ObjectId& toolId) const
{
    ToolIo io;
    RtView hold = viewRuntime();
    if (!hold.rt) {
        return io;
    }
    const JobRuntime* rt = hold.rt;
    std::map<ObjectId, std::shared_ptr<HsCommData::HsImage> >::const_iterator img = rt->images.find(toolId);
    if (img != rt->images.end() && img->second && !img->second->isNull()) {
        PortImage cached;
        cached.image = *img->second;
        io.images.push_back(cached);
    }
    std::map<ObjectId, std::shared_ptr<std::vector<HsCommData::HsObject> > >::const_iterator ov = rt->overlays.find(toolId);
    if (ov != rt->overlays.end() && ov->second) {
        io.appendObjects(*ov->second);
    }
    std::map<ObjectId, std::shared_ptr<ParamMap> >::const_iterator val = rt->values.find(toolId);
    if (val != rt->values.end() && val->second) {
        HsCommData::Pose2 p;
        if (poseFromValues(*val->second, &p)) {
            PortPose cachedPose;
            cachedPose.pose = p;
            io.poses.push_back(cachedPose);
        }
    }
    std::map<ObjectId, bool>::const_iterator okIt = rt->ok.find(toolId);
    if (okIt != rt->ok.end()) {
        PortLogic L;
        L.ok = okIt->second;
        L.valid = true;
        io.logics.push_back(L);
    }
    return io;
}

// 本流程源图
HsCommData::HsImage Job::sourceImage() const
{
    RtView hold = viewRuntime();
    if (!hold.rt) {
        return HsCommData::HsImage();
    }
    return copyImage(hold.rt->source);
}

// 优先图像连线的上游图，否则取执行序里上一张缓存图
HsCommData::HsImage Job::inputImage(const JobCfg& job, const ObjectId& toolId) const
{
    RtView hold = viewRuntime();
    const JobRuntime* rt = hold.rt;
    if (!rt) {
        return HsCommData::HsImage();
    }
    for (int i = 0; i < static_cast<int>(job.links.size()); ++i) {
        if (job.links[static_cast<size_t>(i)].toId != toolId) {
            continue;
        }
        const PortSlot& tp = job.links[static_cast<size_t>(i)].toPort;
        if (tp.dir != PortDir::In || tp.data != PortData::Image) {
            continue;
        }
        const ObjectId from = job.links[static_cast<size_t>(i)].fromId;
        std::map<ObjectId, std::shared_ptr<HsCommData::HsImage> >::const_iterator it = rt->images.find(from);
        if (it != rt->images.end() && it->second) {
            return *it->second;
        }
    }
    const std::vector<ObjectId> order = job.execOrder();
    HsCommData::HsImage last;
    for (int i = 0; i < static_cast<int>(order.size()); ++i) {
        if (order[static_cast<size_t>(i)] == toolId) {
            break;
        }
        std::map<ObjectId, std::shared_ptr<HsCommData::HsImage> >::const_iterator it = rt->images.find(order[static_cast<size_t>(i)]);
        if (it != rt->images.end() && it->second) {
            last = *it->second;
        }
    }
    return last;
}

// 合并区域、轮廓、ROI 连线的上游几何
std::vector<HsCommData::HsObject> Job::inputGraphics(const JobCfg& job, const ObjectId& toolId) const
{
    std::vector<HsCommData::HsObject> g;
    for (int i = 0; i < static_cast<int>(job.links.size()); ++i) {
        if (job.links[static_cast<size_t>(i)].toId != toolId) {
            continue;
        }
        const PortSlot& tp = job.links[static_cast<size_t>(i)].toPort;
        if (tp.dir != PortDir::In
            || (tp.data != PortData::Region && tp.data != PortData::Contour && tp.data != PortData::Roi)) {
            continue;
        }
        const ObjectId from = job.links[static_cast<size_t>(i)].fromId;
        const ToolIo src = cachedIo(from);
        { const std::vector<HsCommData::HsObject> regs = plainRegions(src.regions); g.insert(g.end(), regs.begin(), regs.end()); }
        { const std::vector<HsCommData::HsObject> cons = plainContours(src.contours); g.insert(g.end(), cons.begin(), cons.end()); }
        for (size_t k = 0; k < src.rois.size(); ++k) {
            if (src.rois[k].roi.isInitialized() && !src.rois[k].roi.isEmpty()) {
                g.push_back(src.rois[k].roi);
            }
        }
        RtView hold = viewRuntime();
        const JobRuntime* rt = hold.rt;
        if (src.regions.empty() && src.contours.empty() && src.rois.empty()
            && rt && rt->overlays.count(from) != 0 && rt->overlays.find(from)->second) {
            const std::vector<HsCommData::HsObject>& ov = *rt->overlays.find(from)->second;
            g.insert(g.end(), ov.begin(), ov.end());
        }
    }
    return g;
}

// 合并位姿连线的上游位姿，后写覆盖
bool Job::inputPose(const JobCfg& job, const ObjectId& toolId, HsCommData::Pose2* pose) const
{
    if (!pose) {
        return false;
    }
    bool found = false;
    for (int i = 0; i < static_cast<int>(job.links.size()); ++i) {
        if (job.links[static_cast<size_t>(i)].toId != toolId) {
            continue;
        }
        const PortSlot& tp = job.links[static_cast<size_t>(i)].toPort;
        if (tp.dir != PortDir::In || tp.data != PortData::Pose) {
            continue;
        }
        const ObjectId from = job.links[static_cast<size_t>(i)].fromId;
        const ToolIo src = cachedIo(from);
        for (int k = 0; k < static_cast<int>(src.poses.size()); ++k) {
            *pose = src.poses[static_cast<size_t>(k)].pose;
            found = true;
        }
        RtView hold = viewRuntime();
        const JobRuntime* rt = hold.rt;
        if (!found && rt && rt->values.count(from) != 0 && rt->values.find(from)->second) {
            HsCommData::Pose2 one;
            if (poseFromValues(*rt->values.find(from)->second, &one)) {
                *pose = one;
                found = true;
            }
        }
    }
    return found;
}

// 合并逻辑连线的上游数值
ParamMap Job::upstreamValues(const JobCfg& job, const ObjectId& toolId) const
{
    ParamMap u;
    RtView hold = viewRuntime();
    const JobRuntime* rt = hold.rt;
    if (!rt) {
        return u;
    }
    for (int i = 0; i < static_cast<int>(job.links.size()); ++i) {
        if (job.links[static_cast<size_t>(i)].toId != toolId) {
            continue;
        }
        const PortSlot& tp = job.links[static_cast<size_t>(i)].toPort;
        if (tp.dir != PortDir::In || tp.data != PortData::Logic) {
            continue;
        }
        const ObjectId from = job.links[static_cast<size_t>(i)].fromId;
        std::map<ObjectId, std::shared_ptr<ParamMap> >::const_iterator it = rt->values.find(from);
        if (it == rt->values.end() || !it->second) {
            continue;
        }
        const ParamMap& one = *it->second;
        const std::vector<std::string> keys = one.keys();
        for (int k = 0; k < static_cast<int>(keys.size()); ++k) {
            u.insert(keys[static_cast<size_t>(k)], one.value(keys[static_cast<size_t>(k)]));
        }
    }
    return u;
}

// 按连线把上游分类数据汇总到本工具输入
ToolIo Job::inputIo(const JobCfg& job, const ObjectId& toolId) const
{
    ToolIo io;
    for (int i = 0; i < static_cast<int>(job.links.size()); ++i) {
        if (job.links[static_cast<size_t>(i)].toId != toolId) {
            continue;
        }
        const PortSlot& tp = job.links[static_cast<size_t>(i)].toPort;
        if (tp.dir != PortDir::In) {
            continue;
        }
        const ToolIo src = cachedIo(job.links[static_cast<size_t>(i)].fromId);
        io.appendByType(src, job.links[static_cast<size_t>(i)].fromPort, tp, job.links[static_cast<size_t>(i)].index, job.links[static_cast<size_t>(i)].use);
    }
    return io;
}

// 把草稿的参数、搜索区和设备盖到正式工具上，供本次执行使用
ToolCfg Job::resolveRecipeTool(const ToolCfg* formal, const ToolCfg* recipeTool,
                                  const JobCfg* recipeJob) const
{
    if (!formal) {
        ToolCfg empty;
        return empty;
    }
    const ToolCfg* recipe = recipeTool;
    if (recipeJob) {
        const ToolCfg* jt = recipeJob->findTool(formal->id);
        if (jt) {
            recipe = jt;
        }
    }
    if (!recipe) {
        return *formal;
    }
    ToolCfg shadow = *formal;
    shadow.params = recipe->params;
    shadow.rois = recipe->rois;
    shadow.deviceId = recipe->deviceId;
    shadow.syncRois();
    return shadow;
}

// 尝试占用本流程。已经在执行则失败
bool Job::tryBeginRun()
{
    bool expected = false;
    return m_executing.compare_exchange_strong(expected, true);
}

// 参数指纹。自动运行时用来判断要不要重新填参数
std::string Job::paramStamp(const ToolParam& param) const
{
    std::string stamp = param.paraVersion;
    stamp.push_back('|');
    stamp += std::to_string(static_cast<int>(param.algLib));
    stamp.push_back('|');
    stamp += param.deviceId.toString();
    const ParamMap* maps[2] = { &param.learn, &param.run };
    for (int m = 0; m < 2; ++m) {
        std::vector<std::string> keys = maps[m]->keys();
        std::sort(keys.begin(), keys.end());
        for (size_t i = 0; i < keys.size(); ++i) {
            stamp.push_back('|');
            stamp += keys[i];
            stamp.push_back('=');
            stamp += maps[m]->value(keys[i]).toString();
        }
    }
    for (size_t i = 0; i < param.judge.size(); ++i) {
        const JudgeParam& one = param.judge[i];
        stamp.push_back('|');
        stamp += one.name;
        stamp.push_back(one.enable ? '1' : '0');
        stamp += std::to_string(one.minVal);
        stamp += std::to_string(one.maxVal);
    }
    stamp += "|roi";
    stamp += std::to_string(param.rois.size());
    stamp += std::to_string(param.roiItems.size());
    return stamp;
}

void Job::initAutoParas()
{
    for (size_t i = 0; i < m_tools.size(); ++i) {
        ToolBase* inst = m_tools[i];
        if (!inst) {
            continue;
        }
        const std::string id = inst->info().id.toString();
        const std::string stamp = paramStamp(inst->param());
        std::map<std::string, std::string>::iterator prev = m_paramStamp.find(id);
        if (prev != m_paramStamp.end() && prev->second == stamp && inst->paramOk()) {
            continue;
        }
        inst->initToolParam(inst->param());
        m_paramStamp[id] = stamp;
    }
    m_autoPrepared = true;
}
Job::RtView Job::viewRuntime() const
{
    RtView view;
    if (t_runningJob == this) {
        view.rt = &m_runtime;
        return view;
    }
    std::lock_guard<std::mutex> lock(m_publishMu);
    view.keep = m_published;
    view.rt = view.keep.get();
    return view;
}

// 把本拍运行态换成界面可见的快照
void Job::publishRuntime()
{
    std::shared_ptr<JobRuntime> next = std::make_shared<JobRuntime>(m_runtime);
    std::lock_guard<std::mutex> lock(m_publishMu);
    m_published = next;
}
void Job::endRun()
{
    publishRuntime();
    m_autoPrepared = false;
    m_executing.store(false);
}

namespace {

// 采集路径是否存在
bool grabPathExists(const std::string& p)
{
    if (p.empty()) {
        return false;
    }
    const DWORD a = GetFileAttributesW(utf8ToWide(p).c_str());
    return a != INVALID_FILE_ATTRIBUTES;
}

// 拼接采集路径
std::string joinGrabPath(const std::string& a, const std::string& b)
{
    if (a.empty()) {
        return b;
    }
    const char c = a[a.size() - 1];
    if (c == '\\' || c == '/') {
        return a + b;
    }
    return a + "\\" + b;
}

// 参数路径为空时用设备的 folder 或 file。相对路径再试 exe 目录
std::string resolveGrabPath(const std::string& paramPath, const HardwareDevice* dev)
{
    std::string p = paramPath;
    if (p.empty() && dev) {
        p = dev->defaultParams.value("folder").toString();
        if (p.empty()) {
            p = dev->defaultParams.value("file").toString();
        }
    }
    if (!p.empty() && !grabPathExists(p)) {
        const std::string alt = joinGrabPath(AppPaths::exeDir(), p);
        if (grabPathExists(alt)) {
            p = alt;
        }
    }
    return p;
}

// 采集设备状态。真相机还没接 SDK
std::string cameraDeviceStatus(const HardwareDevice* dev)
{
    if (!dev) {
        return std::string("missing");
    }
    if (dev->kind == DeviceFolder) {
        return std::string("folder");
    }
    if (dev->kind == DeviceImage) {
        return std::string("image");
    }
    if (dev->kind == DeviceCamera) {
        return std::string("no_sdk");
    }
    return std::string("not_acquire");
}

// 把各工具名字和数值放进引用表，键是工具 id
void fillRefValues(ToolIn* in, const JobCfg& job, const JobRuntime& rt)
{
    if (!in) {
        return;
    }
    for (int i = 0; i < static_cast<int>(job.tools.size()); ++i) {
        const ToolCfg& t = job.tools[static_cast<size_t>(i)];
        const std::string key = t.id.toString();
        in->refNames[key] = t.name;
        std::map<ObjectId, std::shared_ptr<ParamMap> >::const_iterator it = rt.values.find(t.id);
        if (it != rt.values.end() && it->second) {
            in->refValues[key] = *it->second;
        }
    }
}

// 逻辑工具没有有效输入时，用前面定位和检测的 OK 补判定
void fillJudgeLogics(ToolIn* in, const JobCfg& job, const ObjectId& self, const JobRuntime& rt)
{
    if (!in) {
        return;
    }
    for (int i = 0; i < static_cast<int>(in->inputIo.logics.size()); ++i) {
        if (in->inputIo.logics[static_cast<size_t>(i)].valid) {
            return;
        }
    }
    const std::vector<ObjectId> order = job.execOrder();
    for (int i = 0; i < static_cast<int>(order.size()); ++i) {
        if (order[static_cast<size_t>(i)] == self) {
            break;
        }
        if (rt.ok.count(order[static_cast<size_t>(i)]) == 0) {
            continue;
        }
        const ToolCfg* t = job.findTool(order[static_cast<size_t>(i)]);
        if (!t || t->type == ToolType::Acquire || t->type == ToolType::Comm || t->type == ToolType::Logic) {
            continue;
        }
        const bool one = rt.ok.find(t->id)->second;
        PortLogic L;
        L.ok = one;
        L.valid = true;
        L.message = t->name + (one ? "=OK" : "=NG");
        in->inputIo.logics.push_back(L);
    }
}

// 结果处理没有几何输入时，把前面定位和检测的叠层收进 inputIo
void fillMergeOverlays(ToolIn* in, const JobCfg& job, const ObjectId& self, const JobRuntime& rt)
{
    if (!in) {
        return;
    }
    if (!in->inputIo.regions.empty() || !in->inputIo.contours.empty() || !in->inputIo.rois.empty()) {
        return;
    }
    const std::vector<ObjectId> order = job.execOrder();
    for (int i = 0; i < static_cast<int>(order.size()); ++i) {
        if (order[static_cast<size_t>(i)] == self) {
            break;
        }
        const ToolCfg* t = job.findTool(order[static_cast<size_t>(i)]);
        if (!t || (t->type != ToolType::Locate && t->type != ToolType::Detect
                   && t->type != ToolType::RstProcess)) {
            continue;
        }
        std::map<ObjectId, std::shared_ptr<std::vector<HsCommData::HsObject> > >::const_iterator it = rt.overlays.find(t->id);
        if (it == rt.overlays.end() || !it->second) {
            continue;
        }
        in->inputIo.appendObjects(*it->second);
    }
}

// 流程正忙时的失败结果
ToolOut busyToolOut()
{
    ToolOut out;
    out.ok = false;
    out.error = true;
    out.message = std::string(Translate::tr("job is already running"));
    return out;
}

struct JobRunGuard {
    Vision::Job* job;
    Vision::Job* prev;
    explicit JobRunGuard(Vision::Job* j)
        : job(j)
        , prev(t_runningJob)
    {
        t_runningJob = j;
    }
    ~JobRunGuard()
    {
        if (job) {
            job->endRun();
        }
        t_runningJob = prev;
    }
};

} // namespace

// 从分类输出里取一块或全部几何。index 小于 0 表示全部
static bool collectPort(const ToolIo& io, PortData data, int index, std::vector<HsCommData::HsObject>* out)
{
    std::vector<HsCommData::HsObject> all;
    if (data == PortData::Roi) {
        for (size_t i = 0; i < io.rois.size(); ++i) {
            if (io.rois[i].use == RegionUse::Invalid) {
                continue;
            }
            if (io.rois[i].roi.isInitialized() && !io.rois[i].roi.isEmpty()) {
                all.push_back(io.rois[i].roi);
            }
        }
    } else if (data == PortData::Region) {
        for (size_t i = 0; i < io.regions.size(); ++i) {
            if (io.regions[i].use == RegionUse::Invalid) {
                continue;
            }
            if (io.regions[i].object.isInitialized() && !io.regions[i].object.isEmpty()) {
                all.push_back(io.regions[i].object);
            }
        }
    }
    if (all.empty()) {
        return false;
    }
    if (index < 0) {
        out->insert(out->end(), all.begin(), all.end());
        return true;
    }
    if (index >= static_cast<int>(all.size())) {
        return false;
    }
    out->push_back(all[static_cast<size_t>(index)]);
    return true;
}

// 按搜索项解析本工具自己的区域。引用在前，绘制在后
static bool resolveRoiItems(const std::vector<ToolRoiItem>& items, const JobRuntime& rt,
                            std::vector<ResolvedRoi>* out, std::string* err)
{
    out->clear();
    std::vector<ToolRoiItem> ordered;
    for (size_t i = 0; i < items.size(); ++i) {
        const ToolRoiItem& item = items[i];
        const bool isRef = item.kind == RoiItemKind::RefRoi || item.kind == RoiItemKind::RefRegion;
        if (isRef && !item.refToolId.isNull()) {
            ordered.push_back(item);
        }
    }
    for (size_t i = 0; i < items.size(); ++i) {
        const ToolRoiItem& item = items[i];
        const bool isRef = item.kind == RoiItemKind::RefRoi || item.kind == RoiItemKind::RefRegion;
        if (!isRef) {
            ordered.push_back(item);
        }
    }
    for (size_t i = 0; i < ordered.size(); ++i) {
        const ToolRoiItem& item = ordered[i];
        if (!item.enabled) {
            continue;
        }
        std::vector<HsCommData::HsObject> shapes;
        bool found = false;
        if (item.kind == RoiItemKind::DrawRoi || item.kind == RoiItemKind::DrawRegion) {
            if (item.shape.isInitialized() && !item.shape.isEmpty()) {
                shapes.push_back(item.shape);
                found = true;
            }
        } else {
            std::map<ObjectId, std::shared_ptr<ToolIo> >::const_iterator it = rt.io.find(item.refToolId);
            if (it != rt.io.end() && it->second) {
                const PortData data = (item.kind == RoiItemKind::RefRoi) ? PortData::Roi : PortData::Region;
                found = collectPort(*it->second, data, item.refIndex, &shapes);
                if (!found) {
                    const PortData other = (data == PortData::Roi) ? PortData::Region : PortData::Roi;
                    found = collectPort(*it->second, other, item.refIndex, &shapes);
                }
            }
        }
        if (!found) {
            if (item.presence == RoiPresence::Required) {
                if (err) {
                    *err = "required search region was not found";
                }
                return false;
            }
            continue;
        }
        for (size_t s = 0; s < shapes.size(); ++s) {
            HsCommData::HsObject shape = shapes[s];
            ResolvedRoi row;
            row.slot = item.slot;
            row.shape = shape;
            out->push_back(row);
        }
    }
    return true;
}

// 把解析好的搜索区写进输入。没有搜索项时沿用配方里的区域
static bool fillResolvedRois(const ToolCfg& tool, const JobRuntime& rt, ToolIn* in, std::string* err)
{
    if (!in) {
        return false;
    }
    if (tool.param.roiItems.empty()) {
        in->rois = tool.effectiveRois();
        in->ownRoisResolved = false;
        return true;
    }
    std::vector<ResolvedRoi> resolved;
    if (!resolveRoiItems(tool.param.roiItems, rt, &resolved, err)) {
        return false;
    }
    in->ownRoisResolved = true;
    in->rois.clear();
    in->ownRois.clear();
    in->ownRegions.clear();
    int roiSeq = 0;
    for (size_t i = 0; i < resolved.size(); ++i) {
        const ResolvedRoi& row = resolved[i];
        if (row.shape.kind() == HsCommData::ObjRegion) {
            PortRegion n;
            n.port = row.slot;
            n.index = roiSeq++;
            n.object = row.shape;
            in->ownRegions.push_back(n);
        } else {
            PortRoi n;
            n.port = row.slot;
            n.index = roiSeq++;
            n.roi = row.shape;
            in->ownRois.push_back(n);
            in->rois.push_back(row.shape);
        }
    }
    return true;
}
// 占用后跑单个工具。结果由工具写回缓存
ToolOut Job::runTool(Solution* sol, const ObjectId& toolId, const ToolCfg* recipeTool)
{
    if (!tryBeginRun()) {
        return busyToolOut();
    }
    JobRunGuard guard(this);
    return runToolBody(sol, toolId, recipeTool);
}

// 把组合工具对外的参数和搜索区盖到内部流程
static void overlayComposite(JobCfg* inner, const CompositeDef& def, const ToolParam& param)
{
    if (!inner) {
        return;
    }
    for (size_t i = 0; i < def.paras.size(); ++i) {
        const CompositeParaPub& pub = def.paras[i];
        ToolCfg* tool = inner->findTool(pub.innerToolId);
        if (!tool) {
            continue;
        }
        const ParamMap& src = pub.learn ? param.learn : param.run;
        if (!src.contains(pub.key)) {
            continue;
        }
        ParamMap& dst = pub.learn ? tool->param.learn : tool->param.run;
        dst.insert(pub.key, src.value(pub.key));
    }
    for (size_t i = 0; i < def.rois.size(); ++i) {
        const CompositeRoiPub& pub = def.rois[i];
        ToolCfg* tool = inner->findTool(pub.innerToolId);
        if (!tool) {
            continue;
        }
        tool->param.roiItems.clear();
        tool->param.rois.clear();
        for (size_t k = 0; k < param.roiItems.size(); ++k) {
            if (param.roiItems[k].slot != pub.name) {
                continue;
            }
            ToolRoiItem item = param.roiItems[k];
            item.slot = pub.innerSlot;
            tool->param.roiItems.push_back(item);
            if (item.shape.isInitialized() && !item.shape.isEmpty()) {
                tool->param.rois.push_back(item.shape);
            }
        }
    }
}

static ToolOut runCompositeNode(Solution* sol, const ToolCfg& node, const ToolIn& in)
{
    ToolOut out;
    const CompositeDef* def = findComposite(node.info.compositeId);
    if (!def) {
        out.ok = false;
        out.error = true;
        out.message = "composite definition was not found";
        return out;
    }
    JobCfg inner = def->inner;
    overlayComposite(&inner, *def, node.param);
    const ObjectId ext = ObjectId::create();
    for (size_t i = 0; i < def->ports.size(); ++i) {
        const CompositePort& port = def->ports[i];
        if (port.dir != PortDir::In) {
            continue;
        }
        ToolCfg* dest = inner.findTool(port.innerToolId);
        if (!dest) {
            continue;
        }
        ToolLink edge;
        edge.fromId = ext;
        edge.fromPort = PortSlot(PortDir::Out, port.data);
        edge.toId = port.innerToolId;
        edge.toPort = port.innerPort;
        edge.origin = LinkOriginManual;
        inner.links.push_back(edge);
    }
    inner.ensureTree();
    Job child(&inner);
    ToolIo seed = in.inputIo;
    if (seed.images.empty() && !in.input.isNull()) {
        PortImage image;
        image.image = in.input;
        seed.images.push_back(image);
    }
    if (!in.input.isNull()) {
        child.runtime().images[ext] = std::make_shared<HsCommData::HsImage>(in.input);
    }
    child.runtime().io[ext] = std::make_shared<ToolIo>(seed);
    const ToolOut ran = child.run(sol, 0);
    out = ran;
    for (size_t i = 0; i < def->ports.size(); ++i) {
        const CompositePort& port = def->ports[i];
        if (port.dir != PortDir::Out || port.data != PortData::Image) {
            continue;
        }
        const HsCommData::HsImage image = child.cachedImage(port.innerToolId);
        if (!image.isNull()) {
            out.image = image;
        }
    }
    return out;
}

ToolOut Job::runToolBody(Solution* sol, const ObjectId& toolId, const ToolCfg* recipeTool)
{
    ToolOut out;
    ToolCfg* formal = sol ? sol->findTool(toolId) : nullptr;
    StationCfg* st = nullptr;
    JobCfg* job = sol ? sol->findJobCfgOfTool(toolId, &st) : nullptr;
    if (!formal && m_recipeJob) {
        formal = const_cast<ToolCfg*>(m_recipeJob->findTool(toolId));
        job = const_cast<JobCfg*>(m_recipeJob);
    }
    if (!formal || !job) {
        out.message = std::string(Translate::tr("tool was not found"));
        out.error = true;
        return out;
    }
    ToolCfg execTool = resolveRecipeTool(formal, recipeTool, m_recipeJob);
    if (!execTool.enabled) {
        out.ok = true;
        out.message = std::string(Translate::tr("tool is disabled"));
        return out;
    }
    execTool.runtime = formal->runtime;
    execTool.ensureDefaults();

    JobCfg* linkJob = m_recipeJob ? const_cast<JobCfg*>(m_recipeJob) : job;
    ToolIn in;
    in.inputIo = inputIo(*linkJob, toolId);
    if (!in.inputIo.images.empty()) {
        in.input = in.inputIo.images[0].image; in.inputNative = in.inputIo.images[0].native;
    } else {
        in.input = inputImage(*linkJob, toolId);
        if (!in.input.isNull()) {
            { PortImage one; one.image = in.input; in.inputIo.images.push_back(one); }
        }
    }
    if (in.inputIo.regions.empty() && in.inputIo.contours.empty() && in.inputIo.rois.empty()) {
        in.inputIo.appendObjects(inputGraphics(*linkJob, toolId));
    }
    if (in.inputIo.poses.empty()) {
        HsCommData::Pose2 pose;
        if (inputPose(*linkJob, toolId, &pose)) {
            { PortPose onePose; onePose.pose = pose; in.inputIo.poses.push_back(onePose); }
        }
    }
    in.param = execTool.param;
    if (m_runtime.loopIndex >= 0) {
        in.param.run.insert("loopIndex", m_runtime.loopIndex);
        in.param.run.insert("loopCount", m_runtime.loopCount);
    }
    in.algLib = execTool.algLib;
    std::string roiErr;
    if (!fillResolvedRois(execTool, m_runtime, &in, &roiErr)) {
        out.ok = false;
        out.error = true;
        out.message = roiErr;
        return out;
    }
    in.runtime = execTool.runtime;
    in.upstream = upstreamValues(*linkJob, toolId);
    fillRefValues(&in, *linkJob, m_runtime);
    if (execTool.type == ToolType::Logic) {
        fillJudgeLogics(&in, *linkJob, toolId, m_runtime);
    }
    if (execTool.type == ToolType::RstProcess) {
        fillMergeOverlays(&in, *linkJob, toolId, m_runtime);
    }
    if (execTool.type == ToolType::Acquire) {
        const HardwareDevice* dev = sol ? sol->findDevice(execTool.deviceId) : nullptr;
        in.param.run.insert("deviceStatus", cameraDeviceStatus(dev));
        if (dev && dev->kind == DeviceImage && in.param.run.value("sourceMode").toString() != "file") {
            in.param.run.insert("images", dev->defaultParams.value("images").toString());
        }
        if (dev && dev->kind == DeviceFolder && in.param.run.value("path").toString().empty()
            && in.param.run.value("sourceMode").toString() != "file") {
            in.param.run.insert("path", dev->defaultParams.value("folder").toString());
            in.param.run.insert("sourceMode", std::string("folder"));
        }
        const std::string resolved = resolveGrabPath(in.param.run.value("path").toString(), dev);
        if (!resolved.empty()) {
            in.param.run.insert("path", resolved);
        }
        const int64_t fid = ++m_runtime.frameSeq;
        in.runtime.insert("frameId", static_cast<double>(fid));
    }
    if (execTool.type == ToolType::Comm) {
        const HardwareDevice* plc = sol ? sol->findDevice(execTool.deviceId) : nullptr;
        if (plc && plc->kind == DevicePlc) {
            const std::string chId = plc->defaultParams.value("commChannelId").toString();
            if (!chId.empty() && in.param.run.value("commChannelId").toString().empty()) {
                in.param.run.insert("commChannelId", chId);
            }
            if (in.param.run.value("slaveId").toInt(0) <= 0) {
                const int sid = plc->defaultParams.value("slaveId").toInt(1);
                in.param.run.insert("slaveId", sid > 0 ? sid : 1);
            }
        }
    }

    if (sol && !sol->filePath.empty()) {
        in.solutionDir = pathDirName(sol->filePath);
        const ObjectId stationId = st ? st->id : ObjectId();
        const ObjectId jobId = job ? job->id : ObjectId();
        in.toolDataDir = toolDataDir(sol->filePath, stationId, jobId, toolId);
        if (!in.toolDataDir.empty()) {
            ensureDirectoryTree(in.toolDataDir);
        }
    }

    // 配置里有这个工具但实例还没建出来时，先重建
    if (m_config && m_config->findTool(toolId) && !findTool(toolId)) {
        rebuildTools();
    }
    if (execTool.toolName == "composite") {
        return runCompositeNode(sol, execTool, in);
    }
    ToolBase* inst = findTool(toolId);
    applyToolConfig(inst, execTool);
    if (inst && !m_autoPrepared) {
        if (!inst->initToolParam(in.param)) {
            out.ok = false;
            out.error = true;
            out.message = inst->paramBlock();
            return out;
        }
        inst->setPara(in.param);
        m_paramStamp.erase(toolId.toString());
    } else if (inst && m_autoPrepared && !inst->paramOk()) {
        out.ok = false;
        out.error = true;
        out.message = inst->paramBlock();
        return out;
    }
    if (inst) {
        in.param.paraVersion = inst->param().paraVersion;
    }
    in.toolParam = inst ? inst->toolParam() : 0;
    out = runToolInstance(inst, in, formal);
    m_runtime.elapsedMs[toolId] = out.elapsedMs;
    if (!out.image.isNull()) {
        std::shared_ptr<HsCommData::HsImage> shot = std::make_shared<HsCommData::HsImage>(out.image);
        m_runtime.images[toolId] = shot;
        if (!m_runtime.source || m_runtime.source->isNull()) {
            m_runtime.source = shot;
        }
    }
    m_runtime.overlays[toolId] = std::make_shared<std::vector<HsCommData::HsObject> >(out.objects);
    m_runtime.ok[toolId] = out.ok;
    m_runtime.values[toolId] = std::make_shared<ParamMap>(out.values);
    m_runtime.io[toolId] = std::make_shared<ToolIo>(out.io);
    // 模板学完后清掉重建标志，避免下一拍再学
    if (formal->toolName == "locate.template"
        && formal->params.value("rebuildModel").toBool(false)) {
        formal->params.insert("rebuildModel", false);
        if (sol) {
            sol->dirty = true;
        }
    }

    {
        emitLog(out.error ? RunLogError : (out.ok ? RunLogInfo : RunLogWarn),
                Translate::format("[{0}] {1}  {2}",
                                  out.ok ? std::string("OK") : std::string("NG"),
                                  formal->name,
                                  out.message),
                toolId, out.elapsedMs);
    }
    return out;
}

// 按执行序先跑上游，再跑到指定工具为止
ToolOut Job::runToolChain(Solution* sol, const ObjectId& toolId, const ToolCfg* recipeTool,
                             const JobCfg* recipeJobArg)
{
    if (!tryBeginRun()) {
        return busyToolOut();
    }
    JobRunGuard guard(this);

    ToolOut last;
    if (!sol) {
        last.message = std::string(Translate::tr("no solution is open"));
        last.error = true;
        return last;
    }
    const JobCfg* savedRecipeJob = m_recipeJob;
    if (recipeJobArg) {
        m_recipeJob = recipeJobArg;
    }
    const JobCfg* recipeJob = m_recipeJob;
    JobCfg* job = recipeJob ? const_cast<JobCfg*>(recipeJob) : sol->findJobCfgOfTool(toolId, 0);
    if (!job) {
        last = runToolBody(sol, toolId, recipeTool);
        m_recipeJob = savedRecipeJob;
        return last;
    }
    const ToolCfg* chainRecipe = recipeTool;
    ToolCfg chainHolder;
    if (!chainRecipe && recipeJob) {
        const ToolCfg* jt = recipeJob->findTool(toolId);
        if (jt) {
            ToolCfg* formalStep = sol->findTool(toolId);
            if (!formalStep) {
                formalStep = const_cast<ToolCfg*>(jt);
            }
            chainHolder = resolveRecipeTool(formalStep, jt, 0);
            chainRecipe = &chainHolder;
        }
    }
    const std::vector<ObjectId> order = job->execOrder();
    const ToolCfg* savedRecipeTool = m_recipeTool;
    m_recipeTool = chainRecipe;
    bool reached = false;
    for (int i = 0; i < static_cast<int>(order.size()); ++i) {
        const ObjectId id = order[static_cast<size_t>(i)];
        const ToolCfg* stepRecipe = nullptr;
        ToolCfg stepHolder;
        if (recipeJob) {
            const ToolCfg* jt = recipeJob->findTool(id);
            if (jt) {
                ToolCfg* formalStep = sol->findTool(id);
                if (!formalStep) {
                    formalStep = const_cast<ToolCfg*>(jt);
                }
                stepHolder = resolveRecipeTool(formalStep, jt, 0);
                stepRecipe = &stepHolder;
            }
        } else if (chainRecipe && id == toolId) {
            stepRecipe = chainRecipe;
        }
        last = runToolBody(sol, id, stepRecipe);
        if (id == toolId) {
            reached = true;
            break;
        }
        if (!last.ok && job->failPolicy != FailContinue) {
            m_recipeTool = savedRecipeTool;
            m_recipeJob = savedRecipeJob;
            return last;
        }
    }
    if (!reached) {
        last = runToolBody(sol, toolId, chainRecipe);
    }
    m_recipeTool = savedRecipeTool;
    m_recipeJob = savedRecipeJob;
    return last;
}

// 按组执行。叶子跑工具，条件选支，循环按次数重跑，并行逐个跑完
ToolOut Job::runGroup(Solution* sol, JobCfg* job, const ToolGroup& g, bool fallbackOk, int depth)
{
    ToolOut last;
    last.ok = true;
    if (!job) {
        last.message = std::string(Translate::tr("no job"));
        last.ok = false;
        return last;
    }
    if (g.kind == GroupSubJob) {
        if (depth >= 8) {
            last.ok = false;
            last.message = std::string(Translate::tr("sub-job nesting is too deep"));
            return last;
        }
        JobCfg* ref = 0;
        if (sol && !g.subJobRef.empty()) {
            for (size_t s = 0; s < sol->stations.size() && !ref; ++s) {
                for (size_t j = 0; j < sol->stations[s].jobs.size(); ++j) {
                    JobCfg& one = sol->stations[s].jobs[j];
                    if (&one != job && one.name == g.subJobRef) {
                        ref = &one;
                        break;
                    }
                }
            }
        }
        if (!ref) {
            last.ok = false;
            last.message = std::string(Translate::tr("sub-job was not found"));
            return last;
        }
        Job nested(ref);
        return nested.run(sol, 0, depth + 1);
    }
    if (!g.toolId.isNull() && g.children.empty()) {
        return runToolBody(sol, g.toolId, 0);
    }
    if (g.children.empty()) {
        last.ok = fallbackOk;
        last.message = std::string(Translate::tr("branch is empty"));
        return last;
    }
    if (g.kind == GroupKindUndefined) {
        last.ok = false;
        last.message = std::string(Translate::tr("group kind is undefined"));
        return last;
    }
    if (g.kind == GroupIf) {
        const bool takeThen = evalIf(g, fallbackOk);
        const int idx = takeThen ? 0 : 1;
        emitLog(RunLogDebug, Translate::format("condition {0} takes {1}", g.name, takeThen ? std::string("Then") : std::string("Else")));
        if (idx < static_cast<int>(g.children.size())) {
            return runGroup(sol, job, g.children[static_cast<size_t>(idx)], fallbackOk, depth);
        }
        last.message = std::string(Translate::tr("condition branch is missing"));
        return last;
    }
    if (g.kind == GroupLoop) {
        int n = g.loopCount;
        if (n < 1) {
            n = 1;
        }
        emitLog(RunLogDebug, Translate::format("loop {0} x{1}", g.name, std::to_string(n)));
        const int savedIdx = m_runtime.loopIndex;
        const int savedCnt = m_runtime.loopCount;
        m_runtime.loopCount = n;
        last.ok = true;
        bool lastOk = fallbackOk;
        for (int k = 0; k < n; ++k) {
            m_runtime.loopIndex = k;
            for (int i = 0; i < static_cast<int>(g.children.size()); ++i) {
                last = runGroup(sol, job, g.children[static_cast<size_t>(i)], lastOk, depth);
                lastOk = last.ok;
                if (!last.ok && job->failPolicy != FailContinue) {
                    const bool nextIf = (i + 1 < static_cast<int>(g.children.size())
                                         && g.children[static_cast<size_t>(i + 1)].kind == GroupIf);
                    if (g.children[static_cast<size_t>(i)].kind != GroupIf && !nextIf) {
                        break;
                    }
                }
            }
            if (!last.ok && job->failPolicy != FailContinue) {
                break;
            }
        }
        m_runtime.loopIndex = savedIdx;
        m_runtime.loopCount = savedCnt;
        return last;
    }
    if (g.kind == GroupParallel) {
        last.ok = true;
        for (int i = 0; i < static_cast<int>(g.children.size()); ++i) {
            const ToolOut r = runGroup(sol, job, g.children[static_cast<size_t>(i)], fallbackOk, depth);
            if (!r.ok) {
                last.ok = false;
            }
            if (!r.image.isNull()) {
                last.image = r.image;
            }
            last.objects = r.objects;
            last.message = r.message;
            last.elapsedMs += r.elapsedMs;
        }
        return last;
    }
    bool lastOk = fallbackOk;
    for (int i = 0; i < static_cast<int>(g.children.size()); ++i) {
        last = runGroup(sol, job, g.children[static_cast<size_t>(i)], lastOk, depth);
        lastOk = last.ok;
        if (!last.ok && job->failPolicy != FailContinue) {
            // 失败后如果下一项是条件，继续跑，让条件能接到这次 NG
            const bool nextIf = (i + 1 < static_cast<int>(g.children.size())
                                 && g.children[static_cast<size_t>(i + 1)].kind == GroupIf);
            if (g.children[static_cast<size_t>(i)].kind != GroupIf && !nextIf) {
                break;
            }
        }
    }
    return last;
}

// 取条件源的 OK。期望 NG 时取反
bool Job::evalIf(const ToolGroup& g, bool fallbackOk) const
{
    bool ok = fallbackOk;
    if (!g.condSourceId.isNull()) {
        ok = vpMapValue(m_runtime.ok, g.condSourceId, false);
    }
    if (g.condWant == CondThenOnNg) {
        return !ok;
    }
    return ok;
}

// 跑整条流程。recipe 非空时用草稿配置
ToolOut Job::run(Solution* sol, const JobCfg* recipeJob, int depth)
{
    if (!tryBeginRun()) {
        m_autoPrepared = false;
        return busyToolOut();
    }
    JobRunGuard guard(this);

    ToolOut last;
    JobCfg* formalJob = m_config;
    JobCfg* job = recipeJob ? const_cast<JobCfg*>(recipeJob) : formalJob;
    if (!job || !job->enabled) {
        if (!job) {
            last.ok = false;
            last.error = true;
            last.message = std::string(Translate::tr("no job"));
            emitLog(RunLogError, last.message);
        } else {
            last.ok = true;
            last.message = std::string(Translate::tr("job is disabled"));
            emitLog(RunLogInfo, last.message);
        }
        return last;
    }
    job->ensureTree();
    const JobCfg* savedRecipeJob = m_recipeJob;
    m_recipeJob = recipeJob;
    m_runtime.source.reset();
    last = runGroup(sol, job, job->root, true, depth);
    m_recipeJob = savedRecipeJob;
    last.source = copyImage(m_runtime.source);
    if (!job->resultFromRuleId.isNull() && !job->resultRules.empty()) {
        std::map<ObjectId, bool> toolOk;
        std::map<ObjectId, bool> toolRan;
        for (std::map<ObjectId, bool>::const_iterator it = m_runtime.ok.begin(); it != m_runtime.ok.end(); ++it) {
            toolOk[it->first] = it->second;
            toolRan[it->first] = true;
        }
        bool ruleOk = last.ok;
        std::string ruleMsg;
        if (job->evalRstRules(toolOk, toolRan, job->resultFromRuleId, &ruleOk, &ruleMsg)) {
            last.ok = ruleOk;
            if (!ruleMsg.empty()) {
                last.message = ruleMsg;
            }
            const RstRule* sel = nullptr;
            for (size_t i = 0; i < job->resultRules.size(); ++i) {
                if (job->resultRules[i].id == job->resultFromRuleId) {
                    sel = &job->resultRules[i];
                    break;
                }
            }
            if (sel && !sel->toolId.isNull() && hasCache(sel->toolId)) {
                last.image = cachedImage(sel->toolId);
                last.objects = cachedOverlay(sel->toolId);
                last.values = cachedValues(sel->toolId);
            }
        }
    }
    m_runtime.lastOut = last;
    return last;
}


// 把定位数值写进工具运行态，供下一拍参考
static void copyLocateRuntime(ParamMap* runtime, const ParamMap& values)
{
    if (!runtime) {
        return;
    }
    if (paramHas(values, ValueKey::X)) {
        runtimeSet(runtime, RuntimeKey::LastX, paramGet(values, ValueKey::X));
    }
    if (paramHas(values, ValueKey::Y)) {
        runtimeSet(runtime, RuntimeKey::LastY, paramGet(values, ValueKey::Y));
    }
    if (paramHas(values, ValueKey::Angle)) {
        runtimeSet(runtime, RuntimeKey::LastA, paramGet(values, ValueKey::Angle));
    }
    if (paramHas(values, ValueKey::Score)) {
        runtimeSet(runtime, RuntimeKey::LastScore, paramGet(values, ValueKey::Score));
    }
}

// 把配置里的身份、参数和运行态写入工具实例
void Job::applyToolConfig(ToolBase* inst, const ToolCfg& config)
{
    if (!inst) {
        return;
    }
    inst->applyInstanceData(config.info, std::vector<ToolInputLink>(), config.param, config.runtime);
}

// 把实例的身份、参数和运行态收回配置，不改画布坐标
void Job::harvestToolConfig(const ToolBase* inst, ToolCfg* config)
{
    if (!inst || !config) {
        return;
    }
    config->info = inst->info();
    config->links.clear();
    config->param = inst->param();
    config->runtime = inst->runtime();
}

// 执行工具实例。没有实例时用注册表里的原型
ToolOut Job::runToolInstance(ToolBase* inst, const ToolIn& in, ToolCfg* formal)
{
    ToolOut out;
    StageTimer watch;
    watch.begin("execute");
    ToolBase* runner = inst;
    if (!runner && formal) {
        runner = ToolRegistry::instance().find(formal->toolName);
        if (!runner) {
            runner = ToolRegistry::instance().findByType(formal->type);
        }
    }
    if (runner) {
        out = runner->execute(in);
    } else {
        out.ok = false;
        out.error = true;
        out.errCode = HS_ERR_NO_ALG;
        out.message = std::string(Translate::tr("tool is not registered"));
        out.image = in.input;
    }
    if (formal) {
        if (out.runtime.size() > 0) {
            formal->runtime = out.runtime;
        }
        copyLocateRuntime(&formal->runtime, out.values);
        out.toolId = formal->id;
    } else if (inst) {
        out.toolId = inst->info().id;
    }
    if (out.frameId == 0) {
        out.frameId = out.image.isNull() ? in.input.frameId : out.image.frameId;
    }
    if (out.image.isNull() && !in.input.isNull()) {
        out.image = in.input;
    }
    out.syncIo();
    watch.end("execute");
    const double elapsed = watch.ms("execute");
    out.elapsedMs = elapsed < 0 ? 0 : static_cast<int>(elapsed);

    if (inst) {
        if (formal) {
            inst->setRuntime(formal->runtime);
        } else if (out.runtime.size() > 0) {
            inst->setRuntime(out.runtime);
        }
        inst->setIo(out.io);
    }
    return out;
}

// 当前配置
JobCfg* Job::config() const
{
    return m_config;
}

// 运行态
JobRuntime& Job::runtime()
{
    return m_runtime;
}

// 界面可见的上一拍快照
std::shared_ptr<const JobRuntime> Job::publishedRuntime() const
{
    std::lock_guard<std::mutex> lock(m_publishMu);
    return m_published;
}

// 工具实例个数
int Job::toolCount() const
{
    return static_cast<int>(m_tools.size());
}

// 是否正在执行
bool Job::isRunning() const
{
    return m_executing.load();
}



}
