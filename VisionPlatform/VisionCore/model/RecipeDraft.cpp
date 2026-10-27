/**
 * @file RecipeDraft.cpp
 * @brief RecipeDraft实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "RecipeDraft.h"
#include "ParamKeys.h"
#include "VpUtil.h"

namespace Vision {

// 是否运行态键
bool isRuntimeParamKey(const std::string& key)
{
    RuntimeKey rk;
    return runtimeKeyFromName(key, &rk);
}

// 迁移运行态键
void ToolCfg::migrateRuntime()
{
        const std::vector<std::string> keys = params.keys();
    for (size_t i = 0; i < keys.size(); ++i) {
        RuntimeKey rk;
        if (!runtimeKeyFromName(keys[i], &rk)) {
            continue;
        }
        runtime.insert(keys[i], params.value(keys[i]));
        params.raw().erase(keys[i]);
    }
}

// 比较两份 ParamMap 是否相同
bool paramMapEquals(const ParamMap& a, const ParamMap& b)
{
    if (a.size() != b.size()) {
        return false;
    }
    const std::vector<std::string> keys = a.keys();
    for (size_t i = 0; i < keys.size(); ++i) {
        if (a.value(keys[i]).toString() != b.value(keys[i]).toString()) {
            return false;
        }
    }
    return true;
}

// 比较两份 ROI 列表是否相同
bool toolRoisEquals(const std::vector<HsCommData::HsObject>& a, const std::vector<HsCommData::HsObject>& b)
{
    if (a.size() != b.size()) {
        return false;
    }
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i].kind() != b[i].kind()) {
            return false;
        }
    }
    return true;
}

// 从 ToolCfg 加载配方
void ToolRecipe::loadFrom(const ToolCfg& t)
{
    params = t.param.run;
    rois = t.param.rois;
    deviceId = t.param.deviceId;
    algLib = t.param.algLib;
}

// 写回 ToolCfg 配方
void ToolRecipe::applyTo(ToolCfg* t) const
{
    if (!t) {
        return;
    }
    t->param.run = params;
    t->param.rois = rois;
    t->param.deviceId = deviceId;
    t->param.algLib = algLib;
    t->syncRois();
}

// 比较两份工具配方是否相同
bool ToolRecipe::equals(const ToolRecipe& other) const
{
    return deviceId == other.deviceId
        && algLib == other.algLib
        && paramMapEquals(params, other.params)
        && toolRoisEquals(rois, other.rois);
}

// 深拷贝流程组
ToolGroup ToolGroup::clone() const
{
    ToolGroup c = *this;
    c.children.clear();
    for (size_t i = 0; i < children.size(); ++i) {
        c.children.push_back(children[i].clone());
    }
    return c;
}

// 深拷贝 JobCfg
JobCfg JobCfg::clone() const
{
    JobCfg j = *this;
    j.tools = tools;
    j.links = links;
    j.root = root.clone();
    for (size_t i = 0; i < j.tools.size(); ++i) {
        j.tools[i].migrateRuntime();
    }
    bool anyIn = false;
    for (size_t i = 0; i < j.tools.size(); ++i) {
        if (!j.tools[i].links.empty()) {
            anyIn = true;
            break;
        }
    }
    if (anyIn) {
        j.rebuildLinksCache();
    }
    return j;
}

// 两份 JobCfg 配方是否相同
bool JobCfg::equals(const JobCfg& other) const
{
    if (id != other.id || name != other.name || timeoutMs != other.timeoutMs
        || failPolicy != other.failPolicy || trigger != other.trigger
        || order != other.order || enabled != other.enabled) {
        return false;
    }
    if (tools.size() != other.tools.size() || links.size() != other.links.size()) {
        return false;
    }
    for (size_t i = 0; i < tools.size(); ++i) {
        const ToolCfg& ta = tools[i];
        const ToolCfg& tb = other.tools[i];
        ToolRecipe ra;
        ToolRecipe rb;
        ra.loadFrom(ta);
        rb.loadFrom(tb);
        if (ta.id != tb.id || ta.toolName != tb.toolName || ta.name != tb.name
            || ta.enabled != tb.enabled || ta.canvasX != tb.canvasX
            || ta.canvasY != tb.canvasY || ta.canvasPlaced != tb.canvasPlaced
            || !ra.equals(rb)) {
            return false;
        }
    }
    for (size_t i = 0; i < links.size(); ++i) {
        if (links[i].fromId != other.links[i].fromId || links[i].toId != other.links[i].toId
            || links[i].fromPort != other.links[i].fromPort
            || links[i].toPort != other.links[i].toPort
            || links[i].routeCustom != other.links[i].routeCustom
            || links[i].routePoints.size() != other.links[i].routePoints.size()) {
            return false;
        }
        for (size_t pi = 0; pi < links[i].routePoints.size(); ++pi) {
            if (links[i].routePoints[pi].x != other.links[i].routePoints[pi].x
                || links[i].routePoints[pi].y != other.links[i].routePoints[pi].y) {
                return false;
            }
        }
    }
    if (resultFromRuleId != other.resultFromRuleId
        || resultRules.size() != other.resultRules.size()) {
        return false;
    }
    for (size_t i = 0; i < resultRules.size(); ++i) {
        const RstRule& ra = resultRules[i];
        const RstRule& rb = other.resultRules[i];
        if (ra.id != rb.id || ra.name != rb.name || ra.toolId != rb.toolId
            || ra.terms.size() != rb.terms.size()) {
            return false;
        }
        for (size_t j = 0; j < ra.terms.size(); ++j) {
            if (ra.terms[j].kind != rb.terms[j].kind
                || ra.terms[j].sourceId != rb.terms[j].sourceId
                || ra.terms[j].state != rb.terms[j].state) {
                return false;
            }
        }
    }
    return true;
}

// 缺省工站配方：使能、串行、超时 500 毫秒、失败停工站
StationRecipe::StationRecipe()
    : enabled(true)
    , jobSchedule(ScheduleSequence)
    , timeoutMs(500)
    , failPolicy(FailStopStation)
{
}

// 从 StationCfg 加载 draft
void StationRecipe::loadFrom(const StationCfg& st)
{
    name = st.name;
    stationCode = st.stationCode;
    enabled = st.enabled;
    jobSchedule = st.jobSchedule;
    timeoutMs = st.timeoutMs;
    failPolicy = st.failPolicy;
    boundDevices = st.boundDevices;
    jobs.clear();
    for (size_t i = 0; i < st.jobs.size(); ++i) {
        jobs.push_back(st.jobs[i].clone());
    }
    commChannelId = st.commChannelId;
}

// 写回 StationCfg
void StationRecipe::applyTo(StationCfg* st) const
{
    if (!st) {
        return;
    }
    st->name = name;
    st->stationCode = stationCode;
    st->enabled = enabled;
    st->jobSchedule = jobSchedule;
    st->timeoutMs = timeoutMs;
    st->failPolicy = failPolicy;
    st->boundDevices = boundDevices;
    st->jobs = jobs;
    st->commChannelId = commChannelId;
}

// 比较两份工站配方是否相同
bool StationRecipe::equals(const StationRecipe& other) const
{
    if (name != other.name || stationCode != other.stationCode || enabled != other.enabled
        || jobSchedule != other.jobSchedule || timeoutMs != other.timeoutMs
        || failPolicy != other.failPolicy || commChannelId != other.commChannelId) {
        return false;
    }
    if (boundDevices.size() != other.boundDevices.size()) {
        return false;
    }
    for (size_t i = 0; i < boundDevices.size(); ++i) {
        if (boundDevices[i].deviceId != other.boundDevices[i].deviceId
            || boundDevices[i].enabled != other.boundDevices[i].enabled) {
            return false;
        }
    }
    if (jobs.size() != other.jobs.size()) {
        return false;
    }
    for (size_t i = 0; i < jobs.size(); ++i) {
        if (!jobs[i].equals(other.jobs[i])) {
            return false;
        }
    }
    return true;
}

// 从 HardwareDevice 加载 draft
void DeviceRecipe::loadFrom(const HardwareDevice& d)
{
    name = d.name;
    kind = d.kind;
    cameraBackend = d.cameraBackend;
    defaultParams = d.defaultParams;
}

// 写回 HardwareDevice
void DeviceRecipe::applyTo(HardwareDevice* d) const
{
    if (!d) {
        return;
    }
    d->name = name;
    d->kind = kind;
    d->cameraBackend = cameraBackend;
    d->defaultParams = defaultParams;
}

// 比较两份设备配方是否相同
bool DeviceRecipe::equals(const DeviceRecipe& other) const
{
    return name == other.name && kind == other.kind && cameraBackend == other.cameraBackend
        && paramMapEquals(defaultParams, other.defaultParams);
}

// 把工具配方 draft 写回正式 ToolCfg，保留 runtime
void ToolRecipe::commitTo(ToolCfg* formal) const
{
    if (!formal) {
        return;
    }
    applyTo(formal);
}

// 把 draft JobCfg 写回方案中同 id 的 JobCfg
bool Solution::commitJob(const JobCfg& draft)
{
    for (size_t si = 0; si < stations.size(); ++si) {
        for (size_t ji = 0; ji < stations[si].jobs.size(); ++ji) {
            if (stations[si].jobs[ji].id == draft.id) {
                stations[si].jobs[ji] = draft.clone();
                return true;
            }
        }
    }
    return false;
}

// 提交工站草稿，保留 id 与 runtimeView
bool Solution::commitStation(const StationCfg& draft)
{
    if (draft.id.isNull()) {
        return false;
    }
    StationCfg* st = findStationConfig(draft.id);
    if (!st) {
        return false;
    }
    st->name = draft.name;
    st->stationCode = draft.stationCode;
    st->jobSchedule = draft.jobSchedule;
    st->timeoutMs = draft.timeoutMs;
    st->failPolicy = draft.failPolicy;
    st->boundDevices = draft.boundDevices;
    st->jobs = draft.jobs;
    st->commChannelId = draft.commChannelId;
    return true;
}

// 把 draft Device 写回台账
bool Solution::commitDevice(const HardwareDevice& draft)
{
    if (draft.id.isNull()) {
        return false;
    }
    HardwareDevice* d = findDevice(draft.id);
    if (!d) {
        return false;
    }
    d->name = draft.name;
    d->kind = draft.kind;
    d->defaultParams = draft.defaultParams;
    return true;
}

}


