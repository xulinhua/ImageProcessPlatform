/**
 * @file JobCfg.cpp
 * @brief JobCfg实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "JobCfg.h"
#include "ToolBase.h"
#include "AppPaths.h"
#include "Translate.h"
#include "VpUtil.h"
#include <map>
#include <string>
#include <vector>

namespace Vision {
// 从各工具输入连线汇总全流程视图（含 layout 折线）
std::vector<ToolLink> JobCfg::collectLinks() const
{
    std::vector<ToolLink> out;
    for (size_t ti = 0; ti < tools.size(); ++ti) {
        const ToolCfg& t = tools[ti];
        for (size_t li = 0; li < t.links.size(); ++li) {
            const ToolInputLink& in = t.links[li];
            ToolLink L;
            L.fromId = in.fromToolId;
            L.toId = t.id;
            L.fromPort = in.fromPort;
            L.toPort = in.toPort;
            L.index = in.index; L.use = in.use; L.origin = in.origin;
            for (size_t ri = 0; ri < layout.routes.size(); ++ri) {
                const JobRouteLayout& r = layout.routes[ri];
                if (r.toToolId == t.id && r.toPort == in.toPort) {
                    L.routeCustom = r.routeCustom;
                    L.routePoints = r.routePoints;
                    break;
                }
            }
            out.push_back(L);
        }
    }
    return out;
}

// 工具 links 写入 job.links 缓存
void JobCfg::rebuildLinksCache()
{
}

// 兼容旧名
void JobCfg::syncLinksCache()
{
    rebuildLinksCache();
}

// 用 job.links 回填各工具 links（工具已有入边则跳过该端口）
void JobCfg::distributeLinks()
{
    for (size_t i = 0; i < links.size(); ++i) {
        const ToolLink& L = links[i];
        ToolCfg* to = findTool(L.toId);
        if (!to) {
            continue;
        }
        bool exists = false;
        for (size_t j = 0; j < to->links.size(); ++j) {
            if (to->links[j].toPort == L.toPort) {
                exists = true;
                break;
            }
        }
        if (exists) {
            continue;
        }
        ToolInputLink in;
        in.fromToolId = L.fromId;
        in.fromPort = L.fromPort;
        in.toPort = L.toPort;
        in.index = L.index; in.use = L.use; in.origin = (L.origin == LinkOriginAuto) ? LinkOriginAuto : LinkOriginManual;
        to->links.push_back(in);
    }
}

// 工具画布镜像同步到 JobLayout.nodes
void JobCfg::syncLayoutFromTools()
{
    layout.nodes.clear();
    for (size_t i = 0; i < tools.size(); ++i) {
        const ToolCfg& t = tools[i];
        JobNodeLayout n;
        n.toolId = t.id;
        n.canvasX = t.canvasX;
        n.canvasY = t.canvasY;
        n.canvasPlaced = t.canvasPlaced;
        layout.nodes.push_back(n);
    }
    // 折线：从 links 缓存读 route 的写入 routes
    layout.routes.clear();
    for (size_t i = 0; i < links.size(); ++i) {
        const ToolLink& L = links[i];
        if (!L.routeCustom) {
            continue;
        }
        JobRouteLayout r;
        r.toToolId = L.toId;
        r.toPort = L.toPort;
        r.routeCustom = true;
        r.routePoints = L.routePoints;
        layout.routes.push_back(r);
    }
}

// JobLayout 写回工具画布镜像
void JobCfg::applyLayoutToTools()
{
    for (size_t i = 0; i < layout.nodes.size(); ++i) {
        const JobNodeLayout& n = layout.nodes[i];
        ToolCfg* t = findTool(n.toolId);
        if (!t) {
            continue;
        }
        t->canvasX = n.canvasX;
        t->canvasY = n.canvasY;
        t->canvasPlaced = n.canvasPlaced;
    }
    for (size_t i = 0; i < layout.routes.size(); ++i) {
        const JobRouteLayout& r = layout.routes[i];
        for (size_t j = 0; j < links.size(); ++j) {
            ToolLink& L = links[j];
            if (L.toId == r.toToolId && L.toPort == r.toPort) {
                L.routeCustom = r.routeCustom;
                L.routePoints = r.routePoints;
                break;
            }
        }
    }
}

// 指向指定输入端口的第一条连线
const ToolLink* JobCfg::findLinkToPort(const ObjectId& toId, const PortSlot& toPort) const
{
    for (int i = 0; i < static_cast<int>(links.size()); ++i) {
        const ToolLink& L = links[static_cast<size_t>(i)];
        if (L.toId == toId && L.toPort == toPort) {
            return &L;
        }
    }
    return 0;
}
// 按 id 在本 JobCfg 工具列表中查找
const ToolCfg* JobCfg::findTool(const ObjectId& id) const
{
    if (id.isNull()) {
        return 0;
    }
    for (int i = 0; i < static_cast<int>(tools.size()); ++i) {
        if (tools[static_cast<size_t>(i)].id == id) {
            return &tools[static_cast<size_t>(i)];
        }
    }
    return 0;
}

// 按 id 查找（可写）
ToolCfg* JobCfg::findTool(const ObjectId& id)
{
    return const_cast<ToolCfg*>(static_cast<const JobCfg*>(this)->findTool(id));
}

// 先按显示名、再按 toolName 查找
const ToolCfg* JobCfg::findToolByName(const std::string& name) const
{
    const std::string n = vpTrim(name);
    if (n.empty()) {
        return 0;
    }
    for (int i = 0; i < static_cast<int>(tools.size()); ++i) {
        if (tools[static_cast<size_t>(i)].name == n) {
            return &tools[static_cast<size_t>(i)];
        }
    }
    for (int i = 0; i < static_cast<int>(tools.size()); ++i) {
        if (tools[static_cast<size_t>(i)].toolName == n) {
            return &tools[static_cast<size_t>(i)];
        }
    }
    return 0;
}

// 按名称查找（可写）
ToolCfg* JobCfg::findToolByName(const std::string& name)
{
    return const_cast<ToolCfg*>(static_cast<const JobCfg*>(this)->findToolByName(name));
}

// 按列表下标取工具
const ToolCfg* JobCfg::findToolAt(int index) const
{
    if (index < 0 || index >= static_cast<int>(tools.size())) {
        return 0;
    }
    return &tools[static_cast<size_t>(index)];
}

// 按列表下标取工具（可写）
ToolCfg* JobCfg::findToolAt(int index)
{
    return const_cast<ToolCfg*>(static_cast<const JobCfg*>(this)->findToolAt(index));
}

// 工具 id 在列表中的下标，找不到为 -1
int JobCfg::indexOf(const ObjectId& id) const
{
    if (id.isNull()) {
        return -1;
    }
    for (int i = 0; i < static_cast<int>(tools.size()); ++i) {
        if (tools[static_cast<size_t>(i)].id == id) {
            return i;
        }
    }
    return -1;
}

// 叶子：挂了工具且无子节点
static bool isToolLeaf(const ToolGroup& g)
{
    return !g.toolId.isNull() && g.children.empty();
}

// 为工具建一个顺序组叶子
static ToolGroup makeToolLeaf(const ToolCfg& t)
{
    ToolGroup g;
    g.id = ObjectId::create();
    g.kind = GroupSequence;
    g.name = t.name;
    g.toolId = t.id;
    return g;
}

// 深度优先收集组内全部 toolId
static void collectToolIds(const ToolGroup& g, std::vector<ObjectId>* ids)
{
    if (!g.toolId.isNull()) {
        ids->push_back(g.toolId);
    }
    for (int i = 0; i < static_cast<int>(g.children.size()); ++i) {
        collectToolIds(g.children[static_cast<size_t>(i)], ids);
    }
}

// 组内全部 toolId
std::vector<ObjectId> ToolGroup::toolIds() const
{
    std::vector<ObjectId> ids;
    collectToolIds(*this, &ids);
    return ids;
}

// 并行或条件组：子节点彼此不串行
static bool isSplitKind(GroupKind k)
{
    return k == GroupParallel || k == GroupIf;
}

// 条件组补 Then/Else 两支，并递归子树
static void ensureIfShape(ToolGroup* g)
{
    if (!g) {
        return;
    }
    if (g->kind == GroupIf && g->toolId.isNull()) {
        while (static_cast<int>(g->children.size()) < 2) {
            ToolGroup br;
            br.id = ObjectId::create();
            br.kind = GroupSequence;
            br.name = g->children.empty()
                ? std::string(Translate::tr("Then"))
                : std::string(Translate::tr("Else"));
            g->children.push_back(br);
        }
        if (vpTrim(g->children[0].name).empty()) {
            g->children[0].name = std::string(Translate::tr("Then"));
        }
        if (vpTrim(g->children[1].name).empty()) {
            g->children[1].name = std::string(Translate::tr("Else"));
        }
    }
    for (int i = 0; i < static_cast<int>(g->children.size()); ++i) {
        ensureIfShape(&g->children[static_cast<size_t>(i)]);
    }
}

// 线性查 id 是否在列表中
static bool containsToolId(const std::vector<ObjectId>& ids, const ObjectId& id)
{
    for (int i = 0; i < static_cast<int>(ids.size()); ++i) {
        if (ids[static_cast<size_t>(i)] == id) {
            return true;
        }
    }
    return false;
}

// 按组 id 深度优先查找
static ToolGroup* findGroupIn(ToolGroup* g, const ObjectId& id)
{
    if (!g) {
        return 0;
    }
    if (g->id == id) {
        return g;
    }
    for (int i = 0; i < static_cast<int>(g->children.size()); ++i) {
        ToolGroup* f = findGroupIn(&g->children[static_cast<size_t>(i)], id);
        if (f) {
            return f;
        }
    }
    return 0;
}

// 按 toolId 找叶子组
static ToolGroup* findLeafIn(ToolGroup* g, const ObjectId& toolId)
{
    if (!g) {
        return 0;
    }
    if (g->toolId == toolId) {
        return g;
    }
    for (int i = 0; i < static_cast<int>(g->children.size()); ++i) {
        ToolGroup* f = findLeafIn(&g->children[static_cast<size_t>(i)], toolId);
        if (f) {
            return f;
        }
    }
    return 0;
}

// 找直接包含 childId 的父组
static ToolGroup* findParentIn(ToolGroup* g, const ObjectId& childId)
{
    if (!g) {
        return 0;
    }
    for (int i = 0; i < static_cast<int>(g->children.size()); ++i) {
        if (g->children[static_cast<size_t>(i)].id == childId) {
            return g;
        }
        ToolGroup* f = findParentIn(&g->children[static_cast<size_t>(i)], childId);
        if (f) {
            return f;
        }
    }
    return 0;
}

// 子节点在父组中的下标
static int childIndexOf(const ToolGroup& p, const ObjectId& id)
{
    for (int i = 0; i < static_cast<int>(p.children.size()); ++i) {
        if (p.children[static_cast<size_t>(i)].id == id) {
            return i;
        }
    }
    return -1;
}

// JobCfg 工具列表是否仍有该 id
static bool toolExists(const JobCfg& job, const ObjectId& id)
{
    return job.indexOf(id) >= 0;
}

// 自底向上删掉工具已不存在的叶子
static void pruneMissingLeaves(ToolGroup* g, const JobCfg& job)
{
    if (!g) {
        return;
    }
    for (int i = static_cast<int>(g->children.size()) - 1; i >= 0; --i) {
        pruneMissingLeaves(&g->children[static_cast<size_t>(i)], job);
        if (isToolLeaf(g->children[static_cast<size_t>(i)]) && !toolExists(job, g->children[static_cast<size_t>(i)].toolId)) {
            vpRemoveAt(g->children, i);
        }
    }
}

// 在 JobCfg 树中按组 id 查找
ToolGroup* JobCfg::findGroup(const ObjectId& id)
{
    if (id.isNull()) {
        return 0;
    }
    return findGroupIn(&root, id);
}

// 在 JobCfg 树中按 toolId 找叶子
ToolGroup* JobCfg::findLeaf(const ObjectId& toolId)
{
    if (toolId.isNull()) {
        return 0;
    }
    return findLeafIn(&root, toolId);
}

// 找节点的父组
ToolGroup* JobCfg::findParent(const ObjectId& childId)
{
    if (childId.isNull()) {
        return 0;
    }
    return findParentIn(&root, childId);
}

// 补根组、剪失效叶子、补 If 形状，并把未入树的工具挂到根
void JobCfg::ensureTree()
{
    if (root.id.isNull()) {
        root.id = ObjectId::create();
        root.kind = GroupSequence;
        root.name = std::string(Translate::tr("流程"));
    }
    pruneMissingLeaves(&root, *this);
    ensureIfShape(&root);
    std::vector<ObjectId> ids;
    collectToolIds(root, &ids);
    for (int i = 0; i < static_cast<int>(tools.size()); ++i) {
        if (!containsToolId(ids, tools[static_cast<size_t>(i)].id)) {
            root.children.push_back(makeToolLeaf(tools[static_cast<size_t>(i)]));
            ids.push_back(tools[static_cast<size_t>(i)].id);
        }
    }
}

struct GroupEnds {
    std::vector<ObjectId> firsts;
    std::vector<ObjectId> lasts;
};

// 按组类型生成串行/并行连线，并返回该组入口、出口工具
static GroupEnds emitGroupLinks(const ToolGroup& g, std::vector<ToolLink>* links)
{
    GroupEnds e;
    if (isToolLeaf(g)) {
        e.firsts.push_back(g.toolId);
        e.lasts.push_back(g.toolId);
        return e;
    }
    if (isSplitKind(g.kind)) {
        // 并行/条件：各支独立，入口出口并集
        for (int i = 0; i < static_cast<int>(g.children.size()); ++i) {
            const GroupEnds c = emitGroupLinks(g.children[static_cast<size_t>(i)], links);
            for (int k = 0; k < static_cast<int>(c.firsts.size()); ++k) {
                e.firsts.push_back(c.firsts[static_cast<size_t>(k)]);
            }
            for (int k = 0; k < static_cast<int>(c.lasts.size()); ++k) {
                e.lasts.push_back(c.lasts[static_cast<size_t>(k)]);
            }
        }
        return e;
    }
    // sequence group: connect previous branch to next
    bool hasPrev = false;
    GroupEnds prev;
    for (int i = 0; i < static_cast<int>(g.children.size()); ++i) {
        const GroupEnds c = emitGroupLinks(g.children[static_cast<size_t>(i)], links);
        if (c.firsts.empty() && c.lasts.empty()) {
            continue;
        }
        if (!hasPrev) {
            e.firsts = c.firsts;
            hasPrev = true;
        } else {
            for (int a = 0; a < static_cast<int>(prev.lasts.size()); ++a) {
                for (int b = 0; b < static_cast<int>(c.firsts.size()); ++b) {
                    ToolLink L;
                    L.fromId = prev.lasts[static_cast<size_t>(a)];
                    L.toId = c.firsts[static_cast<size_t>(b)];
                    links->push_back(L);
                }
            }
        }
        prev = c;
        e.lasts = c.lasts;
    }
    return e;
}

// 按 id 取工具记录
static const ToolCfg* ToolCfgOf(const JobCfg& job, const ObjectId& id)
{
    return job.findTool(id);
}

// 按上下游类型展开端口：图像、位姿、区域、逻辑可同时存在（仅示例方案初值）
static void fillLinkPorts(JobCfg* job)
{
    std::vector<ToolLink> out;
    for (int i = 0; i < static_cast<int>(job->links.size()); ++i) {
        const ToolLink& src = job->links[static_cast<size_t>(i)];
        const ToolCfg* from = ToolCfgOf(*job, src.fromId);
        const ToolCfg* to = ToolCfgOf(*job, src.toId);
        if (!to) {
            continue;
        }
        if (from && toolTypeIsImageSource(from->type)
            && to->type != ToolType::Logic && to->type != ToolType::Comm) {
            ToolLink L = src;
            L.fromPort = portSlotOut(PortData::Image);
            L.toPort = portSlotIn(PortData::Image);
            out.push_back(L);
        }
        if (from && from->type == ToolType::Locate
            && (to->type == ToolType::Locate || to->type == ToolType::Detect
                || to->type == ToolType::RstProcess)) {
            ToolLink L = src;
            L.fromPort = portSlotOut(PortData::Pose);
            L.toPort = portSlotIn(PortData::Pose);
            out.push_back(L);
        }
        if (from && (from->type == ToolType::Locate || from->type == ToolType::Detect
                     || from->type == ToolType::RstProcess)
            && (to->type == ToolType::RstProcess || to->type == ToolType::Detect)) {
            ToolLink L = src;
            L.fromPort = portSlotOut(PortData::Region);
            L.toPort = portSlotIn(PortData::Region);
            out.push_back(L);
        }
        if (to->type == ToolType::Logic || to->type == ToolType::Comm) {
            ToolLink L = src;
            L.fromPort = portSlotOut(PortData::Logic);
            L.toPort = portSlotIn(PortData::Logic);
            out.push_back(L);
        }
    }
    job->links = out;
    for (size_t ti = 0; ti < job->tools.size(); ++ti) {
        job->tools[ti].links.clear();
    }

}

// 该输入端口是否已有来线
static bool hasLinkToPort(const std::vector<ToolLink>& links, const ObjectId& toId, const PortSlot& toPort)
{
    for (int i = 0; i < static_cast<int>(links.size()); ++i) {
        if (links[static_cast<size_t>(i)].toId == toId
            && links[static_cast<size_t>(i)].toPort == toPort) {
            return true;
        }
    }
    return false;
}

// 工具是否出现在旧连线里（用来判断是不是刚插入的新工具）
static bool toolInLinks(const std::vector<ToolLink>& links, const ObjectId& id)
{
    for (int i = 0; i < static_cast<int>(links.size()); ++i) {
        if (links[static_cast<size_t>(i)].fromId == id || links[static_cast<size_t>(i)].toId == id) {
            return true;
        }
    }
    return false;
}

// 树改结构后：保留用户拖的数据边；只给全新工具补默认图像入（只补缺失，不覆盖 manual）
void JobCfg::rebuildLinksFromTree()
{
    // ensure tree matches tools
    if (links.empty()) {
        links = collectLinks();
    }

    const std::vector<ToolLink> old = links;
    std::vector<ToolLink> adj;
    emitGroupLinks(root, &adj);

    std::vector<ToolLink> out = old;
    for (int i = 0; i < static_cast<int>(adj.size()); ++i) {
        const ToolLink& a = adj[static_cast<size_t>(i)];
        const ToolCfg* from = findTool(a.fromId);
        const ToolCfg* to = findTool(a.toId);
        if (!to || toolInLinks(old, a.toId)) {
            continue;
        }
        if (to->type != ToolType::Logic && to->type != ToolType::Comm
            && !hasLinkToPort(out, a.toId, portSlotIn(PortData::Image))) {
            ToolLink L = a;
            if (from && toolTypeIsImageSource(from->type)) {
                L.fromPort = portSlotOut(PortData::Image);
                L.toPort = portSlotIn(PortData::Image);
                L.origin = LinkOriginAuto;
                out.push_back(L);
            }
        }
        if (from && from->type == ToolType::Locate
            && (to->type == ToolType::Locate || to->type == ToolType::Detect
                || to->type == ToolType::RstProcess)
            && !hasLinkToPort(out, a.toId, portSlotIn(PortData::Pose))) {
            ToolLink L = a;
            L.fromPort = portSlotOut(PortData::Pose);
            L.toPort = portSlotIn(PortData::Pose);
            L.origin = LinkOriginAuto;
            out.push_back(L);
        }
        if (from && (from->type == ToolType::Locate || from->type == ToolType::Detect)
            && (to->type == ToolType::Detect || to->type == ToolType::RstProcess)
            && !hasLinkToPort(out, a.toId, portSlotIn(PortData::Region))) {
            ToolLink L = a;
            L.fromPort = portSlotOut(PortData::Region);
            L.toPort = portSlotIn(PortData::Region);
            L.origin = LinkOriginAuto;
            out.push_back(L);
        }
        if ((to->type == ToolType::Logic || to->type == ToolType::Comm)
            && !hasLinkToPort(out, a.toId, portSlotIn(PortData::Logic))) {
            ToolLink L = a;
            L.fromPort = portSlotOut(PortData::Logic);
            L.toPort = portSlotIn(PortData::Logic);
            L.origin = LinkOriginAuto;
            out.push_back(L);
        }
    }
    // cleanup invalid edges
    std::vector<ToolLink> cleaned;
    for (size_t i = 0; i < out.size(); ++i) {
        const ToolLink& L = out[i];
        if (L.fromId == L.toId) {
            continue;
        }
        if (!findTool(L.fromId) || !findTool(L.toId)) {
            continue;
        }
        cleaned.push_back(L);
    }
        links = cleaned;
    for (size_t ti = 0; ti < tools.size(); ++ti) {
        tools[ti].links.clear();
    }
}
bool JobCfg::connectPorts(const ObjectId& fromId, const PortSlot& fromPort,
                      const ObjectId& toId, const PortSlot& toPort, std::string* err)
{
    if (fromId == toId) {
        if (err) {
            *err = std::string(Translate::tr("找不到工具"));
        }
        return false;
    }
    if (!findTool(fromId) || !findTool(toId)) {
        if (err) {
            *err = std::string(Translate::tr("找不到工具"));
        }
        return false;
    }
    if (fromPort.dir != PortDir::Out || toPort.dir != PortDir::In) {
        if (err) {
            *err = std::string(Translate::tr("找不到工具"));
        }
        return false;
    }
    if (!portsTypeMatch(fromPort, toPort)) {
        if (err) {
            std::string msg = std::string(Translate::tr("类型不同，不能连接："));
            msg += portDataTitle(fromPort.data);
            msg += " -> ";
            msg += portDataTitle(toPort.data);
            *err = msg;
        }
        return false;
    }
    std::vector<ToolLink> kept;
    for (size_t i = 0; i < links.size(); ++i) {
        if (links[i].toId == toId && links[i].toPort == toPort) {
            continue;
        }
        kept.push_back(links[i]);
    }
    ToolLink edge;
    edge.fromId = fromId;
    edge.fromPort = fromPort;
    edge.toId = toId;
    edge.toPort = toPort;
    edge.origin = LinkOriginManual;
    kept.push_back(edge);
    links = kept;
    for (size_t i = 0; i < tools.size(); ++i) {
        tools[i].links.clear();
    }
    return true;
}

// 按下标删一条数据边
bool JobCfg::disconnectLink(int index)
{
    if (index < 0 || index >= static_cast<int>(links.size())) {
        return false;
    }
    links.erase(links.begin() + index);
    for (size_t i = 0; i < tools.size(); ++i) {
        tools[i].links.clear();
    }
    return true;
}

// 断开指向指定输入端口的连线
bool JobCfg::disconnectPort(const ObjectId& toId, const PortSlot& toPort)
{
    if (!findTool(toId)) {
        return false;
    }
    std::vector<ToolLink> kept;
    bool removed = false;
    for (size_t i = 0; i < links.size(); ++i) {
        if (links[i].toId == toId && links[i].toPort == toPort) {
            removed = true;
            continue;
        }
        kept.push_back(links[i]);
    }
    if (!removed) {
        return false;
    }
    links = kept;
    for (size_t i = 0; i < tools.size(); ++i) {
        tools[i].links.clear();
    }
    return true;
}

// 先保证树完整再重建连线
void JobCfg::rebuildSerialLinks()
{
    ensureTree();
    rebuildLinksFromTree();
}

// 选中项：id 优先，否则当工具叶子
static ToolGroup* resolveSelect(JobCfg* job, const ObjectId& selectId)
{
    if (selectId.isNull()) {
        return 0;
    }
    ToolGroup* g = job->findGroup(selectId);
    if (g) {
        return g;
    }
    return job->findLeaf(selectId);
}

// 在选中节点附近插入叶子：根后追加、叶子后插入、并行末支、If 的 Then
static void insertLeafNear(JobCfg* job, const ToolGroup& leaf, const ObjectId& selectId)
{
    ToolGroup* sel = resolveSelect(job, selectId);
    if (!sel || sel->id == job->root.id) {
        job->root.children.push_back(leaf);
        return;
    }
    if (isToolLeaf(*sel)) {
        ToolGroup* p = job->findParent(sel->id);
        if (!p) {
            p = &job->root;
        }
        const int idx = childIndexOf(*p, sel->id);
        if (idx >= 0) {
            vpInsertAt(p->children, idx + 1, leaf);
        } else {
            p->children.push_back(leaf);
        }
        return;
    }
    if (sel->kind == GroupParallel) {
        if (sel->children.empty()) {
            ToolGroup br;
            br.id = ObjectId::create();
            br.kind = GroupSequence;
            br.name = std::string(Translate::tr("支路1"));
            br.children.push_back(leaf);
            sel->children.push_back(br);
        } else {
            sel->children.back().children.push_back(leaf);
        }
        return;
    }
    if (sel->kind == GroupIf) {
        if (sel->children.empty()) {
            ensureIfShape(sel);
        }
        sel->children[0].children.push_back(leaf);
        return;
    }
    sel->children.push_back(leaf);
}

// 追加工具并在选中处挂叶子，再重建连线
bool JobCfg::insertTool(const ToolCfg& t, const ObjectId& selectId)
{
    ToolCfg rec = t;
    if (rec.id.isNull()) {
        rec.id = ObjectId::create();
    }
    ensureTree();
    tools.push_back(rec);
    ToolCfg* added = &tools.back();
    assignNewToolCanvasPosition( added, selectId);
    insertLeafNear(this, makeToolLeaf(*added), selectId);
    rebuildLinksFromTree();
    return true;
}

// 估算工具节点在画布上的高度（与 JobCanvas 布局常量一致）
static double estimateToolCanvasHeight(const ToolCfg* t)
{
    if (!t) {
        return 62.0;
    }
    int rows = 1;
    const std::vector<PortDesc> ports = defaultPortsForType(t->type);
    int ins = 0;
    int outs = 0;
    for (int i = 0; i < static_cast<int>(ports.size()); ++i) {
        if (ports[static_cast<size_t>(i)].dir() == PortDir::In) {
            ++ins;
        } else {
            ++outs;
        }
    }
    rows = ins > outs ? ins : outs;
    if (rows < 1) {
        rows = 1;
    }
    return 22.0 + 18.0 + 4.0 + rows * 16.0 + 6.0;
}

// 插入点是否在并行/条件组或其支路内（应走树布局，不能写 canvasPlaced）
static bool insertTargetUsesTreeLayout(JobCfg* job, const ObjectId& selectId)
{
    ToolGroup* sel = resolveSelect(job, selectId);
    if (!sel) {
        return false;
    }
    if (sel->kind == GroupParallel || sel->kind == GroupIf) {
        return true;
    }
    const ObjectId walkId = isToolLeaf(*sel) ? sel->toolId : sel->id;
    for (ToolGroup* p = job->findParent(walkId); p; p = job->findParent(p->id)) {
        if (p->kind == GroupParallel || p->kind == GroupIf) {
            return true;
        }
        if (p->kind == GroupSequence && p->id != job->root.id) {
            ToolGroup* gp = job->findParent(p->id);
            if (gp && (gp->kind == GroupParallel || gp->kind == GroupIf)) {
                return true;
            }
        }
    }
    return false;
}

// 新工具加入 JobCfg 时分配合适画布坐标
void JobCfg::assignNewToolCanvasPosition(ToolCfg* t, const ObjectId& selectId)
{
    if (!t) {
        return;
    }
    if (insertTargetUsesTreeLayout(this, selectId)) {
        t->canvasPlaced = false;
        t->canvasX = 0;
        t->canvasY = 0;
        return;
    }
    const double kX = 16.0;
    const double kTop = 32.0;
    const double kGap = 36.0;
    const ToolCfg* anchor = selectId.isNull() ? 0 : findTool(selectId);
    if (anchor && anchor->canvasPlaced && anchor->id != t->id) {
        t->canvasX = anchor->canvasX;
        t->canvasY = anchor->canvasY + estimateToolCanvasHeight(anchor) + kGap;
        t->canvasPlaced = true;
        return;
    }
    double maxBottom = kTop;
    bool anyPlaced = false;
    for (int i = 0; i < static_cast<int>(tools.size()); ++i) {
        const ToolCfg& r = tools[static_cast<size_t>(i)];
        if (r.id == t->id || !r.canvasPlaced) {
            continue;
        }
        anyPlaced = true;
        const double bottom = r.canvasY + estimateToolCanvasHeight(&r);
        if (bottom > maxBottom) {
            maxBottom = bottom;
        }
    }
    if (anyPlaced) {
        t->canvasX = kX;
        t->canvasY = maxBottom + kGap;
        t->canvasPlaced = true;
        return;
    }
    int idx = 0;
    for (int i = 0; i < static_cast<int>(tools.size()); ++i) {
        if (tools[static_cast<size_t>(i)].id == t->id) {
            idx = i;
            break;
        }
    }
    const double h = estimateToolCanvasHeight(t);
    t->canvasX = kX;
    t->canvasY = kTop + idx * (h + kGap);
    t->canvasPlaced = true;
}

// 递归清除组与其内工具的自定义画布坐标
static void clearGroupCanvasPlaced(JobCfg* job, ToolGroup* g)
{
    if (!g) {
        return;
    }
    g->canvasPlaced = false;
    g->canvasX = 0;
    g->canvasY = 0;
    if (isToolLeaf(*g)) {
        ToolCfg* t = job ? job->findTool(g->toolId) : nullptr;
        if (t) {
            t->canvasPlaced = false;
            t->canvasX = 0;
            t->canvasY = 0;
        }
        return;
    }
    for (int i = 0; i < static_cast<int>(g->children.size()); ++i) {
        clearGroupCanvasPlaced(job, &g->children[static_cast<size_t>(i)]);
    }
}

// 自动排列：清自定义坐标与连线路由，交由画布按树水平居中、自上而下排布（横平竖排 C 形线）
void JobCfg::autoLayoutCanvas()
{
    ensureTree();
    for (int i = 0; i < static_cast<int>(tools.size()); ++i) {
        ToolCfg& t = tools[static_cast<size_t>(i)];
        t.canvasPlaced = false;
        t.canvasX = 0;
        t.canvasY = 0;
    }
    clearGroupCanvasPlaced(this, &root);
    for (int i = 0; i < static_cast<int>(links.size()); ++i) {
        ToolLink& L = links[static_cast<size_t>(i)];
        L.routeCustom = false;
        L.routePoints.clear();
    }
}

// 统计某种组类型出现次数（不含叶子）
static int countKind(const ToolGroup& g, GroupKind k)
{
    int n = (g.kind == k && g.toolId.isNull()) ? 1 : 0;
    for (int i = 0; i < static_cast<int>(g.children.size()); ++i) {
        n += countKind(g.children[static_cast<size_t>(i)], k);
    }
    return n;
}

// 在选中节点附近插入组（规则同 insertLeafNear）
static ObjectId insertGroupNear(JobCfg* job, ToolGroup g, const ObjectId& selectId)
{
    const ObjectId nid = g.id;
    ToolGroup* sel = resolveSelect(job, selectId);
    if (!sel || sel->id == job->root.id) {
        job->root.children.push_back(g);
        job->rebuildLinksFromTree();
        return nid;
    }
    if (isToolLeaf(*sel)) {
        ToolGroup* p = job->findParent(sel->id);
        if (!p) {
            p = &job->root;
        }
        const int idx = childIndexOf(*p, sel->id);
        if (idx >= 0) {
            vpInsertAt(p->children, idx + 1, g);
        } else {
            p->children.push_back(g);
        }
        job->rebuildLinksFromTree();
        return nid;
    }
    if (isSplitKind(sel->kind)) {
        ToolGroup* p = job->findParent(sel->id);
        if (!p) {
            p = &job->root;
        }
        const int idx = childIndexOf(*p, sel->id);
        const int at = (idx >= 0) ? idx + 1 : static_cast<int>(p->children.size());
        vpInsertAt(p->children, at, g);
        job->rebuildLinksFromTree();
        return nid;
    }
    sel->children.push_back(g);
    job->rebuildLinksFromTree();
    return nid;
}

// 插入带两条空支路的并行组
ObjectId JobCfg::addParallelGroup(const ObjectId& selectId)
{
    ensureTree();
    ToolGroup g;
    g.id = ObjectId::create();
    g.kind = GroupParallel;
    g.name = Translate::format("并行组{0}", std::to_string(countKind(root, GroupParallel) + 1));
    ToolGroup a;
    a.id = ObjectId::create();
    a.kind = GroupSequence;
    a.name = std::string(Translate::tr("支路A"));
    ToolGroup b;
    b.id = ObjectId::create();
    b.kind = GroupSequence;
    b.name = std::string(Translate::tr("支路B"));
    g.children.push_back(a);
    g.children.push_back(b);
    return insertGroupNear(this, g, selectId);
}

// 插入 Then/Else 条件组；选中叶子时以 OK 为条件源
ObjectId JobCfg::addIfGroup(const ObjectId& selectId)
{
    ensureTree();
    ToolGroup g;
    g.id = ObjectId::create();
    g.kind = GroupIf;
    g.name = Translate::format("条件组{0}", std::to_string(countKind(root, GroupIf) + 1));
    g.condWant = CondThenOnOk;
    ToolGroup* sel = resolveSelect(this, selectId);
    if (sel && isToolLeaf(*sel)) {
        g.condSourceId = sel->toolId;
    }
    ToolGroup a;
    a.id = ObjectId::create();
    a.kind = GroupSequence;
    a.name = std::string(Translate::tr("Then"));
    ToolGroup b;
    b.id = ObjectId::create();
    b.kind = GroupSequence;
    b.name = std::string(Translate::tr("Else"));
    g.children.push_back(a);
    g.children.push_back(b);
    return insertGroupNear(this, g, selectId);
}


// 设置循环组次数；至少为 1
bool JobCfg::setLoopCount(const ObjectId& loopId, int count)
{
    ToolGroup* g = findGroup( loopId);
    if (!g || g->kind != GroupLoop) {
        return false;
    }
    g->loopCount = (count < 1) ? 1 : count;
    return true;
}

// 插入循环组（空壳，工具再往里加）
ObjectId JobCfg::addLoopGroup(const ObjectId& selectId)
{
    ensureTree();
    ToolGroup g;
    g.id = ObjectId::create();
    g.kind = GroupLoop;
    g.name = Translate::format("循环组{0}", std::to_string(countKind(root, GroupLoop) + 1));
    g.loopCount = 1;
    return insertGroupNear(this, g, selectId);
}

// 设置条件组的源工具与 Then 触发极性
bool JobCfg::setIfCondition(const ObjectId& ifId, const ObjectId& sourceId, int want)
{
    ToolGroup* g = findGroup( ifId);
    if (!g || g->kind != GroupIf) {
        return false;
    }
    g->condSourceId = sourceId;
    g->condWant = (want == CondThenOnNg) ? CondThenOnNg : CondThenOnOk;
    return true;
}

// 给并行组追加一条空顺序支路
ObjectId JobCfg::addSequenceBranch(const ObjectId& parallelId)
{
    ToolGroup* g = findGroup( parallelId);
    if (!g || g->kind != GroupParallel) {
        return ObjectId();
    }
    ToolGroup br;
    br.id = ObjectId::create();
    br.kind = GroupSequence;
    br.name = Translate::format("支路{0}", std::to_string(static_cast<int>(g->children.size()) + 1));
    g->children.push_back(br);
    rebuildLinksFromTree();
    return br.id;
}

// 在同级中交换相邻子节点；If 子节点不可调
bool JobCfg::moveTreeNode(const ObjectId& nodeId, int delta)
{
    if (delta == 0 || nodeId.isNull() || nodeId == root.id) {
        return false;
    }
    ensureTree();
    ToolGroup* p = findParent( nodeId);
    if (!p || p->kind == GroupIf) {
        return false;
    }
    const int i = childIndexOf(*p, nodeId);
    const int j = i + delta;
    if (i < 0 || j < 0 || j >= static_cast<int>(p->children.size())) {
        return false;
    }
    const ToolGroup tmp = p->children[static_cast<size_t>(i)];
    p->children[static_cast<size_t>(i)] = p->children[static_cast<size_t>(j)];
    p->children[static_cast<size_t>(j)] = tmp;
    rebuildLinksFromTree();
    return true;
}

// 重命名组（根组除外）
bool JobCfg::renameGroup(const ObjectId& id, const std::string& name)
{
    ToolGroup* g = findGroup( id);
    if (!g || g->id == root.id) {
        return false;
    }
    const std::string n = vpTrim(name);
    if (n.empty() || n == g->name) {
        return false;
    }
    g->name = n;
    return true;
}

// 删除子树对应的全部工具记录
static void removeToolsListed(JobCfg* job, const ToolGroup& g)
{
    std::vector<ObjectId> ids;
    collectToolIds(g, &ids);
    for (int i = 0; i < static_cast<int>(ids.size()); ++i) {
        const int k = job->indexOf(ids[static_cast<size_t>(i)]);
        if (k >= 0) {
            vpRemoveAt(job->tools, k);
        }
    }
}

// 删除组及其工具；If 的 Then/Else 不可删
bool JobCfg::removeGroup(const ObjectId& id)
{
    if (id.isNull() || id == root.id) {
        return false;
    }
    ensureTree();
    ToolGroup* p = findParent( id);
    if (!p || p->kind == GroupIf) {
        return false;
    }
    const int i = childIndexOf(*p, id);
    if (i < 0) {
        return false;
    }
    removeToolsListed(this, p->children[static_cast<size_t>(i)]);
    vpRemoveAt(p->children, i);
    rebuildLinksFromTree();
    return true;
}

// 深拷贝组树：叶子工具走注册表 duplicate，并记录旧→新 toolId
static ToolGroup cloneGroupTree(JobCfg* job, const ToolGroup& src, bool top, std::map<ObjectId, ObjectId>* toolMap)
{
    ToolGroup g;
    g.id = ObjectId::create();
    g.kind = src.kind;
    g.name = top ? (src.name + std::string(Translate::tr(" 副本"))) : src.name;
    g.condWant = src.condWant;
    g.condSourceId = src.condSourceId;
    g.loopCount = src.loopCount;
    if (!src.toolId.isNull()) {
        const ToolCfg* srcTool = job->findTool(src.toolId);
        if (srcTool) {
            ToolCfg t = srcTool->duplicate();
            t.name = t.name + std::string(Translate::tr(" 副本"));
            job->tools.push_back(t);
            g.toolId = t.id;
            g.name = t.name;
            if (toolMap) {
                (*toolMap)[src.toolId] = t.id;
            }
        }
    }
    for (int i = 0; i < static_cast<int>(src.children.size()); ++i) {
        g.children.push_back(cloneGroupTree(job, src.children[static_cast<size_t>(i)], false, toolMap));
    }
    return g;
}

// 把条件源 id 映射到副本工具
static void remapGroupConds(ToolGroup* g, const std::map<ObjectId, ObjectId>& map)
{
    if (!g) {
        return;
    }
    if (!g->condSourceId.isNull() && map.count(g->condSourceId) != 0) {
        g->condSourceId = vpMapValue(map, g->condSourceId, ObjectId());
    }
    for (int i = 0; i < static_cast<int>(g->children.size()); ++i) {
        remapGroupConds(&g->children[static_cast<size_t>(i)], map);
    }
}

// 复制组并插到原节点之后
ObjectId JobCfg::duplicateGroup(const ObjectId& id)
{
    if (id.isNull() || id == root.id) {
        return ObjectId();
    }
    ensureTree();
    ToolGroup* src = findGroup( id);
    ToolGroup* p = findParent( id);
    if (!src || !p || p->kind == GroupIf) {
        return ObjectId();
    }
    const int i = childIndexOf(*p, id);
    if (i < 0) {
        return ObjectId();
    }
    std::map<ObjectId, ObjectId> toolMap;
    ToolGroup copy = cloneGroupTree(this, *src, true, &toolMap);
    remapGroupConds(&copy, toolMap);
    const ObjectId nid = copy.id;
    vpInsertAt(p->children, i + 1, copy);
    rebuildLinksFromTree();
    return nid;
}

// 移动工具叶子在同级的位置
bool JobCfg::moveTool(const ObjectId& id, int delta)
{
    ensureTree();
    ToolGroup* leaf = findLeaf( id);
    if (!leaf) {
        return false;
    }
    return moveTreeNode( leaf->id, delta);
}

// 从树和工具列表中删除该工具
bool JobCfg::removeTool(const ObjectId& id)
{
    ensureTree();
    ToolGroup* leaf = findLeaf( id);
    if (leaf) {
        ToolGroup* p = findParent( leaf->id);
        if (p) {
            const int k = childIndexOf(*p, leaf->id);
            if (k >= 0) {
                vpRemoveAt(p->children, k);
            }
        }
    }
    const int i = indexOf( id);
    if (i < 0) {
        rebuildLinksFromTree();
        return false;
    }
    vpRemoveAt(tools, i);
    rebuildLinksFromTree();
    return true;
}

// 复制工具并插到原叶子附近
ObjectId JobCfg::duplicateTool(const ObjectId& id)
{
    const ToolCfg* src = findTool(id);
    if (!src) {
        return ObjectId();
    }
    ToolCfg t = src->duplicate();
    t.name = t.name + std::string(Translate::tr(" 副本"));
    ensureTree();
    tools.push_back(t);
    insertLeafNear(this, makeToolLeaf(t), id);
    rebuildLinksFromTree();
    return t.id;
}

// 同步改工具显示名与对应叶子名
bool JobCfg::renameTool(const ObjectId& id, const std::string& name)
{
    ToolCfg* t = findTool(id);
    if (!t) {
        return false;
    }
    const std::string n = vpTrim(name);
    if (n.empty() || n == t->name) {
        return false;
    }
    t->name = n;
    ToolGroup* leaf = findLeaf( id);
    if (leaf) {
        leaf->name = n;
    }
    return true;
}

// Kahn 拓扑序；有环则退回工具列表顺序
std::vector<ObjectId> JobCfg::execOrder() const
{
    std::vector<ObjectId> order;
    std::map<std::string, int> indeg;
    std::map<std::string, std::vector<std::string> > outs;
    for (int i = 0; i < static_cast<int>(tools.size()); ++i) {
        indeg[tools[static_cast<size_t>(i)].id.toString()] = 0;
    }
    for (int i = 0; i < static_cast<int>(links.size()); ++i) {
        const std::string a = links[static_cast<size_t>(i)].fromId.toString();
        const std::string b = links[static_cast<size_t>(i)].toId.toString();
        if (indeg.count(a) == 0 || indeg.count(b) == 0) {
            continue;
        }
        outs[a].push_back(b);
        indeg[b] = vpMapValue(indeg, b, 0) + 1;
    }
    std::vector<std::string> q;
    for (int i = 0; i < static_cast<int>(tools.size()); ++i) {
        const std::string id = tools[static_cast<size_t>(i)].id.toString();
        if (vpMapValue(indeg, id, 0) == 0) {
            q.push_back(id);
        }
    }
    while (!q.empty()) {
        const std::string id = q.front();
        q.erase(q.begin());
        order.push_back(ObjectId(id));
        const std::vector<std::string> next = vpMapValue(outs, id, std::vector<std::string>());
        for (int i = 0; i < static_cast<int>(next.size()); ++i) {
            const int n = vpMapValue(indeg, next[static_cast<size_t>(i)], 0) - 1;
            indeg[next[static_cast<size_t>(i)]] = n;
            if (n == 0) {
                q.push_back(next[static_cast<size_t>(i)]);
            }
        }
    }
    if (static_cast<int>(order.size()) != static_cast<int>(tools.size())) {
        order.clear();
        for (int i = 0; i < static_cast<int>(tools.size()); ++i) {
            order.push_back(tools[static_cast<size_t>(i)].id);
        }
    }
    return order;
}


// JobCfg 内可参与 OK/NG 矩阵的工具 id（拓扑执行序）
std::vector<ObjectId> JobCfg::okNgToolIds() const
{
    std::vector<ObjectId> out;
    const std::vector<ObjectId> order = execOrder();
    for (size_t i = 0; i < order.size(); ++i) {
        const ToolCfg* t = findTool(order[i]);
        if (t && t->producesOkNg()) {
            out.push_back(order[i]);
        }
    }
    return out;
}

// 清理结果规则无效引用；resultFromRuleId 失效时改为最后一行或清空
void JobCfg::normalizeRstRules()
{
    bool hasFrom = false;
    for (size_t ri = 0; ri < resultRules.size(); ++ri) {
        RstRule& rule = resultRules[ri];
        if (!rule.toolId.isNull() && !findTool(rule.toolId)) {
            rule.toolId = ObjectId();
        }
        for (size_t ti = resultRules[ri].terms.size(); ti > 0; --ti) {
            const RstRuleTerm& term = resultRules[ri].terms[ti - 1];
            if (term.kind == RstTermTool) {
                if (!findTool(term.sourceId)) {
                    resultRules[ri].terms.erase(resultRules[ri].terms.begin()
                                                     + static_cast<int>(ti - 1));
                }
            } else if (term.kind == RstTermRule) {
                bool found = false;
                for (size_t rj = 0; rj < resultRules.size(); ++rj) {
                    if (resultRules[rj].id == term.sourceId) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    resultRules[ri].terms.erase(resultRules[ri].terms.begin()
                                                     + static_cast<int>(ti - 1));
                }
            }
        }
        if (rule.id == resultFromRuleId) {
            hasFrom = true;
        }
    }
    if (!resultFromRuleId.isNull() && !hasFrom) {
        if (!resultRules.empty()) {
            resultFromRuleId = resultRules.back().id;
        } else {
            resultFromRuleId = ObjectId();
        }
    }
}

// 读规则项三态；无则 Ignore
int RstRule::termState(int kind, const ObjectId& sourceId) const
{
    for (size_t i = 0; i < terms.size(); ++i) {
        if (terms[i].kind == kind && terms[i].sourceId == sourceId) {
            return terms[i].state;
        }
    }
    return RstTermIgnore;
}

// 写规则项三态；Ignore 则移除项
void RstRule::setTermState(int kind, const ObjectId& sourceId, int state)
{
    if (sourceId.isNull()) {
        return;
    }
    for (size_t i = 0; i < terms.size(); ++i) {
        if (terms[i].kind == kind && terms[i].sourceId == sourceId) {
            if (state == RstTermIgnore) {
                terms.erase(terms.begin() + static_cast<int>(i));
            } else {
                terms[i].state = state;
            }
            return;
        }
    }
    if (state == RstTermIgnore) {
        return;
    }
    RstRuleTerm term;
    term.kind = kind;
    term.sourceId = sourceId;
    term.state = state;
    terms.push_back(term);
}

// 求单行规则：非 Ignore 用 AND；未运行工具忽略；全 Ignore 算 OK
static bool evalSingleRstRule(const RstRule& rule,
                                 const std::map<ObjectId, bool>& toolOk,
                                 const std::map<ObjectId, bool>& toolRan,
                                 const std::map<ObjectId, bool>& ruleOk,
                                 std::string* detail)
{
    bool rowOk = true;
    bool any = false;
    std::string parts;
    for (size_t i = 0; i < rule.terms.size(); ++i) {
        const RstRuleTerm& term = rule.terms[i];
        if (term.state == RstTermIgnore) {
            continue;
        }
        bool srcOk = false;
        if (term.kind == RstTermTool) {
            if (toolRan.find(term.sourceId) == toolRan.end()) {
                continue;
            }
            srcOk = vpMapValue(toolOk, term.sourceId, false);
        } else {
            if (ruleOk.find(term.sourceId) == ruleOk.end()) {
                continue;
            }
            srcOk = vpMapValue(ruleOk, term.sourceId, false);
        }
        any = true;
        const bool pass = (term.state == RstTermRequireOk) ? srcOk : !srcOk;
        if (!pass) {
            rowOk = false;
        }
        if (!parts.empty()) {
            parts.push_back(' ');
        }
        parts += pass ? "OK" : "NG";
    }
    if (!any) {
        rowOk = true;
    }
    if (detail) {
        *detail = parts.empty() ? std::string(Translate::tr("尚无判定项，默认 OK")) : parts;
    }
    return rowOk;
}

// 按行顺序求值并返回指定 OK/NG
bool JobCfg::evalRstRules(
                        const std::map<ObjectId, bool>& toolOk,
                        const std::map<ObjectId, bool>& toolRan,
                        const ObjectId& fromRuleId,
                        bool* outOk,
                        std::string* outMessage)
{
    if (!outOk || fromRuleId.isNull() || resultRules.empty()) {
        return false;
    }
    std::map<ObjectId, bool> ruleOk;
    std::map<ObjectId, std::string> ruleDetail;
    for (size_t i = 0; i < resultRules.size(); ++i) {
        const RstRule& rule = resultRules[i];
        std::string detail;
        const bool ok = evalSingleRstRule(rule, toolOk, toolRan, ruleOk, &detail);
        ruleOk[rule.id] = ok;
        ruleDetail[rule.id] = detail;
    }
    if (ruleOk.find(fromRuleId) == ruleOk.end()) {
        return false;
    }
    *outOk = ruleOk[fromRuleId];
    if (outMessage) {
        for (size_t i = 0; i < resultRules.size(); ++i) {
            if (resultRules[i].id == fromRuleId) {
                *outMessage = resultRules[i].name + ": "
                    + vpMapValue(ruleDetail, fromRuleId, std::string());
                break;
            }
        }
    }
    return true;
}

}
