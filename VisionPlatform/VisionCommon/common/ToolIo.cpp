/**
 * @file ToolIo.cpp
 * @brief ToolIo实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "ToolIo.h"
#include <HsCommData/Style.h>
#include <cmath>

namespace Vision {

namespace {

// 口名是否对得上。来源名为空时只收缺省口
bool portNameOk(const std::string& itemPort, const std::string& fromName)
{
    if (fromName.empty()) {
        return itemPort.empty();
    }
    return itemPort == fromName;
}

// index 小于 0 收整口，否则只收对上的第 index 个
bool indexOk(int index, int seen)
{
    return index < 0 || seen == index;
}

// 单元素图标对象收进对应数组
void keepOne(ToolIo* io, const HsCommData::HsObject& obj)
{
    if (!io || !obj.isInitialized() || obj.isEmpty()) {
        return;
    }
    const HsCommData::ObjectKind k = obj.kind();
    if (k == HsCommData::ObjRegion) {
        PortRegion n;
        n.object = obj;
        io->regions.push_back(n);
        return;
    }
    if (k == HsCommData::ObjContour || k == HsCommData::ObjPoly) {
        PortContour n;
        n.object = obj;
        io->contours.push_back(n);
        return;
    }
    if (k == HsCommData::ObjCircle || k == HsCommData::ObjEllipse
        || k == HsCommData::ObjRect1 || k == HsCommData::ObjRect2
        || k == HsCommData::ObjLine) {
        PortRoi n;
        n.roi = obj;
        io->rois.push_back(n);
    }
}

} // namespace

// 区域、轮廓、ROI、位姿转叠加，只给图像窗
std::vector<HsCommData::Graphic> ToolIo::toGraphics() const
{
    std::vector<HsCommData::Graphic> gfx;
    for (int i = 0; i < static_cast<int>(rois.size()); ++i) {
        if (rois[static_cast<size_t>(i)].use == RegionUse::Invalid) {
            continue;
        }
        HsCommData::Graphic g = roiToDrawing(rois[static_cast<size_t>(i)].roi);
        g.role = HsCommData::RoleStack;
        g.geomStyle = HsCommData::Style::stackStyle();
        gfx.push_back(g);
    }
    for (int i = 0; i < static_cast<int>(regions.size()); ++i) {
        if (regions[static_cast<size_t>(i)].use == RegionUse::Invalid) {
            continue;
        }
        gfx.push_back(objectToGraphic(regions[static_cast<size_t>(i)].object));
    }
    for (int i = 0; i < static_cast<int>(contours.size()); ++i) {
        gfx.push_back(objectToGraphic(contours[static_cast<size_t>(i)].object));
    }
    for (int i = 0; i < static_cast<int>(poses.size()); ++i) {
        const HsCommData::Pose2& p = poses[static_cast<size_t>(i)].pose;
        HsCommData::Graphic g;
        g.kind = HsCommData::KindCross;
        g.role = HsCommData::RoleStack;
        g.visible = true;
        HsCommData::HsCross cr;
        cr.center = HsCommData::Point2d(p.tx, p.ty);
        cr.size = 8.0;
        cr.angle = p.angle;
        g.cross.push_back(cr);
        g.geomStyle = HsCommData::Style::stackStyle();
        g.updateBounds();
        gfx.push_back(g);

        HsCommData::Graphic ga;
        ga.kind = HsCommData::KindArrow;
        ga.role = HsCommData::RoleStack;
        ga.visible = true;
        HsCommData::HsArrow ar;
        ar.tail = HsCommData::Point2d(p.tx, p.ty);
        const double len = 20.0;
        ar.head = HsCommData::Point2d(p.tx + len * std::cos(p.angle), p.ty + len * std::sin(p.angle));
        ga.arrow.push_back(ar);
        ga.geomStyle = HsCommData::Style::stackStyle();
        ga.updateBounds();
        gfx.push_back(ga);
    }
    return gfx;
}

// 按种类拆进区域、轮廓、ROI
void ToolIo::appendObjects(const std::vector<HsCommData::HsObject>& objs)
{
    for (size_t i = 0; i < objs.size(); ++i) {
        const HsCommData::HsObject& obj = objs[i];
        if (!obj.isInitialized() || obj.isEmpty()) {
            continue;
        }
        if (obj.kind() == HsCommData::ObjMixed) {
            const int n = obj.count();
            for (int ei = 0; ei < n; ++ei) {
                keepOne(this, obj.select(ei));
            }
            continue;
        }
        keepOne(this, obj);
    }
}

// 绘制图元里的几何拆进区域、轮廓、ROI
void ToolIo::appendFromGraphics(const std::vector<HsCommData::Graphic>& gfx)
{
    std::vector<HsCommData::HsObject> objs;
    for (int i = 0; i < static_cast<int>(gfx.size()); ++i) {
        const HsCommData::Graphic& g = gfx[static_cast<size_t>(i)];
        if (g.geom.isInitialized() && !g.geom.isEmpty()) {
            objs.push_back(g.geom);
        }
    }
    appendObjects(objs);
}

// 按连线从 src 收一项或整口，并改写到目标口名
void ToolIo::appendByType(const ToolIo& src, const PortSlot& fromPort, const PortSlot& toPort, int index, RegionUse use)
{
    // 各类数据同一规则：口名匹配后按序号挑；index < 0 收整口，否则命中一项即返回
    if (fromPort.data == PortData::Image) {
        int seen = 0;
        for (int i = 0; i < static_cast<int>(src.images.size()); ++i) {
            const PortImage& one = src.images[static_cast<size_t>(i)];
            if (!portNameOk(one.port, fromPort.name)) {
                continue;
            }
            if (!indexOk(index, seen)) {
                ++seen;
                continue;
            }
            PortImage n = one;
            n.port = toPort.name;
            images.push_back(n);
            ++seen;
            if (index >= 0) {
                return;
            }
        }
        return;
    }
    if (fromPort.data == PortData::Region) {
        int seen = 0;
        for (int i = 0; i < static_cast<int>(src.regions.size()); ++i) {
            const PortRegion& one = src.regions[static_cast<size_t>(i)];
            if (!portNameOk(one.port, fromPort.name)) {
                continue;
            }
            if (!indexOk(index, seen)) {
                ++seen;
                continue;
            }
            PortRegion n = one;
            n.port = toPort.name;
            n.use = use;
            regions.push_back(n);
            ++seen;
            if (index >= 0) {
                return;
            }
        }
        return;
    }
    if (fromPort.data == PortData::Contour) {
        int seen = 0;
        for (int i = 0; i < static_cast<int>(src.contours.size()); ++i) {
            const PortContour& one = src.contours[static_cast<size_t>(i)];
            if (!portNameOk(one.port, fromPort.name)) {
                continue;
            }
            if (!indexOk(index, seen)) {
                ++seen;
                continue;
            }
            PortContour n = one;
            n.port = toPort.name;
            contours.push_back(n);
            ++seen;
            if (index >= 0) {
                return;
            }
        }
        return;
    }
    if (fromPort.data == PortData::Roi) {
        int seen = 0;
        for (int i = 0; i < static_cast<int>(src.rois.size()); ++i) {
            const PortRoi& one = src.rois[static_cast<size_t>(i)];
            if (!portNameOk(one.port, fromPort.name)) {
                continue;
            }
            if (!indexOk(index, seen)) {
                ++seen;
                continue;
            }
            PortRoi n = one;
            n.port = toPort.name;
            n.use = use;
            rois.push_back(n);
            ++seen;
            if (index >= 0) {
                return;
            }
        }
        return;
    }
    // 逻辑结果没有口名，只按序号挑
    if (fromPort.data == PortData::Logic) {
        int seen = 0;
        for (int i = 0; i < static_cast<int>(src.logics.size()); ++i) {
            if (!indexOk(index, seen)) {
                ++seen;
                continue;
            }
            logics.push_back(src.logics[static_cast<size_t>(i)]);
            ++seen;
            if (index >= 0) {
                return;
            }
        }
        return;
    }
    if (fromPort.data == PortData::Pose) {
        int seen = 0;
        for (int i = 0; i < static_cast<int>(src.poses.size()); ++i) {
            const PortPose& one = src.poses[static_cast<size_t>(i)];
            if (!portNameOk(one.port, fromPort.name)) {
                continue;
            }
            if (!indexOk(index, seen)) {
                ++seen;
                continue;
            }
            PortPose n = one;
            n.port = toPort.name;
            poses.push_back(n);
            ++seen;
            if (index >= 0) {
                return;
            }
        }
        return;
    }
    if (fromPort.data == PortData::Tuple) {
        int seen = 0;
        for (int i = 0; i < static_cast<int>(src.tuples.size()); ++i) {
            const PortTuple& one = src.tuples[static_cast<size_t>(i)];
            if (!portNameOk(one.port, fromPort.name)) {
                continue;
            }
            if (!indexOk(index, seen)) {
                ++seen;
                continue;
            }
            PortTuple n = one;
            n.port = toPort.name;
            tuples.push_back(n);
            ++seen;
            if (index >= 0) {
                return;
            }
        }
        return;
    }
    if (fromPort.data == PortData::Cloud) {
        int seen = 0;
        for (int i = 0; i < static_cast<int>(src.clouds.size()); ++i) {
            const PortCloud& one = src.clouds[static_cast<size_t>(i)];
            if (!portNameOk(one.port, fromPort.name)) {
                continue;
            }
            if (!indexOk(index, seen)) {
                ++seen;
                continue;
            }
            PortCloud n = one;
            n.port = toPort.name;
            clouds.push_back(n);
            ++seen;
            if (index >= 0) {
                return;
            }
        }
        return;
    }
    if (fromPort.data == PortData::Pose3) {
        int seen = 0;
        for (int i = 0; i < static_cast<int>(src.poses3.size()); ++i) {
            const PortPose3& one = src.poses3[static_cast<size_t>(i)];
            if (!portNameOk(one.port, fromPort.name)) {
                continue;
            }
            if (!indexOk(index, seen)) {
                ++seen;
                continue;
            }
            PortPose3 n = one;
            n.port = toPort.name;
            poses3.push_back(n);
            ++seen;
            if (index >= 0) {
                return;
            }
        }
        return;
    }
    if (fromPort.data == PortData::Shape3) {
        int seen = 0;
        for (int i = 0; i < static_cast<int>(src.shapes3.size()); ++i) {
            const PortShape3& one = src.shapes3[static_cast<size_t>(i)];
            if (!portNameOk(one.port, fromPort.name)) {
                continue;
            }
            if (!indexOk(index, seen)) {
                ++seen;
                continue;
            }
            PortShape3 n = one;
            n.port = toPort.name;
            shapes3.push_back(n);
            ++seen;
            if (index >= 0) {
                return;
            }
        }
    }
}

// 只追加与端口类型对应的缺省口
void ToolIo::appendByType(const ToolIo& src, PortData kind)
{
    PortSlot from(PortDir::Out, kind);
    PortSlot to(PortDir::In, kind);
    appendByType(src, from, to, -1, RegionUse::Normal);
}

// image、ok、values、objects 与分类 io 对齐
void ToolOut::syncIo()
{
    if (io.images.empty() && !image.isNull()) {
        PortImage n;
        n.image = image;
        n.native = imageNative;
        io.images.push_back(n);
    }
    if (image.isNull() && !io.images.empty()) {
        image = io.images[0].image;
        imageNative = io.images[0].native;
    }
    if (io.logics.empty()) {
        PortLogic L;
        L.ok = ok;
        L.valid = true;
        L.message = message;
        io.logics.push_back(L);
    } else {
        ok = io.logics[0].ok;
    }
    if (io.poses.empty()) {
        HsCommData::Pose2 p;
        if (poseFromValues(values, &p)) {
            PortPose n;
            n.pose = p;
            io.poses.push_back(n);
        }
    }
    if (io.regions.empty() && io.contours.empty() && io.rois.empty() && !objects.empty()) {
        io.appendObjects(objects);
    }
    if (objects.empty()) {
        const std::vector<HsCommData::HsObject> regs = plainRegions(io.regions);
        const std::vector<HsCommData::HsObject> cons = plainContours(io.contours);
        const std::vector<HsCommData::HsObject> roiList = plainRois(io.rois);
        objects.insert(objects.end(), regs.begin(), regs.end());
        objects.insert(objects.end(), cons.begin(), cons.end());
        for (size_t i = 0; i < roiList.size(); ++i) {
            if (roiList[i].isInitialized() && !roiList[i].isEmpty()) {
                objects.push_back(roiList[i]);
            }
        }
    }
}


// 正常搜索区，丢掉屏蔽和无效
std::vector<HsCommData::HsObject> plainRois(const std::vector<PortRoi>& items)
{
    std::vector<HsCommData::HsObject> out;
    for (size_t i = 0; i < items.size(); ++i) {
        if (items[i].use != RegionUse::Normal) {
            continue;
        }
        out.push_back(items[i].roi);
    }
    return out;
}

// 正常区域对象
std::vector<HsCommData::HsObject> plainRegions(const std::vector<PortRegion>& items)
{
    std::vector<HsCommData::HsObject> out;
    for (size_t i = 0; i < items.size(); ++i) {
        if (items[i].use != RegionUse::Normal) {
            continue;
        }
        out.push_back(items[i].object);
    }
    return out;
}

// 轮廓对象
std::vector<HsCommData::HsObject> plainContours(const std::vector<PortContour>& items)
{
    std::vector<HsCommData::HsObject> out;
    for (size_t i = 0; i < items.size(); ++i) {
        out.push_back(items[i].object);
    }
    return out;
}

// 位姿列表
std::vector<HsCommData::Pose2> plainPoses(const std::vector<PortPose>& items)
{
    std::vector<HsCommData::Pose2> out;
    for (size_t i = 0; i < items.size(); ++i) {
        out.push_back(items[i].pose);
    }
    return out;
}

// 端口键
std::string PortDesc::key() const
{
    return portKey(slot);
}

// 端口方向
PortDir PortDesc::dir() const
{
    return slot.dir;
}

// 端口数据种类
PortData PortDesc::data() const
{
    return slot.data;
}



}
