/**
 * @file RoiConv.cpp
 * @brief RoiConv实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "RoiConv.h"
#include "ToolCfg.h"
#include "ParamKeys.h"
#include "Translate.h"
#include <HsCommData/GeomOps.h>
#include <HsCommData/HsInterface2D.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace Vision {

namespace {

// 找线类：两点轮廓收成直线
void coerceLineRoiIfNeeded(const std::string& toolName, HsCommData::HsObject* r)
{
    if (!r || !r->isInitialized() || r->isEmpty()) {
        return;
    }
    if (ToolCfg::capsFor(toolName).preferredRoiKind() != static_cast<int>(HsCommData::ObjLine)) {
        return;
    }
    if (r->kind() != HsCommData::ObjContour) {
        return;
    }
    const HsCommData::HsContour* c = r->contourAt(0);
    if (!c || c->ptCount() != 2) {
        return;
    }
    HsCommData::LineP ln;
    ln.a = c->ptAt(0);
    ln.b = c->ptAt(1);
    *r = ln;
}

// 可绘制分区的条数上限。0 表示不限制
int drawSlotLimit(const std::string& toolName)
{
    const ToolCaps caps = ToolCfg::capsFor(toolName);
    for (size_t s = 0; s < caps.slotRules.size(); ++s) {
        const RoiSlotRule& slot = caps.slotRules[s];
        bool draw = false;
        for (size_t k = 0; k < slot.itemKinds.size(); ++k) {
            if (slot.itemKinds[k] == RoiItemKind::DrawRoi || slot.itemKinds[k] == RoiItemKind::DrawRegion) {
                draw = true;
            }
        }
        if (draw) {
            return slot.maxCount;
        }
    }
    return 1;
}

} // namespace

// 工具全部有效 ROI
std::vector<HsCommData::Graphic> ToolCfg::drawings() const
{
    std::vector<HsCommData::Graphic> o;
    const std::vector<HsCommData::HsObject> rs = effectiveRois();
    for (int i = 0; i < static_cast<int>(rs.size()); ++i) {
        if (!rs[static_cast<size_t>(i)].isInitialized() || rs[static_cast<size_t>(i)].isEmpty()) {
            continue;
        }
        HsCommData::Graphic g = roiToDrawing(rs[static_cast<size_t>(i)]);
        char buf[32];
        sprintf_s(buf, "roi.%d", i);
        g.id = buf;
        o.push_back(g);
    }
    return o;
}

// 全部 ROI 绘制层，并按选中行调节透明度
std::vector<HsCommData::Graphic> ToolCfg::drawingsForDisplay(int selectedIndex) const
{
    const std::vector<HsCommData::Graphic> all = drawings();
    std::vector<HsCommData::Graphic> out;
    out.reserve(all.size());
    for (int i = 0; i < static_cast<int>(all.size()); ++i) {
        HsCommData::Graphic g = all[static_cast<size_t>(i)];
        g.geomStyle = HsCommData::Style::drawingStyle();
        if (selectedIndex >= 0) {
            g.geomStyle.opacity = (i == selectedIndex) ? 1.0 : 0.35;
        } else {
            g.geomStyle.opacity = 0.55;
        }
        out.push_back(g);
    }
    return out;
}

// 把一笔绘制写回；多分区/多 ROI 追加到未满槽，其余替换
void ToolCfg::applyDrawnRoi(const HsCommData::Graphic& g)
{
    HsCommData::HsObject drawn = drawingToRoi(g);
    coerceLineRoiIfNeeded(toolName, &drawn);
    if (!drawn.isInitialized() || drawn.isEmpty()) {
        return;
    }
    if (!roiKindMatches(static_cast<int>(drawn.kind()))) {
        return;
    }
    RoiItemKind kind = RoiItemKind::DrawRoi;
    if (drawn.kind() == HsCommData::ObjRegion || drawn.kind() == HsCommData::ObjPoly
        || drawn.kind() == HsCommData::ObjContour) {
        kind = RoiItemKind::DrawRegion;
    }
    const std::string slot = nextRoiSlotKey();
    // 优先填同槽空绘制项
    for (size_t i = 0; i < param.roiItems.size(); ++i) {
        ToolRoiItem& it = param.roiItems[i];
        if (it.slot != slot) {
            continue;
        }
        if (it.kind == RoiItemKind::RefRoi || it.kind == RoiItemKind::RefRegion) {
            continue;
        }
        if (!it.shape.isInitialized() || it.shape.isEmpty()) {
            it.kind = kind;
            it.shape = drawn;
            rebuildRoisFromItems();
            return;
        }
    }
    if (keepsExistingRoisOnAdd()) {
        ToolRoiItem item;
        item.slot = slot;
        item.kind = kind;
        item.shape = drawn;
        appendRoiItem(item);
        return;
    }
    param.roiItems.clear();
    rois.clear();
    ToolRoiItem item;
    item.slot = slot;
    item.kind = kind;
    item.shape = drawn;
    appendRoiItem(item);
}

// 按行号更新已有 ROI
void ToolCfg::applyDrawnRoiAt(const HsCommData::Graphic& g, int index)
{
    if (index < 0) {
        return;
    }
    syncRois();
    HsCommData::HsObject r = drawingToRoi(g);
    coerceLineRoiIfNeeded(toolName, &r);
    if (!r.isInitialized() || r.isEmpty() || index >= static_cast<int>(rois.size())) {
        return;
    }
    if (!roiKindMatches(static_cast<int>(r.kind()))) {
        return;
    }
    rois[static_cast<size_t>(index)] = r;
    syncRois();
}

// 从 Drawing id（roi.N）解析行号
int roiIndexFromDrawingId(const std::string& id)
{
    if (id.size() <= 4 || id.compare(0, 4, "roi.") != 0) {
        return -1;
    }
    const char* p = id.c_str() + 4;
    if (!*p) {
        return -1;
    }
    char* end = nullptr;
    const long v = strtol(p, &end, 10);
    if (end == p || (end && *end != '\0') || v < 0 || v > 100000) {
        return -1;
    }
    return static_cast<int>(v);
}

// 清空检测框（含引用项）
void ToolCfg::clearRois()
{
    rois.clear();
    param.roiItems.clear();
}

// 删最后一个检测框（按 roiItems）
void ToolCfg::removeLastRoi()
{
    if (param.roiItems.empty()) {
        return;
    }
    param.roiItems.pop_back();
    rebuildRoisFromItems();
}

// 按 roiItems 行号删除
void ToolCfg::removeRoiAt(int index)
{
    removeRoiItemAt(index);
}

// 几何种类中文名
std::string roiKindTitle(int kind)
{
    if (kind == static_cast<int>(HsCommData::ObjCircle)) {
        return Translate::tr("圆");
    }
    if (kind == static_cast<int>(HsCommData::ObjEllipse)) {
        return Translate::tr("椭圆");
    }
    if (kind == static_cast<int>(HsCommData::ObjRect1)) {
        return Translate::tr("矩形");
    }
    if (kind == static_cast<int>(HsCommData::ObjRect2)) {
        return Translate::tr("旋转矩形");
    }
    if (kind == static_cast<int>(HsCommData::ObjLine)) {
        return Translate::tr("直线");
    }
    if (kind == static_cast<int>(HsCommData::ObjContour) || kind == static_cast<int>(HsCommData::ObjPoly)) {
        return Translate::tr("轮廓");
    }
    if (kind == static_cast<int>(HsCommData::ObjRegion)) {
        return Translate::tr("区域");
    }
    return Translate::tr("ROI");
}

// 按种类拼可读明细
std::string formatRoiDetail(const HsCommData::HsObject& roi)
{
    char buf[320];
    buf[0] = 0;
    if (!roi.isInitialized() || roi.isEmpty()) {
        return std::string(Translate::tr("ROI"));
    }
    const int kind = static_cast<int>(roi.kind());
    if (roi.kind() == HsCommData::ObjCircle) {
        const HsCommData::CircleP* c = roi.circleAt(0);
        sprintf_s(buf, "%s  X=%.1f  Y=%.1f  %s=%.1f",
                  roiKindTitle(kind).c_str(), c->c.x, c->c.y, Translate::tr("半径").c_str(), c->r);
    } else if (roi.kind() == HsCommData::ObjEllipse) {
        const HsCommData::EllipseP* e = roi.ellipseAt(0);
        sprintf_s(buf, "%s  X=%.1f  Y=%.1f  a=%.1f  b=%.1f  %s=%.1f%s",
                  roiKindTitle(kind).c_str(), e->c.x, e->c.y, e->ra, e->rb,
                  Translate::tr("角度").c_str(), HsCommData::toDegree(e->phi), Translate::tr("度").c_str());
    } else if (roi.kind() == HsCommData::ObjRect1) {
        const HsCommData::Rect1P* rc = roi.rect1At(0);
        sprintf_s(buf, "%s  X=%.1f  Y=%.1f  %s=%.1f  %s=%.1f",
                  roiKindTitle(kind).c_str(), rc->r.x, rc->r.y, Translate::tr("宽").c_str(), rc->r.w, Translate::tr("高").c_str(), rc->r.h);
    } else if (roi.kind() == HsCommData::ObjRect2) {
        const HsCommData::Rect2P* rc = roi.rect2At(0);
        sprintf_s(buf, "%s  X=%.1f  Y=%.1f  %s=%.1f  %s=%.1f  %s=%.1f%s",
                  roiKindTitle(kind).c_str(), rc->c.x, rc->c.y, Translate::tr("半长").c_str(), rc->l1, Translate::tr("半宽").c_str(), rc->l2,
                  Translate::tr("角度").c_str(), HsCommData::toDegree(rc->phi), Translate::tr("度").c_str());
    } else if (roi.kind() == HsCommData::ObjLine) {
        const HsCommData::LineP* ln = roi.lineAt(0);
        const double dx = ln->b.x - ln->a.x;
        const double dy = ln->b.y - ln->a.y;
        const double len = std::sqrt(dx * dx + dy * dy);
        sprintf_s(buf, "%s  (%.1f, %.1f)-(%.1f, %.1f)  %s=%.1f",
                  roiKindTitle(kind).c_str(), ln->a.x, ln->a.y, ln->b.x, ln->b.y, Translate::tr("长度").c_str(), len);
    } else if (roi.kind() == HsCommData::ObjContour) {
        const HsCommData::HsContour* c = roi.contourAt(0);
        sprintf_s(buf, "%s  %s=%u",
                  roiKindTitle(kind).c_str(), Translate::tr("顶点").c_str(),
                  static_cast<unsigned>(c ? c->ptCount() : 0));
    } else if (roi.kind() == HsCommData::ObjRegion) {
        const HsCommData::HsRegion* reg = roi.regionAt(0);
        int64_t area = reg ? static_cast<int64_t>(reg->area()) : 0;
        sprintf_s(buf, "%s  %s=%lld",
                  roiKindTitle(kind).c_str(), Translate::tr("面积").c_str(), static_cast<long long>(area));
    } else {
        sprintf_s(buf, "%s", roiKindTitle(kind).c_str());
    }
    return std::string(buf);
}

// 明细叠在图形下方
void roiLabelAnchor(const HsCommData::HsObject& roi, double* x, double* y)
{
    const HsCommData::Graphic g = roiToDrawing(roi);
    double ax = g.boundsImage.x;
    double ay = g.boundsImage.y + g.boundsImage.h + 4.0;
    if (x) {
        *x = ax;
    }
    if (y) {
        *y = ay;
    }
}

}
