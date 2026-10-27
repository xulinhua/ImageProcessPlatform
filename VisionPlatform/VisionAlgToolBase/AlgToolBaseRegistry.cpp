/**
 * @file AlgToolBaseRegistry.cpp
 * @brief AlgToolBaseRegistry实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "AlgToolBase.h"
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
    if (!in.inputIo.images.empty()) {
        ain.image = in.inputIo.images[0];
        for (size_t i = 1; i < in.inputIo.images.size(); ++i) {
            ain.images.push_back(in.inputIo.images[i]);
        }
    } else {
        ain.image.image = in.input;
        ain.image.native = in.inputNative;
    }
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
    ain.param = in.toolParam;
    ain.run = in.param.run;
    ain.judge = in.param.judge;
    ain.solutionDir = in.solutionDir;
    ain.toolDataDir = in.toolDataDir;
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
    AlgToolBase* det = AlgToolBaseRegistry::instance().getPreferred(toolName, in.algLib);
    if (!det) {
        out.ok = false;
        out.error = true;
        out.errCode = 1;
        out.image = in.input;
        out.message = acpToUtf8("未登记图像算法");
        return out;
    }
    const AlgToolOut r = det->run(ain);
    out.ok = r.ok;
    out.errCode = r.errCode;
    out.error = r.errCode != 0;
    out.message = r.message;
    out.values = r.values;
    out.objects = r.objects;
    out.traces = r.traces;
    out.infos = r.infos;
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
