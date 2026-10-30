/**
 * @file Solution.cpp
 * @brief Solution实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "ToolBase.h"
#include "Solution.h"
#include "ParamKeys.h"
#include "Translate.h"
#include "VpUtil.h"
#include "AppPaths.h"
#include <HsCommData/Graphic.h>
#include <cstdio>
#include <map>
#include <string>

namespace Vision {

// 按 id 查找工站
StationCfg* Solution::findStationConfig(const ObjectId& id)
{
    for (int i = 0; i < static_cast<int>(stations.size()); ++i) {
        if (stations[static_cast<size_t>(i)].id == id) {
            return &stations[static_cast<size_t>(i)];
        }
    }
    return 0;
}

// 按 id 查找工站（只读）
const StationCfg* Solution::findStationConfig(const ObjectId& id) const
{
    for (int i = 0; i < static_cast<int>(stations.size()); ++i) {
        if (stations[static_cast<size_t>(i)].id == id) {
            return &stations[static_cast<size_t>(i)];
        }
    }
    return 0;
}

// 在全部工站的 JobCfg 中按 id 查找
JobCfg* Solution::findJobConfig(const ObjectId& id)
{
    for (int i = 0; i < static_cast<int>(stations.size()); ++i) {
        std::vector<JobCfg>& jobs = stations[static_cast<size_t>(i)].jobs;
        for (int j = 0; j < static_cast<int>(jobs.size()); ++j) {
            if (jobs[static_cast<size_t>(j)].id == id) {
                return &jobs[static_cast<size_t>(j)];
            }
        }
    }
    return 0;
}

// 在全部 JobCfg 中按 id 查找工具
ToolCfg* Solution::findTool(const ObjectId& id)
{
    for (int i = 0; i < static_cast<int>(stations.size()); ++i) {
        std::vector<JobCfg>& jobs = stations[static_cast<size_t>(i)].jobs;
        for (int j = 0; j < static_cast<int>(jobs.size()); ++j) {
            ToolCfg* t = jobs[static_cast<size_t>(j)].findTool(id);
            if (t) {
                return t;
            }
        }
    }
    return 0;
}

// 在全部 JobCfg 中按 id 查找工具（只读）
const ToolCfg* Solution::findTool(const ObjectId& id) const
{
    for (int i = 0; i < static_cast<int>(stations.size()); ++i) {
        const std::vector<JobCfg>& jobs = stations[static_cast<size_t>(i)].jobs;
        for (int j = 0; j < static_cast<int>(jobs.size()); ++j) {
            const ToolCfg* t = jobs[static_cast<size_t>(j)].findTool(id);
            if (t) {
                return t;
            }
        }
    }
    return 0;
}

// 按 id 查找硬件设备
HardwareDevice* Solution::findDevice(const ObjectId& id)
{
    for (int i = 0; i < static_cast<int>(inventory.size()); ++i) {
        if (inventory[static_cast<size_t>(i)].id == id) {
            return &inventory[static_cast<size_t>(i)];
        }
    }
    return 0;
}

// 按 id 查找硬件设备（只读）
const HardwareDevice* Solution::findDevice(const ObjectId& id) const
{
    for (int i = 0; i < static_cast<int>(inventory.size()); ++i) {
        if (inventory[static_cast<size_t>(i)].id == id) {
            return &inventory[static_cast<size_t>(i)];
        }
    }
    return 0;
}

// 按 id 查找通信通道
CommChannel* Solution::findCommChannel(const ObjectId& id)
{
    for (int i = 0; i < static_cast<int>(commChannels.size()); ++i) {
        if (commChannels[static_cast<size_t>(i)].id == id) {
            return &commChannels[static_cast<size_t>(i)];
        }
    }
    return 0;
}

// 按 id 查找通信通道（只读）
const CommChannel* Solution::findCommChannel(const ObjectId& id) const
{
    for (int i = 0; i < static_cast<int>(commChannels.size()); ++i) {
        if (commChannels[static_cast<size_t>(i)].id == id) {
            return &commChannels[static_cast<size_t>(i)];
        }
    }
    return 0;
}

// 找出工具所属 JobCfg，可选写出所属工站
JobCfg* Solution::findJobCfgOfTool(const ObjectId& toolId, StationCfg** stationOut)
{
    if (stationOut) {
        *stationOut = nullptr;
    }
    for (int i = 0; i < static_cast<int>(stations.size()); ++i) {
        std::vector<JobCfg>& jobs = stations[static_cast<size_t>(i)].jobs;
        for (int j = 0; j < static_cast<int>(jobs.size()); ++j) {
            if (jobs[static_cast<size_t>(j)].findTool(toolId)) {
                if (stationOut) {
                    *stationOut = &stations[static_cast<size_t>(i)];
                }
                return &jobs[static_cast<size_t>(j)];
            }
        }
    }
    return 0;
}

// 查找流程所属工站
StationCfg* Solution::findStationCfgOfJob(const ObjectId& jobId)
{
    for (int i = 0; i < static_cast<int>(stations.size()); ++i) {
        std::vector<JobCfg>& jobs = stations[static_cast<size_t>(i)].jobs;
        for (int j = 0; j < static_cast<int>(jobs.size()); ++j) {
            if (jobs[static_cast<size_t>(j)].id == jobId) {
                return &stations[static_cast<size_t>(i)];
            }
        }
    }
    return 0;
}

// 第一个工站，无则 0
StationCfg* Solution::primaryStation()
{
    if (stations.empty()) {
        return 0;
    }
    return &stations[0];
}

// 绘制几何写回分区记录，引用条保留
static void syncRoiItemsFromRois(ToolCfg* t)
{
    if (!t) {
        return;
    }
    // 绘制几何归入第一个允许绘制的分区
    std::string slotName = "roi";
    const ToolCaps caps = t->caps();
    for (size_t s = 0; s < caps.slotRules.size(); ++s) {
        const RoiSlotRule& slot = caps.slotRules[s];
        bool draw = slot.itemKinds.empty();
        for (size_t k = 0; k < slot.itemKinds.size(); ++k) {
            if (slot.itemKinds[k] == RoiItemKind::DrawRoi || slot.itemKinds[k] == RoiItemKind::DrawRegion) {
                draw = true;
            }
        }
        if (draw) {
            slotName = slot.slot.empty() ? std::string("roi") : slot.slot;
            break;
        }
    }
    // 拆出旧绘制项与引用项；引用项去重，丢弃无来源的
    std::vector<ToolRoiItem> oldDraw;
    std::vector<ToolRoiItem> refs;
    for (size_t i = 0; i < t->param.roiItems.size(); ++i) {
        const ToolRoiItem& item = t->param.roiItems[i];
        if (item.kind == RoiItemKind::RefRoi || item.kind == RoiItemKind::RefRegion) {
            if (item.refToolId.isNull()) {
                continue;
            }
            bool dup = false;
            for (size_t d = 0; d < refs.size(); ++d) {
                if (refs[d].slot == item.slot && refs[d].refToolId == item.refToolId && refs[d].refIndex == item.refIndex) {
                    dup = true;
                    break;
                }
            }
            if (!dup) {
                refs.push_back(item);
            }
        } else {
            oldDraw.push_back(item);
        }
    }
    ObjectId poseId;
    bool hasPose = false;
    for (size_t i = 0; i < t->links.size(); ++i) {
        const ToolInputLink& link = t->links[i];
        if (link.toPort.dir == PortDir::In && link.toPort.data == PortData::Pose && !link.fromToolId.isNull()) {
            poseId = link.fromToolId;
            hasPose = true;
        }
    }
    std::vector<ToolRoiItem> next;
    for (size_t i = 0; i < refs.size(); ++i) {
        next.push_back(refs[i]);
    }
    refs.clear();
    // 按下标复用旧绘制项属性；新增的若有位姿连线则默认跟随该位姿
    for (size_t i = 0; i < t->param.rois.size(); ++i) {
        ToolRoiItem item;
        if (i < oldDraw.size()) {
            item = oldDraw[i];
        } else if (hasPose) {
            item.followPose = true;
            item.poseToolId = poseId;
        }
        if (item.slot.empty()) {
            item.slot = slotName;
        }
        if (t->param.rois[i].kind() == HsCommData::ObjRegion) {
            item.kind = RoiItemKind::DrawRegion;
        } else if (item.kind != RoiItemKind::DrawRoi && item.kind != RoiItemKind::DrawRegion) {
            item.kind = RoiItemKind::DrawRoi;
        }
        item.shape = t->param.rois[i];
        next.push_back(item);
    }
    t->param.roiItems = next;
}
// 两点轮廓收成直线
void ToolCfg::syncRois()
{
        if (preferredRoiKind() == static_cast<int>(HsCommData::ObjLine)) {
    for (size_t i = 0; i < rois.size(); ++i) {
        HsCommData::HsObject& r = rois[i];
        if (!r.isInitialized() || r.kind() != HsCommData::ObjContour) {
            continue;
        }
        const HsCommData::HsContour* c = r.contourAt(0);
        if (!c || c->ptCount() != 2) {
            continue;
        }
        HsCommData::LineP ln;
        ln.a = c->ptAt(0);
        ln.b = c->ptAt(1);
        r = ln;
    }
    }
    syncRoiItemsFromRois(this);
}

// 实际使用的 ROI 列表
std::vector<HsCommData::HsObject> ToolCfg::effectiveRois() const
{
    return rois;
}

// 查注册表取工具能力
ToolCaps ToolCfg::capsFor(const std::string& toolName)
{
    ToolBase* impl = ToolRegistry::instance().find(ToolCfg::canonicalize(toolName));
    if (impl) {
        return impl->capabilities();
    }
    return ToolCaps();
}



// 运行画面：行与列夹到 1..3，窗格始终保持 9 格以免切换布局丢配置
// 格子数
int RuntimeView::slotCount() const
{
    return rows * cols;
}

void RuntimeView::normalize()
{
    if (rows < 1) {
        rows = 1;
    }
    if (rows > 3) {
        rows = 3;
    }
    if (cols < 1) {
        cols = 1;
    }
    if (cols > 3) {
        cols = 3;
    }
    while (static_cast<int>(panes.size()) < 9) {
        RuntimePane pane;
        pane.overlayToolIds.push_back(ObjectId());
        panes.push_back(pane);
    }
    if (static_cast<int>(panes.size()) > 9) {
        panes.resize(static_cast<size_t>(9));
    }
}

// 用注册表默认参数补齐缺失项，并同步工具类型
void ToolCfg::ensureDefaults()
{
        toolName = ToolCfg::canonicalize(toolName);
    ToolBase* impl = ToolRegistry::instance().find(toolName);
    if (!impl) {
        return;
    }
    type = impl->type();
    const ParamMap d = impl->defaultParams();
    const std::vector<std::string> keys = d.keys();
    for (int i = 0; i < static_cast<int>(keys.size()); ++i) {
        if (!params.contains(keys[static_cast<size_t>(i)])) {
            params.insert(keys[static_cast<size_t>(i)], d.value(keys[static_cast<size_t>(i)]));
        }
    }
}

// 插件未登记时按名称前缀推断工具大类
static ToolType typeFromToolName(const std::string& key)
{
    if (key.compare(0, 11, "preprocess.") == 0) {
        return ToolType::Preprocess;
    }
    if (key.compare(0, 7, "locate.") == 0) {
        return ToolType::Locate;
    }
    if (key.compare(0, 7, "detect.") == 0) {
        return ToolType::Detect;
    }
    if (key.compare(0, 6, "logic.") == 0) {
        return ToolType::Logic;
    }
    if (key.compare(0, 5, "comm.") == 0) {
        return ToolType::Comm;
    }
    if (key.compare(0, 7, "result.") == 0) {
        return ToolType::RstProcess;
    }
    if (key.compare(0, 10, "acquire3d.") == 0) {
        return ToolType::Acquire3D;
    }
    if (key.compare(0, 10, "prepare3d.") == 0) {
        return ToolType::Prepare3D;
    }
    if (key.compare(0, 10, "measure3d.") == 0) {
        return ToolType::Measure3D;
    }
    if (key.compare(0, 8, "align3d.") == 0 || key.compare(0, 9, "locate3d.") == 0) {
        return ToolType::Locate3D;
    }
    if (key.compare(0, 9, "detect3d.") == 0) {
        return ToolType::Detect3D;
    }
    if (key.compare(0, 8, "calib3d.") == 0) {
        return ToolType::Calib3D;
    }
    if (key.compare(0, 6, "infer.") == 0) {
        return ToolType::Infer;
    }
    if (key.compare(0, 9, "identify.") == 0) {
        return ToolType::Identify;
    }
    return ToolType::Acquire;
}
// 按注册名创建工具配置；未登记返回空记录
ToolCfg ToolCfg::createRegistered(const std::string& toolName, const ObjectId& deviceId)
{
    ToolBase* t = ToolRegistry::instance().find(toolName);
    if (!t) {
        return ToolCfg();
    }
    ToolCfg config;
    config.id = ObjectId::create();
    const ToolDesc d = t->desc();
    config.toolName = d.toolName;
    config.type = d.type;
    config.name = d.displayName;
    config.params = t->defaultParams();
    config.enabled = true;
    config.deviceId = deviceId;
    // 三维工具缺省算法库为 PCL（采集类无算法库，保持默认即可）
    if (config.type == ToolType::Prepare3D || config.type == ToolType::Measure3D
        || config.type == ToolType::Locate3D || config.type == ToolType::Detect3D) {
        config.algLib = AlgLibPcl;
    }
    return config;
}

// 复制记录并换新 id；已摆过的节点错开一点以免叠在原处
ToolCfg ToolCfg::duplicate() const
{
    ToolCfg t = *this;
    t.id = ObjectId::create();
    t.links.clear();
    if (t.canvasPlaced) {
        t.canvasX += 28;
        t.canvasY += 28;
    }
    return t;
}

// 优先走注册表 create；未注册则手填一条可用记录
ToolCfg ToolCfg::make(const std::string& toolName, const ObjectId& deviceId)
{
    const std::string key = ToolCfg::canonicalize(toolName);
    ToolCfg t = ToolCfg::createRegistered(key, deviceId);
    if (!t.id.isNull()) {
        return t;
    }
    t.id = ObjectId::create();
    t.toolName = key;
    t.type = typeFromToolName(key);
    t.deviceId = deviceId;
    t.enabled = true;
    t.name = toolName;
    return t;
}
// 本工具的画布能力
ToolCaps ToolCfg::caps() const
{
    return ToolCfg::capsFor(toolName);
}

// 是否在图上画 ROI
bool ToolCfg::usesRoi() const
{
    return caps().usesRoi;
}

// 是否允许多 ROI
bool ToolCfg::usesMultiRoi() const
{
    return caps().usesMultiRoi();
}

// 推荐几何种类
int ToolCfg::preferredRoiKind() const
{
    return caps().preferredRoiKind();
}

// 几何种类是否与推荐一致
bool ToolCfg::roiKindMatches(int kind) const
{
    return caps().kindMatches(kind);
}

// 旧 toolName 映射到当前名
std::string ToolCfg::canonicalize(const std::string& n)
{
    if (n == "detect.find_circle") {
        return "locate.find_circle";
    }
    if (n == "detect.caliper") {
        return "locate.caliper";
    }
    if (n == "camera.file" || n == "camera.device") {
        return "camera.acquire";
    }
    if (n == "preprocess.passthrough" || n == "preprocess.gray_stretch") {
        return "preprocess.enhance";
    }
    return n;
}

// 新建空方案，不含工站
Solution Solution::makeEmpty()
{
    Solution s;
    s.name = std::string(Translate::tr("未命名方案"));
    HardwareDevice d;
    d.id = ObjectId::create();
    d.name = std::string(Translate::tr("文件夹"));
    d.kind = DeviceFolder;
    d.cameraBackend = CameraBackendFile;
    d.defaultParams.insert("folder", AppPaths::exeDir() + "/samples");
    s.inventory.push_back(d);

    s.dirty = true;
    return s;
}

}





