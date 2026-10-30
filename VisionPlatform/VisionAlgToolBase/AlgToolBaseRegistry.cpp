

/**
 * @file AlgToolBaseRegistry.cpp
 * @brief AlgToolBaseRegistry实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "AlgToolBase.h"
#include <cstdio>
#include <HsCommData/HsError.h>
#include "TextEncoding.h"
#include "VisionVersion.h"

namespace Vision {

namespace {

const Vision::VerNoteRaw kAlgNotes[] = {
    { "1.0.0.0", "首版" },
};

}

void AlgToolBase::loadLearn(const std::string&) {}
void AlgToolBase::saveLearn(const std::string&) {}
void AlgToolBase::clearLearn() {}
void AlgToolBase::destroy() { delete this; }

// 版本变更，只追加
std::vector<VerNote> AlgToolBase::versionNotes() const
{
    return verNotesFrom(kAlgNotes, static_cast<int>(sizeof(kAlgNotes) / sizeof(kAlgNotes[0])));
}

int AlgToolBase::shot_init(const AlgToolIn&, AlgToolOut* out)
{
    if (out) {
        *out = AlgToolOut();
    }
    return HS_OK;
}

int AlgToolBase::settle_result(const AlgToolIn&, AlgToolOut* out)
{
    if (!out) {
        return HS_ERR_NULL;
    }
    if (out->message.empty()) {
        out->message = out->ok ? "OK" : "NG";
    }
    return HS_OK;
}

int AlgToolBase::fail(int code, const std::string& msg, AlgToolOut* out) const
{
    if (!out) {
        return HS_ERR_NULL;
    }
    *out = AlgToolOut();
    out->ok = false;
    out->errCode = code == HS_OK ? HS_ERR_OPERATOR : code;
    out->message = msg;
    return out->errCode;
}

std::string AlgToolBase::tool_name() const { return toolName(); }
AlgLibKind AlgToolBase::alg_lib() const { return algLib(); }
std::vector<VerNote> AlgToolBase::version_notes() const { return versionNotes(); }

int AlgToolBase::judge_tol(const AlgToolIn&)
{
    return HS_OK;
}

int AlgToolBase::action(const std::string&, const AlgToolIn&, AlgToolOut* out)
{
    return fail(HS_ERR_OPERATOR, "unknown action", out);
}

void AlgToolBase::init(const std::string&)
{
}

void AlgToolBase::save(const std::string&)
{
}

void AlgToolBase::clear(const std::string&)
{
}

void AlgToolBase::write_alg_steps(AlgToolOut* out, const std::vector<Section>& sections, double totalMs) const
{
    if (out == nullptr) {
        return;
    }
    std::string spread = "算法耗时分布：";
    for (size_t i = 0; i < sections.size(); ++i) {
        if (i > 0) {
            spread += " ";
        }
        char ms[32];
        sprintf_s(ms, "%.1f ms", sections[i].ms);
        spread += sections[i].label;
        spread += "：";
        spread += ms;
    }
    out->algMs = totalMs;
    ToolTrace shown;
    shown.action = "算法";
    shown.detail = spread;
    out->traces.push_back(shown);
}

int AlgToolBase::run(ExecMode, const AlgToolIn& in, AlgToolOut* out)
{
    const int code = execute(in, out);
    if (!out) {
        return code == HS_OK ? HS_ERR_NULL : code;
    }
    if (code != HS_OK && out->errCode == HS_OK) {
        out->errCode = code;
        out->ok = false;
    }
    out->runOk = out->errCode == HS_OK && out->ok;
    out->judgeOk = out->runOk;
    if (out->runOk) {
        const int judged = judge_tol(in);
        if (judged != HS_OK) {
            out->judgeOk = false;
            out->ok = false;
            if (out->errCode == HS_OK) {
                out->errCode = judged;
            }
        }
    }
    settle_result(in, out);
    return out->errCode;
}

AlgToolBaseRegistry& AlgToolBaseRegistry::instance()
{
    static AlgToolBaseRegistry g;
    return g;
}

void AlgToolBaseRegistry::add(AlgToolBase* det)
{
    if (!det) {
        return;
    }
    if (!algLibKindValid(det->algLib())) {
        det->destroy();
        return;
    }
    std::map<AlgLibKind, AlgToolBase*>& slot = m_dets[det->toolName()];
    std::map<AlgLibKind, AlgToolBase*>::iterator it = slot.find(det->algLib());
    if (it != slot.end() && it->second && it->second != det) {
        it->second->destroy();
    }
    slot[det->algLib()] = det;
}

void AlgToolBaseRegistry::set(AlgToolBase* det)
{
    add(det);
}

AlgToolBase* AlgToolBaseRegistry::get(const std::string& toolName, AlgLibKind algLib) const
{
    std::map<std::string, std::map<AlgLibKind, AlgToolBase*> >::const_iterator it = m_dets.find(toolName);
    if (it == m_dets.end()) {
        return 0;
    }
    std::map<AlgLibKind, AlgToolBase*>::const_iterator jt = it->second.find(algLib);
    if (jt == it->second.end()) {
        return 0;
    }
    return jt->second;
}

AlgToolBase* AlgToolBaseRegistry::getPreferred(const std::string& toolName, AlgLibKind prefer) const
{
    if (AlgToolBase* d = get(toolName, prefer)) {
        return d;
    }
    if (AlgToolBase* d = get(toolName, AlgLibHal)) {
        return d;
    }
    if (AlgToolBase* d = get(toolName, AlgLibCv)) {
        return d;
    }
    std::map<std::string, std::map<AlgLibKind, AlgToolBase*> >::const_iterator it = m_dets.find(toolName);
    if (it == m_dets.end() || it->second.empty()) {
        return 0;
    }
    return it->second.begin()->second;
}

AlgToolBase* AlgToolBaseRegistry::get(const std::string& toolName) const
{
    return getPreferred(toolName, AlgLibHal);
}

std::vector<AlgLibKind> AlgToolBaseRegistry::libs(const std::string& toolName) const
{
    std::vector<AlgLibKind> out;
    std::map<std::string, std::map<AlgLibKind, AlgToolBase*> >::const_iterator it = m_dets.find(toolName);
    if (it == m_dets.end()) {
        return out;
    }
    for (std::map<AlgLibKind, AlgToolBase*>::const_iterator jt = it->second.begin(); jt != it->second.end(); ++jt) {
        out.push_back(jt->first);
    }
    return out;
}

AlgToolBaseRegistry::~AlgToolBaseRegistry()
{
    for (std::map<std::string, std::map<AlgLibKind, AlgToolBase*> >::iterator it = m_dets.begin();
         it != m_dets.end(); ++it) {
        for (std::map<AlgLibKind, AlgToolBase*>::iterator jt = it->second.begin(); jt != it->second.end(); ++jt) {
            if (jt->second) {
                jt->second->destroy();
                jt->second = 0;
            }
        }
    }
    m_dets.clear();
}

// 按工具名调算法，填图像、区域并跟随位姿
ToolOut runAlgToolBase(const std::string& toolName, const ToolIn& in, bool passImage)
{
    ToolOut out;
    AlgToolIn ain;
    ain.mode = in.mode;
    // 图像：有端口图时第一张作缺省图、其余进 images；否则用工具壳的 input
    if (!in.inputIo.images.empty()) {
        ain.imagePtr = &in.inputIo.images[0].image;
        ain.imageNative = in.inputIo.images[0].native;
        for (size_t i = 1; i < in.inputIo.images.size(); ++i) {
            ain.images.push_back(in.inputIo.images[i]);
        }
    } else {
        ain.imagePtr = &in.input;
        ain.imageNative = in.inputNative;
    }
    ain.teach = in.param.learn;
    // 搜索区与区域：工具壳已解析的直接用，否则把 rois 逐块包成 PortRoi；上游区域按用途分到 regions/masks
    if (in.ownRoisResolved) {
        ain.rois = in.ownRois;
    } else {
        for (size_t i = 0; i < in.rois.size(); ++i) {
            PortRoi n;
            n.roi = in.rois[i];
            n.index = static_cast<int>(i);
            ain.rois.push_back(n);
        }
    }
    ain.poses = plainPoses(in.inputIo.poses);
    for (size_t i = 0; i < in.inputIo.regions.size(); ++i) {
        const PortRegion& n = in.inputIo.regions[i];
        if (n.use == RegionUse::Invalid) {
            continue;
        }
        if (n.use == RegionUse::Mask) {
            ain.masks.push_back(n);
        } else {
            ain.regions.push_back(n);
        }
    }
    if (in.ownRoisResolved) {
        for (size_t i = 0; i < in.ownRegions.size(); ++i) {
            ain.regions.push_back(in.ownRegions[i]);
        }
    }
    ain.contours = in.inputIo.contours;
    ain.clouds = in.inputIo.clouds;
    ain.poses3 = in.inputIo.poses3;
    ain.shapes3 = in.inputIo.shapes3;
    ain.param = in.toolParam;
    ain.run = in.param.run;
    ain.judge = in.param.judge;
    ain.solutionDir = in.solutionDir;
    ain.toolDataDir = in.toolDataDir;
    // 工具壳未解析搜索区时，正常搜索区跟随最后一个上游位姿
    std::vector<HsCommData::HsObject> followed;
    std::vector<int> followAt;
    for (size_t i = 0; i < ain.rois.size(); ++i) {
        if (ain.rois[i].use != RegionUse::Normal) {
            continue;
        }
        followed.push_back(ain.rois[i].roi);
        followAt.push_back(static_cast<int>(i));
    }
    HsCommData::Pose2 pose;
    if (!in.ownRoisResolved && primaryAlgPose(ain.poses, &pose) && !followed.empty()) {
        applyPoseToRois(&followed, ain.run, pose);
        for (size_t i = 0; i < followAt.size(); ++i) {
            ain.rois[static_cast<size_t>(followAt[i])].roi = followed[i];
        }
    }
    // 直接用登记的原型执行，不克隆
    AlgToolBase* det = AlgToolBaseRegistry::instance().getPreferred(toolName, in.algLib);
    if (!det) {
        out.ok = false;
        out.error = true;
        out.errCode = HS_ERR_NO_ALG;
        out.image = in.input;
        out.message = "未登记图像算法";
        return out;
    }
    AlgToolOut r;
    det->shot_init(ain, &r);
    const int code = det->run(ExecMode::Auto, ain, &r);
    if (code != HS_OK && r.errCode == HS_OK) {
        r.errCode = code;
        r.ok = false;
    }
    // 算法输出抄回 ToolOut；passImage 为假或算法没出图时透传输入图
    out.ok = r.ok && r.errCode == HS_OK;
    out.errCode = r.errCode;
    out.error = r.errCode != HS_OK;
    out.message = r.message;
    out.values = r.values;
    out.objects = r.objects;
    out.traces = r.traces;
    out.infos = r.infos;
    out.labels = r.labels;
    out.io.clouds = r.clouds;
    out.io.poses3 = r.poses3;
    out.io.shapes3 = r.shapes3;
    out.io.tuples = r.tuples;
    for (size_t i = 0; i < r.poses.size(); ++i) {
        PortPose n;
        n.index = static_cast<int>(i);
        n.pose = r.poses[i];
        out.io.poses.push_back(n);
    }
    if (passImage && !r.image.image.isNull()) {
        out.image = r.image.image;
        out.imageNative = r.image.native;
    } else {
        out.image = in.input;
        out.imageNative = in.inputNative;
    }
    out.syncIo();
    return out;
}


// 第一块 ROI；空列表时返回静态空 ROI
const HsCommData::HsObject& firstAlgRoi(const std::vector<HsCommData::HsObject>& rois)
{
    static const HsCommData::HsObject kEmpty;
    return rois.empty() ? kEmpty : rois[0];
}

// 第一块正常搜索区
const HsCommData::HsObject& firstAlgRoi(const std::vector<PortRoi>& rois)
{
    static const HsCommData::HsObject kEmpty;
    for (size_t i = 0; i < rois.size(); ++i) {
        if (rois[i].use == RegionUse::Normal) {
            return rois[i].roi;
        }
    }
    return kEmpty;
}

// 最后一块位姿。空则返回 false
bool primaryAlgPose(const std::vector<HsCommData::Pose2>& poses, HsCommData::Pose2* out)
{
    if (!out || poses.empty()) {
        return false;
    }
    *out = poses.back();
    return true;
}

}
