/**
 * @file VisionCfgMgr.cpp
 * @brief VisionCfgMgr实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "VisionCfgMgr.h"
#include "AppPaths.h"
#include "SolutionDataPaths.h"
#include "TextEncoding.h"
#include "Translate.h"
#include "VisionVersion.h"
#include "VpUtil.h"
#include "ToolBase.h"
#include "RecipeDraft.h"
#include "Json.h"
#include "CfgStore.h"
#include <cstdio>
#include <cstdint>
#include <map>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace Vision {

JsonValue compositeToJson(const CompositeDef& d);
CompositeDef compositeFromJson(const JsonValue& o);

// Read whole file as UTF-8 text; path string is UTF-8
static bool readFileUtf8(const std::string& path, std::string* out)
{
    const CfgBytes got = CfgStore().read(path);
    if (got.kind != CfgFilePlain && got.kind != CfgFileSealed) {
        return false;
    }
    if (out) {
        *out = got.json;
    }
    return true;
}

static bool writeFileAtomic(const std::string& path, const std::string& data)
{
    return CfgStore().write(path, data);
}

// ParamMap 写成 JSON 对象
static JsonValue paramsToJson(const ParamMap& p)
{
    JsonValue o;
    o.makeObject();
    const std::vector<std::string> keys = p.keys();
    for (int i = 0; i < static_cast<int>(keys.size()); ++i) {
        const ParamValue v = p.value(keys[static_cast<size_t>(i)]);
        if (v.kind() == ParamValue::KindBool) {
            o.set(keys[static_cast<size_t>(i)], JsonValue(v.toBool()));
        } else if (v.kind() == ParamValue::KindInt) {
            o.set(keys[static_cast<size_t>(i)], JsonValue(v.toInt()));
        } else if (v.kind() == ParamValue::KindDouble) {
            o.set(keys[static_cast<size_t>(i)], JsonValue(v.toDouble()));
        } else if (v.kind() == ParamValue::KindString) {
            o.set(keys[static_cast<size_t>(i)], JsonValue(v.toString()));
        }
    }
    return o;
}

// 判断参数写成 JSON 数组
static JsonValue judgeToJson(const std::vector<JudgeParam>& items)
{
    JsonValue a;
    a.makeArray();
    for (int i = 0; i < static_cast<int>(items.size()); ++i) {
        const JudgeParam& j = items[static_cast<size_t>(i)];
        JsonValue o;
        o.makeObject();
        o.set("name", j.name);
        o.set("group", j.group);
        o.set("minVal", j.minVal);
        o.set("maxVal", j.maxVal);
        o.set("enable", j.enable);
        o.set("unit", j.unit);
        a.append(o);
    }
    return a;
}

// 从 JSON 数组读判断参数
static void judgeFromJson(const JsonValue& a, std::vector<JudgeParam>* out)
{
    if (!out || !a.isArray()) {
        return;
    }
    for (int i = 0; i < a.size(); ++i) {
        const JsonValue o = a.at(i);
        JudgeParam j;
        j.name = o.value("name").toString();
        j.group = o.value("group").toString();
        j.minVal = o.value("minVal").toDouble();
        j.maxVal = o.value("maxVal").toDouble();
        j.enable = o.value("enable").toBool(true);
        j.unit = o.value("unit").toString();
        out->push_back(j);
    }
}
// JSON 对象还原 ParamMap
static ParamMap paramsFromJson(const JsonValue& o)
{
    ParamMap p;
    if (!o.isObject()) {
        return p;
    }
    const std::map<std::string, JsonValue>& m = o.object();
    for (std::map<std::string, JsonValue>::const_iterator it = m.begin(); it != m.end(); ++it) {
        const JsonValue& v = it->second;
        if (v.isBool()) {
            p.insert(it->first, v.toBool());
        } else if (v.isNumber()) {
            const double d = v.toDouble();
            if (d == static_cast<double>(static_cast<int>(d))) {
                p.insert(it->first, v.toInt());
            } else {
                p.insert(it->first, d);
            }
        } else if (v.isString()) {
            p.insert(it->first, v.toString());
        }
    }
    return p;
}

// 工具记录写成 JSON（format3：info/links/params）
static JsonValue roiShapeToJson(const HsCommData::HsObject& roi);

// 一个工具写成 {info, para}；工具版本为空时取注册原型的版本
static JsonValue toolToJson(const ToolCfg& t)
{
    JsonValue o;
    o.makeObject();

    JsonValue info;
    info.makeObject();
    info.set("id", t.info.id.toString());
    info.set("toolName", t.info.toolName);
    info.set("type", toolTypeToInt(t.info.type));
    info.set("name", t.info.name);
    info.set("enabled", t.info.enabled);
    if (t.info.toolVersion.empty()) {
        if (ToolBase* proto = ToolRegistry::instance().find(t.info.toolName)) {
            info.set("toolVersion", proto->version());
        }
    } else {
        info.set("toolVersion", t.info.toolVersion);
    }
    if (!t.info.compositeId.isNull()) {
        info.set("compositeId", t.info.compositeId.toString());
        info.set("compositeScope", t.info.compositeScope);
    }
    o.set("info", info);



    JsonValue param;
    param.makeObject();
    param.set("paraVersion", t.param.paraVersion);
    param.set("algVersion", t.param.algVersion);
    param.set("algLib", JsonValue(static_cast<int>(t.param.algLib)));
    param.set("deviceId", t.param.deviceId.toString());
    if (!t.param.backendOverride.empty()) {
        param.set("backendOverride", t.param.backendOverride);
    }
    if (t.param.teachPar.valid()) {
        JsonValue teach;
        teach.makeObject();
        teach.set("relPath", t.param.teachPar.relPath);
        teach.set("fingerprint", t.param.teachPar.fingerprint);
        param.set("teachPar", teach);
    }
    if (!t.param.statCfg.empty()) {
        JsonValue stat;
        stat.makeObject();
        for (std::map<std::string, std::string>::const_iterator it = t.param.statCfg.begin();
             it != t.param.statCfg.end(); ++it) {
            stat.set(it->first, it->second);
        }
        param.set("statCfg", stat);
    }
    if (!t.param.displayCfg.empty()) {
        JsonValue disp;
        disp.makeArray();
        for (std::map<std::string, DisplayCfg>::const_iterator it = t.param.displayCfg.begin();
             it != t.param.displayCfg.end(); ++it) {
            JsonValue one;
            one.makeObject();
            one.set("key", it->first);
            one.set("text", it->second.text);
            one.set("enabled", it->second.enabled);
            one.set("alpha", it->second.alpha);
            one.set("anchor", it->second.anchor);
            disp.append(one);
        }
        param.set("displayCfg", disp);
    }
    // params 与 run 内容相同，给旧版读取用
    param.set("learn", paramsToJson(t.param.learn));
    param.set("run", paramsToJson(t.param.run));
    param.set("params", paramsToJson(t.param.run));
    param.set("judge", judgeToJson(t.param.judge));
    JsonValue rois;
    rois.makeArray();
    const std::vector<HsCommData::HsObject> rs = t.effectiveRois();
    for (int i = 0; i < static_cast<int>(rs.size()); ++i) {
        const HsCommData::HsObject& roi = rs[static_cast<size_t>(i)];
        if (!roi.isInitialized() || roi.isEmpty()) {
            continue;
        }
        rois.append(roiShapeToJson(roi));
    }
    if (rois.size() > 0) {
        param.set("rois", rois);
    }
    // 搜索区记录：引用项只存来源，绘制项才写几何
    JsonValue roiItems;
    roiItems.makeArray();
    for (size_t i = 0; i < t.param.roiItems.size(); ++i) {
        const ToolRoiItem& item = t.param.roiItems[i];
        JsonValue one;
        one.makeObject();
        one.set("slot", JsonValue(item.slot));
        one.set("enabled", JsonValue(item.enabled));
        one.set("presence", JsonValue(static_cast<int>(item.presence)));
        one.set("kind", JsonValue(static_cast<int>(item.kind)));
        one.set("refToolId", JsonValue(item.refToolId.toString()));
        one.set("refIndex", JsonValue(item.refIndex));
        one.set("followPose", JsonValue(item.followPose));
        one.set("poseToolId", JsonValue(item.poseToolId.toString()));
        if (item.kind == RoiItemKind::DrawRoi || item.kind == RoiItemKind::DrawRegion) {
            one.set("shape", roiShapeToJson(item.shape));
        }
        roiItems.append(one);
    }
    if (roiItems.size() > 0) {
        param.set("roiItems", roiItems);
    }
    if (!t.param.paramBinds.empty()) {
        JsonValue binds;
        binds.makeArray();
        for (size_t i = 0; i < t.param.paramBinds.size(); ++i) {
            const ParamBind& b = t.param.paramBinds[i];
            JsonValue one;
            one.makeObject();
            one.set("paramKey", b.paramKey);
            one.set("path", b.path);
            one.set("direction", b.direction);
            binds.append(one);
        }
        param.set("paramBinds", binds);
    }
    o.set("para", param);
    return o;
}

// 一块几何写成 JSON 对象；只写圆、椭圆、两种矩形和直线的参数
static JsonValue roiShapeToJson(const HsCommData::HsObject& roi)
{
    JsonValue r;
    r.makeObject();
    if (!roi.isInitialized() || roi.isEmpty()) {
        return r;
    }
    r.set("objectKind", JsonValue(static_cast<int>(roi.kind())));
    using namespace HsCommData;
    const ObjectKind k = roi.kind();
    if (k == ObjCircle) {
        const CircleP* c = roi.circleAt(0);
        r.set("x", JsonValue(c->c.x));
        r.set("y", JsonValue(c->c.y));
        r.set("r", JsonValue(c->r));
    } else if (k == ObjEllipse) {
        const EllipseP* e = roi.ellipseAt(0);
        r.set("x", JsonValue(e->c.x));
        r.set("y", JsonValue(e->c.y));
        r.set("a", JsonValue(e->ra));
        r.set("b", JsonValue(e->rb));
        r.set("phi", JsonValue(e->phi));
    } else if (k == ObjRect1) {
        const Rect1P* rc = roi.rect1At(0);
        r.set("x", JsonValue(rc->r.x));
        r.set("y", JsonValue(rc->r.y));
        r.set("a", JsonValue(rc->r.w));
        r.set("b", JsonValue(rc->r.h));
    } else if (k == ObjRect2) {
        const Rect2P* rc = roi.rect2At(0);
        r.set("x", JsonValue(rc->c.x));
        r.set("y", JsonValue(rc->c.y));
        r.set("a", JsonValue(rc->l1));
        r.set("b", JsonValue(rc->l2));
        r.set("phi", JsonValue(rc->phi));
    } else if (k == ObjLine) {
        const LineP* ln = roi.lineAt(0);
        r.set("x", JsonValue(ln->a.x));
        r.set("y", JsonValue(ln->a.y));
        r.set("x2", JsonValue(ln->b.x));
        r.set("y2", JsonValue(ln->b.y));
    }
    return r;
}

// 从 JSON 读一个 ROI。新结构用 objectKind，旧结构用 kind
static HsCommData::HsObject roiFromJson(const JsonValue& r)
{
    if (r.value("objectKind").isNumber() || r.contains("objectKind")) {
        using namespace HsCommData;
        HsCommData::HsObject roi;
        const ObjectKind k = static_cast<ObjectKind>(r.value("objectKind").toInt(-1));
        if (k == ObjCircle) {
            CircleP c;
            c.c = Point2d(r.value("x").toDouble(), r.value("y").toDouble());
            c.r = r.value("r").toDouble();
            roi = c;
        } else if (k == ObjEllipse) {
            EllipseP e;
            e.c = Point2d(r.value("x").toDouble(), r.value("y").toDouble());
            e.ra = r.value("a").toDouble();
            e.rb = r.value("b").toDouble();
            e.phi = r.value("phi").toDouble();
            roi = e;
        } else if (k == ObjRect1) {
            Rect1P rc;
            rc.r = Box2(r.value("x").toDouble(), r.value("y").toDouble(),
                        r.value("a").toDouble(), r.value("b").toDouble());
            roi = rc;
        } else if (k == ObjRect2) {
            Rect2P rc;
            rc.c = Point2d(r.value("x").toDouble(), r.value("y").toDouble());
            rc.l1 = r.value("a").toDouble();
            rc.l2 = r.value("b").toDouble();
            rc.phi = r.value("phi").toDouble();
            roi = rc;
        } else if (k == ObjLine) {
            LineP ln;
            ln.a = Point2d(r.value("x").toDouble(), r.value("y").toDouble());
            ln.b = Point2d(r.value("x2").toDouble(), r.value("y2").toDouble());
            roi = ln;
        }
        return roi;
    }
    // 旧结构：kind + x/y/a/b/phi，轮廓带折线点，区域带游程
    std::vector<double> polyX;
    std::vector<double> polyY;
    const JsonValue px = r.value("polyX");
    const JsonValue py = r.value("polyY");
    if (px.isArray() && py.isArray()) {
        const int n = px.size() < py.size() ? px.size() : py.size();
        for (int k = 0; k < n; ++k) {
            polyX.push_back(px.at(k).toDouble());
            polyY.push_back(py.at(k).toDouble());
        }
    }
    std::vector<std::int32_t> runRow;
    std::vector<std::int32_t> runCb;
    std::vector<std::int32_t> runCe;
    const JsonValue rr = r.value("runRow");
    const JsonValue rcb = r.value("runCb");
    const JsonValue rce = r.value("runCe");
    if (rr.isArray() && rcb.isArray() && rce.isArray() && rr.size() == rcb.size() && rr.size() == rce.size()) {
        for (int k = 0; k < rr.size(); ++k) {
            runRow.push_back(static_cast<std::int32_t>(rr.at(k).toInt()));
            runCb.push_back(static_cast<std::int32_t>(rcb.at(k).toInt()));
            runCe.push_back(static_cast<std::int32_t>(rce.at(k).toInt()));
        }
    }
    return toolRoiFromLegacy(r.value("kind").toInt(-1),
                             r.value("x").toDouble(), r.value("y").toDouble(),
                             r.value("a").toDouble(), r.value("b").toDouble(),
                             r.value("phi").toDouble(),
                             polyX, polyY, r.value("polyClosed").toBool(false),
                             runRow, runCb, runCe);
}

// JSON 还原工具记录，支持 format3 嵌套与旧扁平
static ToolCfg toolFromJson(const JsonValue& o)
{
    ToolCfg t;
    if (o.value("info").isObject()) {
        const JsonValue info = o.value("info");
        t.info.id = ObjectId::fromString(info.value("id").toString());
        t.info.toolName = ToolCfg::canonicalize(info.value("toolName").toString());
        t.info.name = info.value("name").toString();
        t.info.enabled = info.value("enabled").toBool(true);
        t.info.toolVersion = info.value("toolVersion").toString();
        t.info.compositeId = ObjectId::fromString(info.value("compositeId").toString());
        t.info.compositeScope = info.value("compositeScope").toInt(0);
        if (info.contains("type")) {
            t.info.type = toolTypeFromInt(info.value("type").toInt(-1));
        }
    } else {
        t.id = ObjectId::fromString(o.value("id").toString());
        t.toolName = ToolCfg::canonicalize(o.value("toolName").toString());
        t.name = o.value("name").toString();
        t.enabled = o.value("enabled").toBool(true);
        if (o.contains("type")) {
            t.type = toolTypeFromInt(o.value("type").toInt(-1));
        }
    }

    const JsonValue paraObj = o.value("para");
    const JsonValue paraSrc = paraObj.isObject() ? paraObj : o;
    if (paraSrc.contains("paraVersion")) {
        t.param.paraVersion = paraSrc.value("paraVersion").toString();
    } else {
        t.param.paraVersion.clear();
    }
    t.param.algVersion = paraSrc.value("algVersion").toString();
    t.param.algLib = static_cast<AlgLibKind>(paraSrc.value("algLib").toInt(static_cast<int>(AlgLibHal)));
    t.param.deviceId = ObjectId::fromString(paraSrc.value("deviceId").toString());
    t.param.backendOverride = paraSrc.value("backendOverride").toString();
    const JsonValue teach = paraSrc.value("teachPar");
    if (teach.isObject()) {
        t.param.teachPar.relPath = teach.value("relPath").toString();
        t.param.teachPar.fingerprint = teach.value("fingerprint").toString();
    }
    const JsonValue stat = paraSrc.value("statCfg");
    if (stat.isObject()) {
        const std::map<std::string, JsonValue>& fields = stat.object();
        for (std::map<std::string, JsonValue>::const_iterator it = fields.begin();
             it != fields.end(); ++it) {
            t.param.statCfg[it->first] = it->second.toString();
        }
    }
    const JsonValue disp = paraSrc.value("displayCfg");
    if (disp.isArray()) {
        for (int i = 0; i < disp.size(); ++i) {
            const JsonValue one = disp.at(i);
            DisplayCfg cfg;
            cfg.text = one.value("text").toBool(false);
            cfg.enabled = one.value("enabled").toBool(true);
            cfg.alpha = one.value("alpha").toInt(100);
            cfg.anchor = one.value("anchor").toInt(0);
            t.param.displayCfg[one.value("key").toString()] = cfg;
        }
    }
    if (paraSrc.contains("run")) { t.param.run = paramsFromJson(paraSrc.value("run")); } else { t.param.run = paramsFromJson(paraSrc.value("params")); }
    if (paraSrc.contains("learn")) { t.param.learn = paramsFromJson(paraSrc.value("learn")); }
    judgeFromJson(paraSrc.value("judge"), &t.param.judge);
    const JsonValue binds = paraSrc.value("paramBinds");
    if (binds.isArray()) {
        t.param.paramBinds.clear();
        for (int i = 0; i < binds.size(); ++i) {
            const JsonValue one = binds.at(i);
            ParamBind b;
            b.paramKey = one.value("paramKey").toString();
            b.path = one.value("path").toString();
            b.direction = one.value("direction").toInt(0);
            t.param.paramBinds.push_back(b);
        }
    }
    // 旧文件的 pipeline 数组折成 pl_* 参数
    const JsonValue pipeline = paraSrc.value("pipeline");
    if (pipeline.isArray() && !t.param.run.contains("pl_n")
        && (t.info.toolName == "preprocess.enhance" || t.info.toolName == "preprocess.smooth")) {
        const bool enhance = t.info.toolName == "preprocess.enhance";
        t.param.run.insert("pl_n", pipeline.size());
        for (int i = 0; i < pipeline.size(); ++i) {
            const JsonValue one = pipeline.at(i);
            const std::string op = std::string("pl_") + std::to_string(static_cast<long long>(i)) + "_op";
            const std::string w = std::string("pl_") + std::to_string(static_cast<long long>(i)) + "_w";
            const std::string h = std::string("pl_") + std::to_string(static_cast<long long>(i)) + "_h";
            const std::string c0 = std::string("pl_") + std::to_string(static_cast<long long>(i)) + "_c0";
            const std::string c1 = std::string("pl_") + std::to_string(static_cast<long long>(i)) + "_c1";
            const std::string f = std::string("pl_") + std::to_string(static_cast<long long>(i)) + "_f";
            t.param.run.insert(op, one.value("op").toInt(0));
            t.param.run.insert(w, one.value("maskW").toInt(enhance ? 7 : 3));
            t.param.run.insert(h, one.value("maskH").toInt(enhance ? 7 : 3));
            if (enhance) {
                t.param.run.insert(c0, one.value("coeff").toDouble(1.0));
                t.param.run.insert(c1, one.value("coeff2").toDouble(0.0));
                t.param.run.insert(f, one.value("scaleMax").toBool(false) ? 1 : 0);
            } else {
                t.param.run.insert(c0, one.value("sigmaRange").toDouble(20.0));
                t.param.run.insert(c1, one.value("sigmaSpace").toDouble(3.0));
            }
        }
    }
    const JsonValue rois = paraSrc.value("rois");
    if (rois.isArray() && rois.size() > 0) {
        for (int i = 0; i < rois.size(); ++i) {
            const HsCommData::HsObject roi = roiFromJson(rois.at(i));
            if (roi.isInitialized() && !roi.isEmpty()) {
                t.param.rois.push_back(roi);
            }
        }
    } else {
        const JsonValue roi = paraSrc.value("roi");
        if (roi.isObject()) {
            const HsCommData::HsObject one = roiFromJson(roi);
            if (one.isInitialized() && !one.isEmpty()) {
                t.param.rois.push_back(one);
            }
        }
    }

    const JsonValue roiItems = paraSrc.value("roiItems");
    if (roiItems.isArray() && roiItems.size() > 0) {
        t.param.roiItems.clear();
        for (int i = 0; i < roiItems.size(); ++i) {
            const JsonValue one = roiItems.at(i);
            ToolRoiItem item;
            item.slot = one.value("slot").toString();
            item.enabled = one.value("enabled").toBool(true);
            item.presence = static_cast<RoiPresence>(one.value("presence").toInt(0));
            if (one.contains("kind")) {
                const int rk = one.value("kind").toInt(-1);
                if (rk >= 0 && rk < static_cast<int>(RoiItemKind::Max)) {
                    item.kind = static_cast<RoiItemKind>(rk);
                }
            }
            item.refToolId = ObjectId::fromString(one.value("refToolId").toString());
            item.refIndex = one.value("refIndex").toInt(-1);
            item.followPose = one.value("followPose").toBool(false);
            item.poseToolId = ObjectId::fromString(one.value("poseToolId").toString());
            if (one.value("shape").isObject()) {
                item.shape = roiFromJson(one.value("shape"));
            }
            t.param.roiItems.push_back(item);
        }
        t.param.rois.clear();
        for (size_t i = 0; i < t.param.roiItems.size(); ++i) {
            const ToolRoiItem& item = t.param.roiItems[i];
            if (item.kind != RoiItemKind::DrawRoi && item.kind != RoiItemKind::DrawRegion) {
                continue;
            }
            if (item.shape.isInitialized() && !item.shape.isEmpty()) {
                t.param.rois.push_back(item.shape);
            }
        }
    }

    const JsonValue toolLinks = o.value("links");
    if (toolLinks.isArray()) {
        for (int i = 0; i < toolLinks.size(); ++i) {
            const JsonValue lo = toolLinks.at(i);
            if (!lo.contains("fromToolId") && !lo.contains("fromId")) {
                continue;
            }
            ToolInputLink in;
            if (lo.contains("fromToolId")) {
                in.fromToolId = ObjectId::fromString(lo.value("fromToolId").toString());
            } else {
                in.fromToolId = ObjectId::fromString(lo.value("fromId").toString());
            }
            in.fromPort = portSlotFromInts(lo.value("fromPortDir").toInt(),
                                           lo.value("fromPortData").toInt());
            in.toPort = portSlotFromInts(lo.value("toPortDir").toInt(),
                                         lo.value("toPortData").toInt());
            in.fromPort.name = lo.value("fromPortName").toString();
            in.toPort.name = lo.value("toPortName").toString();
            in.index = lo.value("index").toInt(-1);
            in.use = regionUseFromInt(lo.value("use").toInt(0));
            in.origin = static_cast<ToolLinkOrigin>(lo.value("origin").toInt(LinkOriginManual));
            t.links.push_back(in);
        }
    }

    t.syncRois();
    ToolBase* impl = ToolRegistry::instance().find(t.info.toolName);
    if (impl) {
        t.info.type = impl->type();
    }
    // 旧文件里画布坐标仍可能写在工具上
    t.canvasPlaced = o.value("canvasPlaced").toBool(false);
    t.canvasX = o.value("canvasX").toDouble();
    t.canvasY = o.value("canvasY").toDouble();
    t.ensureDefaults();
    t.migrateRuntime();
    return t;
}

// 组写成 JSON
static JsonValue groupToJson(const ToolGroup& g)
{
    JsonValue o;
    o.makeObject();
    o.set("id", g.id.toString());
    o.set("kind", g.kind);
    o.set("kindName", groupKindName(g.kind));
    o.set("name", g.name);
    o.set("toolId", g.toolId.toString());
    o.set("condSourceId", g.condSourceId.toString());
    o.set("condWant", g.condWant);
    o.set("loopCount", g.loopCount);
    if (!g.subJobRef.empty()) {
        o.set("subJobRef", g.subJobRef);
    }
    if (g.parallelLimit != 0) {
        o.set("parallelLimit", static_cast<int>(g.parallelLimit));
    }
    if (g.canvasPlaced) {
        o.set("canvasPlaced", JsonValue(true));
        o.set("canvasX", JsonValue(g.canvasX));
        o.set("canvasY", JsonValue(g.canvasY));
    }
    JsonValue ch;
    ch.makeArray();
    for (int i = 0; i < static_cast<int>(g.children.size()); ++i) {
        ch.append(groupToJson(g.children[static_cast<size_t>(i)]));
    }
    o.set("children", ch);
    return o;
}

// JSON 还原组
static ToolGroup groupFromJson(const JsonValue& o)
{
    ToolGroup g;
    g.id = ObjectId(o.value("id").toString());
    if (o.contains("kindName") && !o.value("kindName").toString().empty()) {
        g.kind = groupKindFromName(o.value("kindName").toString());
    } else if (o.contains("kind")) {
        g.kind = groupKindFromLegacy(o.value("kind").toInt(-1));
    }
    g.name = o.value("name").toString();
    g.toolId = ObjectId(o.value("toolId").toString());
    g.condSourceId = ObjectId(o.value("condSourceId").toString());
    g.condWant = o.value("condWant").toInt(CondThenOnOk);
    g.loopCount = o.value("loopCount").toInt(1);
    g.subJobRef = o.value("subJobRef").toString();
    g.parallelLimit = static_cast<unsigned>(o.value("parallelLimit").toInt(0));
    if (g.loopCount < 1) {
        g.loopCount = 1;
    }
    g.canvasPlaced = o.value("canvasPlaced").toBool(false);
    g.canvasX = o.value("canvasX").toDouble(0);
    g.canvasY = o.value("canvasY").toDouble(0);
    const JsonValue ch = o.value("children");
    for (int i = 0; i < ch.size(); ++i) {
        g.children.push_back(groupFromJson(ch.at(i)));
    }
    return g;
}

// 流程布局表转 JSON
// 折线拐点写入 JSON 数组
static JsonValue routePointsToJson(const std::vector<RoutePoint>& pts)
{
    JsonValue arr;
    arr.makeArray();
    for (size_t i = 0; i < pts.size(); ++i) {
        JsonValue p;
        p.makeObject();
        p.set("x", pts[i].x);
        p.set("y", pts[i].y);
        arr.append(p);
    }
    return arr;
}

// 读折线拐点；兼容旧 routeX1..Y2
static void routePointsFromJson(const JsonValue& o, bool* custom, std::vector<RoutePoint>* pts)
{
    if (!custom || !pts) {
        return;
    }
    *custom = o.value("routeCustom").toBool(false);
    pts->clear();
    const JsonValue arr = o.value("routePoints");
    if (arr.isArray() && arr.size() > 0) {
        for (int i = 0; i < arr.size(); ++i) {
            const JsonValue p = arr.at(i);
            pts->push_back(RoutePoint(p.value("x").toDouble(0), p.value("y").toDouble(0)));
        }
        if (!pts->empty()) {
            *custom = true;
        }
        return;
    }
    if (!*custom) {
        return;
    }
    const double x1 = o.value("routeX1").toDouble(0);
    const double y1 = o.value("routeY1").toDouble(0);
    const double x2 = o.value("routeX2").toDouble(0);
    const double y2 = o.value("routeY2").toDouble(0);
    if (x1 != 0.0 || y1 != 0.0 || x2 != 0.0 || y2 != 0.0) {
        pts->push_back(RoutePoint(x1, y1));
        pts->push_back(RoutePoint(x2, y2));
    }
}
// 流程布局写成 {nodes, routes}；布局表为空时从工具画布字段和自定义连线补出
static JsonValue jobLayoutToJson(const JobCfg& j)
{
    JsonValue o;
    o.makeObject();
    JsonValue nodes;
    nodes.makeArray();
    if (!j.layout.nodes.empty()) {
        for (size_t i = 0; i < j.layout.nodes.size(); ++i) {
            const JobNodeLayout& n = j.layout.nodes[i];
            JsonValue no;
            no.makeObject();
            no.set("toolId", n.toolId.toString());
            no.set("canvasX", n.canvasX);
            no.set("canvasY", n.canvasY);
            no.set("canvasPlaced", n.canvasPlaced);
            nodes.append(no);
        }
    } else {
        for (size_t i = 0; i < j.tools.size(); ++i) {
            const ToolCfg& t = j.tools[i];
            JsonValue no;
            no.makeObject();
            no.set("toolId", t.id.toString());
            no.set("canvasX", t.canvasX);
            no.set("canvasY", t.canvasY);
            no.set("canvasPlaced", t.canvasPlaced);
            nodes.append(no);
        }
    }
    o.set("nodes", nodes);
    JsonValue routes;
    routes.makeArray();
    if (!j.layout.routes.empty()) {
        for (size_t i = 0; i < j.layout.routes.size(); ++i) {
            const JobRouteLayout& r = j.layout.routes[i];
            JsonValue ro;
            ro.makeObject();
            ro.set("toToolId", r.toToolId.toString());
            ro.set("toPortDir", static_cast<int>(r.toPort.dir));
            ro.set("toPortData", static_cast<int>(r.toPort.data));
            ro.set("toPortName", r.toPort.name);
            ro.set("routeCustom", r.routeCustom);
            ro.set("routePoints", routePointsToJson(r.routePoints));
            routes.append(ro);
        }
    } else {
        // 旧数据只有连线上的拐点，仅导出自定义走线
        for (size_t i = 0; i < j.links.size(); ++i) {
            const ToolLink& L = j.links[i];
            if (!L.routeCustom) {
                continue;
            }
            JsonValue ro;
            ro.makeObject();
            ro.set("toToolId", L.toId.toString());
            ro.set("toPortDir", static_cast<int>(L.toPort.dir));
            ro.set("toPortData", static_cast<int>(L.toPort.data));
            ro.set("toPortName", L.toPort.name);
            ro.set("routeCustom", true);
            ro.set("routePoints", routePointsToJson(L.routePoints));
            routes.append(ro);
        }
    }
    o.set("routes", routes);
    return o;
}

// JSON 还原流程布局
static JobLayout jobLayoutFromJson(const JsonValue& o)
{
    JobLayout lay;
    if (!o.isObject()) {
        return lay;
    }
    const JsonValue nodes = o.value("nodes");
    if (nodes.isArray()) {
        for (int i = 0; i < nodes.size(); ++i) {
            const JsonValue no = nodes.at(i);
            JobNodeLayout n;
            n.toolId = ObjectId::fromString(no.value("toolId").toString());
            n.canvasX = no.value("canvasX").toDouble();
            n.canvasY = no.value("canvasY").toDouble();
            n.canvasPlaced = no.value("canvasPlaced").toBool(false);
            lay.nodes.push_back(n);
        }
    }
    const JsonValue routes = o.value("routes");
    if (routes.isArray()) {
        for (int i = 0; i < routes.size(); ++i) {
            const JsonValue ro = routes.at(i);
            JobRouteLayout r;
            r.toToolId = ObjectId::fromString(ro.value("toToolId").toString());
            r.toPort = portSlotFromInts(ro.value("toPortDir").toInt(),
                                        ro.value("toPortData").toInt());
            r.toPort.name = ro.value("toPortName").toString();
            routePointsFromJson(ro, &r.routeCustom, &r.routePoints);
            lay.routes.push_back(r);
        }
    }
    return lay;
}

// JobCfg 写成 JSON
static JsonValue jobToJson(const JobCfg& j)
{
    JsonValue o;
    o.makeObject();
    o.set("id", j.id.toString());
    o.set("name", j.name);
    o.set("timeoutMs", j.timeoutMs);
    o.set("order", j.order);
    o.set("enabled", j.enabled);
    o.set("execMode", execWayName(j.execMode));
    o.set("failPolicyName", failPolicyName(j.failPolicy));
    if (!j.resultToolId.isNull()) {
        o.set("resultToolId", j.resultToolId.toString());
    }
    o.set("runOnUnloadOnce", j.runOnUnloadOnce);
    o.set("refreshUiWhileRunning", j.refreshUiWhileRunning);
    o.set("encrypted", j.encrypted);
    o.set("retakeCount", j.retakeCount);
    o.set("textX", j.textX);
    o.set("textY", j.textY);
    o.set("textPx", j.textPx);
    o.set("okColor", j.okColor);
    o.set("ngColor", j.ngColor);
    o.set("saveOk", j.saveOk);
    o.set("saveNg", j.saveNg);
    o.set("saveAll", j.saveAll);
    o.set("saveBatchAll", j.saveBatchAll);
    o.set("saveLimit", j.saveLimit);
    o.set("saveDir", j.saveDir);
    o.set("yieldMode", j.yieldMode);
    o.set("batchYield", j.batchYield);
    o.set("recentCount", j.recentCount);
    o.set("recentYield", j.recentYield);
    o.set("ngLimit", j.ngLimit);
    o.set("yieldAction", j.yieldAction);
    o.set("singleToolScope", j.singleToolScope);
    JsonValue tools;
    tools.makeArray();
    for (int i = 0; i < static_cast<int>(j.tools.size()); ++i) {
        tools.append(toolToJson(j.tools[static_cast<size_t>(i)]));
    }
    o.set("tools", tools);
    // 过渡：links 是由工具输入边派生的缓存（rebuildLinksCache 写入），照常写出
    const std::vector<ToolLink>& linkView = j.links;
    JsonValue links;
    links.makeArray();
    for (int i = 0; i < static_cast<int>(linkView.size()); ++i) {
        JsonValue L;
        L.makeObject();
        L.set("fromId", linkView[static_cast<size_t>(i)].fromId.toString());
        L.set("toId", linkView[static_cast<size_t>(i)].toId.toString());
        L.set("fromPortDir", static_cast<int>(linkView[static_cast<size_t>(i)].fromPort.dir));
        L.set("fromPortData", static_cast<int>(linkView[static_cast<size_t>(i)].fromPort.data));
        L.set("toPortDir", static_cast<int>(linkView[static_cast<size_t>(i)].toPort.dir));
        L.set("toPortData", static_cast<int>(linkView[static_cast<size_t>(i)].toPort.data));
        L.set("fromPortName", linkView[static_cast<size_t>(i)].fromPort.name);
        L.set("toPortName", linkView[static_cast<size_t>(i)].toPort.name);
        L.set("index", linkView[static_cast<size_t>(i)].index);
        L.set("use", static_cast<int>(linkView[static_cast<size_t>(i)].use));
        L.set("origin", linkView[static_cast<size_t>(i)].origin);
        if (linkView[static_cast<size_t>(i)].routeCustom) {
            L.set("routeCustom", true);
            L.set("routePoints", routePointsToJson(linkView[static_cast<size_t>(i)].routePoints));
        }
        links.append(L);
    }
    o.set("links", links);
    o.set("layout", jobLayoutToJson(j));
    o.set("root", groupToJson(j.root));
    if (!j.resultFromRuleId.isNull()) {
        o.set("resultFromRuleId", j.resultFromRuleId.toString());
    }
    if (!j.resultRules.empty()) {
        JsonValue rules;
        rules.makeArray();
        for (int i = 0; i < static_cast<int>(j.resultRules.size()); ++i) {
            const RstRule& r = j.resultRules[static_cast<size_t>(i)];
            JsonValue ro;
            ro.makeObject();
            ro.set("id", r.id.toString());
            ro.set("name", r.name);
            if (!r.toolId.isNull()) {
                ro.set("toolId", r.toolId.toString());
            }
            JsonValue terms;
            terms.makeArray();
            for (int t = 0; t < static_cast<int>(r.terms.size()); ++t) {
                const RstRuleTerm& term = r.terms[static_cast<size_t>(t)];
                JsonValue to;
                to.makeObject();
                to.set("kind", term.kind);
                to.set("sourceId", term.sourceId.toString());
                to.set("state", term.state);
                terms.append(to);
            }
            ro.set("terms", terms);
            rules.append(ro);
        }
        o.set("resultRules", rules);
    }
    return o;
}

// JSON 还原 JobCfg
static JobCfg jobFromJson(const JsonValue& o)
{
    JobCfg j;
    j.id = ObjectId(o.value("id").toString());
    j.name = o.value("name").toString();
    j.timeoutMs = o.value("timeoutMs").toInt(500);
    j.order = o.value("order").toInt(1);
    j.enabled = o.value("enabled").toBool(true);
    if (o.contains("execMode")) {
        j.execMode = execWayFromName(o.value("execMode").toString());
    } else if (o.contains("trigger")) {
        j.execMode = execWayFromLegacyTrigger(o.value("trigger").toInt(-1));
    }
    if (o.contains("failPolicyName")) {
        j.failPolicy = failPolicyFromName(o.value("failPolicyName").toString());
    } else {
        j.failPolicy = failPolicyFromLegacy(o.value("failPolicy").toInt(0));
    }
    if (o.contains("resultToolId")) {
        j.resultToolId = ObjectId::fromString(o.value("resultToolId").toString());
    }
    j.runOnUnloadOnce = o.value("runOnUnloadOnce").toBool(false);
    j.refreshUiWhileRunning = o.value("refreshUiWhileRunning").toBool(true);
    j.encrypted = o.value("encrypted").toBool(false);
    j.retakeCount = o.value("retakeCount").toInt(0);
    j.textX = o.value("textX").toInt(20);
    j.textY = o.value("textY").toInt(20);
    j.textPx = o.value("textPx").toInt(16);
    j.okColor = o.value("okColor").toString();
    if (j.okColor.empty()) {
        j.okColor = "#22A050";
    }
    j.ngColor = o.value("ngColor").toString();
    if (j.ngColor.empty()) {
        j.ngColor = "#D03830";
    }
    j.saveOk = o.value("saveOk").toBool(false);
    j.saveNg = o.value("saveNg").toBool(false);
    j.saveAll = o.value("saveAll").toBool(false);
    j.saveBatchAll = o.value("saveBatchAll").toBool(true);
    j.saveLimit = o.value("saveLimit").toInt(20);
    j.saveDir = o.value("saveDir").toString();
    j.yieldMode = o.value("yieldMode").toInt(0);
    j.batchYield = o.value("batchYield").toDouble(90);
    j.recentCount = o.value("recentCount").toInt(100);
    j.recentYield = o.value("recentYield").toDouble(90);
    j.ngLimit = o.value("ngLimit").toInt(20);
    j.yieldAction = o.value("yieldAction").toInt(0);
    j.singleToolScope = o.value("singleToolScope").toInt(0);
    const JsonValue tools = o.value("tools");
    for (int i = 0; i < tools.size(); ++i) {
        j.tools.push_back(toolFromJson(tools.at(i)));
    }
    const JsonValue links = o.value("links");
    for (int i = 0; i < links.size(); ++i) {
        const JsonValue lo = links.at(i);
        ToolLink L;
        L.fromId = ObjectId::fromString(lo.value("fromId").toString());
        L.toId = ObjectId::fromString(lo.value("toId").toString());
        L.fromPort = portSlotFromInts(lo.value("fromPortDir").toInt(),
                                      lo.value("fromPortData").toInt());
        L.toPort = portSlotFromInts(lo.value("toPortDir").toInt(),
                                    lo.value("toPortData").toInt());
        L.fromPort.name = lo.value("fromPortName").toString();
        L.toPort.name = lo.value("toPortName").toString();
        L.index = lo.value("index").toInt(-1);
        L.use = regionUseFromInt(lo.value("use").toInt(0));
        L.origin = static_cast<ToolLinkOrigin>(lo.value("origin").toInt(LinkOriginManual));
        routePointsFromJson(lo, &L.routeCustom, &L.routePoints);
        j.links.push_back(L);
    }
    if (o.contains("layout")) {
        j.layout = jobLayoutFromJson(o.value("layout"));
        j.applyLayoutToTools();
    } else {
        j.syncLayoutFromTools();
    }
    // 工具尚无入边时，由工具 links 回填
    bool anyIn = false;
    for (size_t i = 0; i < j.tools.size(); ++i) {
        if (!j.tools[i].links.empty()) {
            anyIn = true;
            break;
        }
    }
    if (j.links.empty()) {
        j.links = j.collectLinks();
    }
    for (size_t i = 0; i < j.tools.size(); ++i) {
        j.tools[i].links.clear();
    }
    if (o.contains("root")) {
        j.root = groupFromJson(o.value("root"));
    }
    if (o.contains("resultFromRuleId")) {
        j.resultFromRuleId = ObjectId::fromString(o.value("resultFromRuleId").toString());
    }
    if (o.contains("resultRules")) {
        const JsonValue rules = o.value("resultRules");
        for (int i = 0; i < rules.size(); ++i) {
            const JsonValue ro = rules.at(i);
            RstRule r;
            r.id = ObjectId::fromString(ro.value("id").toString());
            r.name = ro.value("name").toString();
            if (ro.contains("toolId")) {
                r.toolId = ObjectId::fromString(ro.value("toolId").toString());
            }
            const JsonValue terms = ro.value("terms");
            for (int t = 0; t < terms.size(); ++t) {
                const JsonValue to = terms.at(t);
                RstRuleTerm term;
                term.kind = to.value("kind").toInt();
                term.sourceId = ObjectId::fromString(to.value("sourceId").toString());
                term.state = to.value("state").toInt();
                r.terms.push_back(term);
            }
            j.resultRules.push_back(r);
        }
    }
    j.normalizeRstRules();
    return j;
}

// 运行画面写成 JSON
static JsonValue runtimeViewToJson(const RuntimeView& view)
{
    RuntimeView v = view;
    v.normalize();
    JsonValue o;
    o.makeObject();
    o.set("rows", v.rows);
    o.set("cols", v.cols);
    o.set("grid", (v.rows == v.cols) ? v.rows : 0);
    o.set("showStats", JsonValue(v.showStats));
    o.set("showHistory", JsonValue(v.showHistory));
    o.set("showSideButtons", JsonValue(v.showSideButtons));
    JsonValue panes;
    panes.makeArray();
    for (int i = 0; i < static_cast<int>(v.panes.size()); ++i) {
        const RuntimePane& pane = v.panes[static_cast<size_t>(i)];
        JsonValue p;
        p.makeObject();
        p.set("enabled", JsonValue(pane.enabled));
        p.set("imageToolId", pane.imageToolId.toString());
        JsonValue ids;
        ids.makeArray();
        for (int k = 0; k < static_cast<int>(pane.overlayToolIds.size()); ++k) {
            ids.append(JsonValue(pane.overlayToolIds[static_cast<size_t>(k)].toString()));
        }
        p.set("overlayToolIds", ids);
        const std::string firstOv = pane.overlayToolIds.empty()
            ? std::string()
            : pane.overlayToolIds[0].toString();
        p.set("overlayToolId", firstOv);
        p.set("showOverlay", JsonValue(pane.showOverlay));
        panes.append(p);
    }
    o.set("panes", panes);
    return o;
}

// 运行画面：网格，或旧的 grid 与 overlayToolId
static RuntimeView runtimeViewFromJson(const JsonValue& o)
{
    RuntimeView v;
    if (o.isNull() || !o.isObject()) {
        v.normalize();
        return v;
    }
    const int grid = o.value("grid").toInt(1);
    v.rows = o.value("rows").toInt(0);
    v.cols = o.value("cols").toInt(0);
    if (v.rows < 1 || v.cols < 1) {
        int g = grid;
        if (g < 1) {
            g = 1;
        }
        if (g > 3) {
            g = 3;
        }
        v.rows = g;
        v.cols = g;
    }
    v.showStats = o.value("showStats").toBool(true);
    v.showHistory = o.value("showHistory").toBool(true);
    v.showSideButtons = o.value("showSideButtons").toBool(true);
    const JsonValue panes = o.value("panes");
    for (int i = 0; i < panes.size() && i < 9; ++i) {
        const JsonValue po = panes.at(i);
        RuntimePane p;
        p.enabled = po.value("enabled").toBool(true);
        p.imageToolId = ObjectId(po.value("imageToolId").toString());
        p.showOverlay = po.value("showOverlay").toBool(true);
        const JsonValue ids = po.value("overlayToolIds");
        if (ids.isArray()) {
            for (int k = 0; k < ids.size(); ++k) {
                p.overlayToolIds.push_back(ObjectId(ids.at(k).toString()));
            }
        } else {
            p.overlayToolIds.push_back(ObjectId(po.value("overlayToolId").toString()));
        }
        v.panes.push_back(p);
    }
    v.normalize();
    return v;
}

// 工站写成 JSON：设备绑定与 JobCfg 列表
static JsonValue stationToJson(const StationCfg& st)
{
    JsonValue o;
    o.makeObject();
    o.set("id", st.id.toString());
    o.set("name", st.name);
    o.set("stationCode", st.stationCode);
    o.set("enabled", st.enabled);
    o.set("jobSchedule", static_cast<int>(st.jobSchedule));
    o.set("timeoutMs", st.timeoutMs);
    o.set("failPolicyName", failPolicyName(st.failPolicy));
    o.set("commChannelId", st.commChannelId);
    JsonValue bound;
    bound.makeArray();
    for (int i = 0; i < static_cast<int>(st.boundDevices.size()); ++i) {
        JsonValue b;
        b.makeObject();
        b.set("deviceId", st.boundDevices[static_cast<size_t>(i)].deviceId.toString());
        b.set("enabled", st.boundDevices[static_cast<size_t>(i)].enabled);
        bound.append(b);
    }
    o.set("boundDevices", bound);
    JsonValue jobs;
    jobs.makeArray();
    for (int i = 0; i < static_cast<int>(st.jobs.size()); ++i) {
        jobs.append(jobToJson(st.jobs[static_cast<size_t>(i)]));
    }
    o.set("jobs", jobs);
    o.set("runtimeView", runtimeViewToJson(st.runtimeView));
    return o;
}

// JSON 还原工站
static StationCfg stationFromJson(const JsonValue& o)
{
    StationCfg st;
    st.id = ObjectId(o.value("id").toString());
    st.name = o.value("name").toString();
    st.stationCode = o.value("stationCode").toString();
    st.enabled = o.value("enabled").toBool(true);
    st.jobSchedule = jobScheduleFromInt(o.value("jobSchedule").toInt());
    st.timeoutMs = o.value("timeoutMs").toInt(500);
    if (o.contains("failPolicyName")) {
        st.failPolicy = failPolicyFromName(o.value("failPolicyName").toString());
    } else {
        st.failPolicy = failPolicyFromLegacy(o.value("failPolicy").toInt(0));
    }
    st.commChannelId = o.value("commChannelId").toString();
    const JsonValue bound = o.value("boundDevices");
    for (int i = 0; i < bound.size(); ++i) {
        DeviceRef r;
        r.deviceId = ObjectId(bound.at(i).value("deviceId").toString());
        r.enabled = bound.at(i).value("enabled").toBool(true);
        st.boundDevices.push_back(r);
    }
    const JsonValue jobs = o.value("jobs");
    for (int i = 0; i < jobs.size(); ++i) {
        st.jobs.push_back(jobFromJson(jobs.at(i)));
    }
    st.runtimeView = runtimeViewFromJson(o.value("runtimeView"));
    return st;
}

// 清掉不参与存盘比较的 paraVersion
static void clearParaVersions(JobCfg& job)
{
    for (size_t i = 0; i < job.tools.size(); ++i) {
        job.tools[i].param.paraVersion.clear();
    }
}

// 清掉不参与存盘比较的 paraVersion
static void clearPersistNoise(Solution& s)
{
    for (size_t i = 0; i < s.stations.size(); ++i) {
        StationCfg& st = s.stations[i];
        for (size_t j = 0; j < st.jobs.size(); ++j) {
            clearParaVersions(st.jobs[j]);
        }
    }
    for (size_t i = 0; i < s.composites.size(); ++i) {
        clearParaVersions(s.composites[i].inner);
    }
}

// 方案可存盘 JSON。saveMeta 为假时不含保存者和时间
static JsonValue solutionToJson(const Solution& s, bool saveMeta)
{
    JsonValue root;
    root.makeObject();
    root.set("format", 3);
    root.set("name", s.name);
    if (saveMeta) {
        JsonValue savedBy;
        savedBy.makeObject();
        savedBy.set("id", s.lastSavedById);
        savedBy.set("name", s.lastSavedByName);
        root.set("lastSavedBy", savedBy);
        root.set("lastSavedAt", s.lastSavedAt);
        root.set("platformVersion", VISION_VERSION_STRING);
    }
    root.set("runtimeStationLayout", static_cast<int>(s.runtimeStationLayout));
    // 设备台账、通信通道、工站、项目组合工具依次写出
    JsonValue inv;
    inv.makeArray();
    for (int i = 0; i < static_cast<int>(s.inventory.size()); ++i) {
        const HardwareDevice& d = s.inventory[static_cast<size_t>(i)];
        JsonValue o;
        o.makeObject();
        o.set("id", d.id.toString());
        o.set("name", d.name);
        o.set("kind", static_cast<int>(d.kind));
        o.set("cameraBackend", static_cast<int>(d.cameraBackend));
        o.set("params", paramsToJson(d.defaultParams));
        inv.append(o);
    }
    root.set("inventory", inv);
    JsonValue chs;
    chs.makeArray();
    for (int i = 0; i < static_cast<int>(s.commChannels.size()); ++i) {
        const CommChannel& c = s.commChannels[static_cast<size_t>(i)];
        JsonValue o;
        o.makeObject();
        o.set("id", c.id.toString());
        o.set("name", c.name);
        o.set("type", static_cast<int>(c.type));
        o.set("enabled", c.enabled);
        o.set("params", paramsToJson(c.params));
        chs.append(o);
    }
    root.set("commChannels", chs);
    JsonValue sts;
    sts.makeArray();
    for (int i = 0; i < static_cast<int>(s.stations.size()); ++i) {
        sts.append(stationToJson(s.stations[static_cast<size_t>(i)]));
    }
    root.set("stations", sts);
    JsonValue comps;
    comps.makeArray();
    for (int i = 0; i < static_cast<int>(s.composites.size()); ++i) {
        comps.append(compositeToJson(s.composites[static_cast<size_t>(i)]));
    }
    root.set("composites", comps);
    // 全局变量与自定义 UI 页（缺省为空，向后兼容）
    JsonValue gvars;
    gvars.makeArray();
    const std::vector<GlobalVar> vars = s.globalVars.items();
    for (size_t i = 0; i < vars.size(); ++i) {
        const GlobalVar& g = vars[i];
        JsonValue o;
        o.makeObject();
        o.set("name", g.name);
        o.set("type", static_cast<int>(g.type));
        o.set("value", g.value.toString());
        o.set("comment", g.comment);
        gvars.append(o);
    }
    root.set("globalVars", gvars);
    JsonValue pages;
    pages.makeArray();
    for (size_t i = 0; i < s.uiPages.size(); ++i) {
        const UiPage& page = s.uiPages[i];
        JsonValue o;
        o.makeObject();
        o.set("id", page.id);
        o.set("name", page.name);
        o.set("width", page.width);
        o.set("height", page.height);
        o.set("enabled", page.enabled);
        JsonValue ws;
        ws.makeArray();
        for (size_t k = 0; k < page.widgets.size(); ++k) {
            const UiWidget& w = page.widgets[k];
            JsonValue one;
            one.makeObject();
            one.set("id", w.id);
            one.set("kind", static_cast<int>(w.kind));
            one.set("x", w.x);
            one.set("y", w.y);
            one.set("w", w.w);
            one.set("h", w.h);
            one.set("text", w.text);
            one.set("dataPath", w.dataPath);
            one.set("action", w.action);
            ws.append(one);
        }
        o.set("widgets", ws);
        pages.append(o);
    }
    root.set("uiPages", pages);
    return root;
}

// 可存盘配方指纹，不含保存者、时间、软件版本和 paraVersion
std::string VisionCfgMgr::persistFingerprint(const Solution& s)
{
    Solution copy = s;
    clearPersistNoise(copy);
    return JsonValue::stringify(solutionToJson(copy, false), false);
}

// 存盘枚举必须已是具体值
static bool groupEnumsOk(const ToolGroup& g)
{
    if (g.kind <= GroupKindUndefined || g.kind >= GroupKindMax) {
        return false;
    }
    for (size_t i = 0; i < g.children.size(); ++i) {
        if (!groupEnumsOk(g.children[i])) {
            return false;
        }
    }
    return true;
}

static bool jobEnumsOk(const JobCfg& job)
{
    if (job.execMode < ExecActive || job.execMode >= ExecWayMax) {
        return false;
    }
    if (!groupEnumsOk(job.root)) {
        return false;
    }
    for (size_t i = 0; i < job.tools.size(); ++i) {
        const ToolCfg& tool = job.tools[i];
        if (tool.info.type <= ToolType::Undefined || tool.info.type >= ToolType::Max) {
            return false;
        }
        for (size_t k = 0; k < tool.param.roiItems.size(); ++k) {
            const RoiItemKind rk = tool.param.roiItems[k].kind;
            if (rk <= RoiItemKind::Undefined || rk >= RoiItemKind::Max) {
                return false;
            }
        }
    }
    return true;
}

static bool persistEnumsOk(const Solution& s, std::string* error)
{
    for (size_t i = 0; i < s.inventory.size(); ++i) {
        const HardwareDevice& d = s.inventory[i];
        if (d.kind <= DeviceKindUndefined || d.kind >= DeviceKindMax) {
            if (error) {
                *error = std::string(Translate::tr("类型未定义，不能保存"));
            }
            return false;
        }
        if (d.kind == DeviceCamera
            && (d.cameraBackend <= CameraBackendUndefined || d.cameraBackend > CameraBackendSdk)) {
            if (error) {
                *error = std::string(Translate::tr("类型未定义，不能保存"));
            }
            return false;
        }
    }
    for (size_t i = 0; i < s.commChannels.size(); ++i) {
        const CommChannelType ct = s.commChannels[i].type;
        if (ct <= CommChannelTypeUndefined || ct >= CommChannelTypeMax) {
            if (error) {
                *error = std::string(Translate::tr("类型未定义，不能保存"));
            }
            return false;
        }
    }
    for (size_t si = 0; si < s.stations.size(); ++si) {
        const StationCfg& st = s.stations[si];
        for (size_t ji = 0; ji < st.jobs.size(); ++ji) {
            if (!jobEnumsOk(st.jobs[ji])) {
                if (error) {
                    *error = std::string(Translate::tr("类型未定义，不能保存"));
                }
                return false;
            }
        }
    }
    for (size_t i = 0; i < s.composites.size(); ++i) {
        if (!jobEnumsOk(s.composites[i].inner)) {
            if (error) {
                *error = std::string(Translate::tr("类型未定义，不能保存"));
            }
            return false;
        }
    }
    return true;
}
// 方案存成 format=3 JSON 并原子落盘
bool VisionCfgMgr::saveSolution(const Solution& s, std::string* error)
{
    if (s.filePath.empty()) {
        if (error) {
            *error = std::string(Translate::tr("未指定方案路径"));
        }
        return false;
    }
    if (!persistEnumsOk(s, error)) {
        return false;
    }
    const JsonValue root = solutionToJson(s, true);
    if (!writeFileAtomic(s.filePath, JsonValue::stringify(root, true))) {
        if (error) {
            *error = std::string(Translate::tr("无法写入文件"));
        }
        return false;
    }
    // 旁路 vpdata：为每个工具建好数据目录
    if (!VisionCfgMgr::ensureSolutionDataDirs(s)) {
        if (error) {
            *error = std::string(Translate::tr("无法创建方案数据目录"));
        }
        return false;
    }
    return true;
}

// 为方案内全部工具确保 vpdata 子目录
bool VisionCfgMgr::ensureSolutionDataDirs(const Solution& s)
{
    const std::string root = solutionDataRootFromFile(s.filePath);
    if (root.empty()) {
        return true;
    }
    if (!ensureDirectoryTree(root)) {
        return false;
    }
    for (size_t si = 0; si < s.stations.size(); ++si) {
        const StationCfg& st = s.stations[si];
        for (size_t ji = 0; ji < st.jobs.size(); ++ji) {
            const JobCfg& job = st.jobs[ji];
            for (size_t ti = 0; ti < job.tools.size(); ++ti) {
                const ToolCfg& tool = job.tools[ti];
                const std::string dir = toolDataDir(s.filePath, st.id, job.id, tool.id);
                if (!dir.empty() && !ensureDirectoryTree(dir)) {
                    return false;
                }
            }
        }
    }
    return true;
}

// 读方案；format 大于 3 则拒绝。成功后 dirty 为 false
bool VisionCfgMgr::loadSolution(const std::string& path, Solution* s, std::string* error)
{
    if (!s) {
        return false;
    }
    std::string text;
    if (!readFileUtf8(path, &text)) {
        if (error) {
            *error = std::string(Translate::tr("无法打开文件"));
        }
        return false;
    }
    std::string perr;
    const JsonValue root = JsonValue::parse(text, &perr);
    if (!root.isObject()) {
        if (error) {
            *error = perr.empty() ? std::string(Translate::tr("方案文件不是 JSON 对象")) : perr;
        }
        return false;
    }
    const int format = root.value("format").toInt(1);
    if (format > 3) {
        if (error) {
            *error = std::string(Translate::tr("方案文件版本更高，请升级软件"));
        }
        return false;
    }
    s->filePath = path;
    s->name = root.value("name").toString();
    const JsonValue savedBy = root.value("lastSavedBy");
    s->lastSavedById = savedBy.value("id").toString();
    s->lastSavedByName = savedBy.value("name").toString();
    s->lastSavedAt = root.value("lastSavedAt").toString();
    s->platformVersion = root.value("platformVersion").toString();
    s->runtimeStationLayout = runtimeStationLayoutFromInt(root.value("runtimeStationLayout").toInt(0));
    // 设备台账：format 1 的旧种类整型需迁移；文件源相机统一归为文件夹设备
    s->inventory.clear();
    const JsonValue inv = root.value("inventory");
    for (int i = 0; i < inv.size(); ++i) {
        const JsonValue o = inv.at(i);
        HardwareDevice d;
        d.id = ObjectId(o.value("id").toString());
        d.name = o.value("name").toString();
        const int kindInt = o.value("kind").toInt();
        if (format < 2) {
            migrateLegacyDeviceKind(kindInt, &d.kind, &d.cameraBackend);
        } else {
            if (kindInt < 0 || kindInt >= static_cast<int>(DeviceKindMax)) {
                d.kind = DeviceKindUndefined;
            } else {
                d.kind = static_cast<DeviceKind>(kindInt);
            }
            if (o.contains("cameraBackend")) {
                const int back = o.value("cameraBackend").toInt(-1);
                if (back == static_cast<int>(CameraBackendFile) || back == static_cast<int>(CameraBackendSdk)) {
                    d.cameraBackend = static_cast<CameraBackend>(back);
                } else {
                    d.cameraBackend = CameraBackendUndefined;
                }
            }
            if (d.kind == DeviceCamera && d.cameraBackend == CameraBackendFile) {
                d.kind = DeviceFolder;
            }
        }
        d.defaultParams = paramsFromJson(o.value("params"));
        s->inventory.push_back(d);
    }
    s->commChannels.clear();
    const JsonValue chs = root.value("commChannels");
    for (int i = 0; i < chs.size(); ++i) {
        const JsonValue o = chs.at(i);
        CommChannel c;
        c.id = ObjectId(o.value("id").toString());
        c.name = o.value("name").toString();
        if (o.contains("type")) {
            c.type = commChannelTypeFromInt(o.value("type").toInt(-1));
        }
        c.enabled = o.value("enabled").toBool(true);
        c.params = paramsFromJson(o.value("params"));
        s->commChannels.push_back(c);
    }
    // 工站引用了不存在的通信通道时清掉引用
    s->stations.clear();
    const JsonValue sts = root.value("stations");
    for (int i = 0; i < sts.size(); ++i) {
        StationCfg st = stationFromJson(sts.at(i));
        if (!st.commChannelId.empty() && s->findCommChannel(ObjectId(st.commChannelId)) == 0) {
            st.commChannelId.clear();
        }
        s->stations.push_back(st);
    }
    s->composites.clear();
    const JsonValue comps = root.value("composites");
    for (int i = 0; i < comps.size(); ++i) {
        s->composites.push_back(compositeFromJson(comps.at(i)));
    }
    // 全局变量（缺键=空表）
    s->globalVars.clear();
    const JsonValue gvars = root.value("globalVars");
    if (gvars.isArray()) {
        std::vector<GlobalVar> loaded;
        for (int i = 0; i < gvars.size(); ++i) {
            const JsonValue o = gvars.at(i);
            GlobalVar g;
            g.name = o.value("name").toString();
            const int ty = o.value("type").toInt(0);
            if (ty < 0 || ty > 3) {
                g.type = GlobalVarInt;
            } else {
                g.type = static_cast<GlobalVarType>(ty);
            }
            g.comment = o.value("comment").toString();
            g.value = VarStore::valueFromText(g.type, o.value("value").toString());
            if (!g.name.empty()) {
                loaded.push_back(g);
            }
        }
        s->globalVars.setItems(loaded);
    }
    // 自定义 UI 页
    s->uiPages.clear();
    const JsonValue pages = root.value("uiPages");
    if (pages.isArray()) {
        for (int i = 0; i < pages.size(); ++i) {
            const JsonValue o = pages.at(i);
            UiPage page;
            page.id = o.value("id").toString();
            page.name = o.value("name").toString();
            page.width = o.value("width").toInt(800);
            page.height = o.value("height").toInt(600);
            page.enabled = o.value("enabled").toBool(true);
            const JsonValue ws = o.value("widgets");
            if (ws.isArray()) {
                for (int k = 0; k < ws.size(); ++k) {
                    const JsonValue one = ws.at(k);
                    UiWidget w;
                    w.id = one.value("id").toString();
                    w.kind = static_cast<UiWidgetKind>(one.value("kind").toInt(0));
                    w.x = one.value("x").toInt(0);
                    w.y = one.value("y").toInt(0);
                    w.w = one.value("w").toInt(120);
                    w.h = one.value("h").toInt(32);
                    w.text = one.value("text").toString();
                    w.dataPath = one.value("dataPath").toString();
                    w.action = one.value("action").toString();
                    page.widgets.push_back(w);
                }
            }
            s->uiPages.push_back(page);
        }
    }
    // 设备、通道等类型未定义则加载失败；此时 s 已被改写
    if (!persistEnumsOk(*s, error)) {
        return false;
    }
    s->dirty = false;
    return true;
}

// 路径是否为已存在的文件（UTF-8）
static bool fileExistsUtf8(const std::string& path)
{
    const DWORD a = GetFileAttributesW(utf8ToWide(path).c_str());
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

// 本机设置读写 settings.json
bool VisionCfgMgr::saveSettings(const AppSettings& s)
{
    AppPaths::ensureDirs();
    JsonValue o;
    o.makeObject();
    o.set("autoLoginOperator", s.autoLoginOperator);
    o.set("lastLoginName", s.lastLoginName);
    o.set("lastSolutionPath", s.lastSolutionPath);
    o.set("themeId", s.themeId.empty() ? std::string("industrial") : s.themeId);
    o.set("language", s.language.empty() ? std::string("zh-CN") : s.language);
    o.set("startupMode", static_cast<int>(s.startupMode));
    o.set("closeAction", static_cast<int>(s.closeAction));
    o.set("popupJobOk", s.popupJobOk);
    o.set("popupJobNg", s.popupJobNg);
    o.set("popupJobException", s.popupJobException);
    o.set("permScope", s.permScope);
    JsonValue nodes;
    nodes.makeArray();
    for (size_t i = 0; i < s.permNodes.size(); ++i) {
        JsonValue g;
        g.makeObject();
        g.set("id", s.permNodes[i].id);
        g.set("op", s.permNodes[i].op);
        g.set("eng", s.permNodes[i].eng);
        g.set("admin", s.permNodes[i].admin);
        nodes.append(g);
    }
    o.set("permNodes", nodes);
    return writeFileAtomic(AppPaths::settingsFile(), JsonValue::stringify(o, true));
}

// 读设置；损坏时仍用出厂默认
FileLoadStatus VisionCfgMgr::loadSettings(AppSettings* s, std::string* error)
{
    if (!s) {
        if (error) {
            *error = "settings is null";
        }
        return FileLoadCorrupt;
    }
    AppSettings def;
    *s = def;
    if (!fileExistsUtf8(AppPaths::settingsFile())) {
        if (error) {
            *error = std::string(Translate::tr("无法读取本机设置"));
        }
        return FileLoadMissing;
    }
    std::string text;
    if (!readFileUtf8(AppPaths::settingsFile(), &text)) {
        if (error) {
            *error = std::string(Translate::tr("无法读取本机设置"));
        }
        return FileLoadCorrupt;
    }
    std::string perr;
    const JsonValue o = JsonValue::parse(text, &perr);
    if (!o.isObject()) {
        if (error) {
            *error = perr.empty() ? std::string(Translate::tr("本机设置不是 JSON 对象")) : perr;
        }
        return FileLoadCorrupt;
    }
    s->autoLoginOperator = o.value("autoLoginOperator").toBool(false);
    s->lastLoginName = o.value("lastLoginName").toString();
    s->lastSolutionPath = o.value("lastSolutionPath").toString();
    s->themeId = o.value("themeId").toString();
    if (s->themeId.empty()) {
        s->themeId = "industrial";
    }
    s->language = o.value("language").toString();
    if (s->language.empty()) {
        s->language = "zh-CN";
    }
    s->startupMode = startupAppModeFromInt(o.value("startupMode").toInt(static_cast<int>(StartupModeDesign)));
    s->closeAction = closeMainWindowActionFromInt(o.value("closeAction").toInt(static_cast<int>(CloseActionExit)));
    s->popupJobOk = o.value("popupJobOk").toBool(false);
    s->popupJobNg = o.value("popupJobNg").toBool(false);
    s->popupJobException = o.value("popupJobException").toBool(false);
    // tray not implemented: force exit
    if (s->closeAction == CloseActionMinimizeTray) {
        s->closeAction = CloseActionExit;
    }
    s->permScope = o.value("permScope").toInt(PermScopeUnified);
    if (s->permScope != PermScopeInstance) {
        s->permScope = PermScopeUnified;
    }
    s->permNodes.clear();
    const JsonValue nodes = o.value("permNodes");
    for (int i = 0; i < nodes.size(); ++i) {
        const JsonValue g = nodes.at(i);
        if (!g.isObject()) {
            continue;
        }
        AppSettings::PermNodeGrant row;
        row.id = g.stringOf("id");
        if (row.id.empty()) {
            continue;
        }
        row.op = g.flagOf("op", true);
        row.eng = g.flagOf("eng", true);
        row.admin = g.flagOf("admin", true);
        s->permNodes.push_back(row);
    }
    if (o.contains("permOperator") || o.contains("permEngineer") || o.contains("permAdmin")
        || o.contains("operatorCanResetCount")) {
        uint32_t op = static_cast<uint32_t>(o.value("permOperator").toDouble());
        uint32_t eng = static_cast<uint32_t>(o.value("permEngineer").toDouble());
        uint32_t admin = static_cast<uint32_t>(o.value("permAdmin").toDouble());
        if (!o.contains("permOperator")) {
            op = permissionsForRole(RoleOperator,
                                    o.value("operatorCanResetCount").toBool(false),
                                    o.value("operatorCanSwitchSolution").toBool(false));
            eng = defaultPerms(RoleEngineer);
            admin = defaultPerms(RoleAdmin);
        }
        migrateLogPermBits(&op, &eng, &admin);
        op |= lockedPerms(RoleOperator);
        eng |= lockedPerms(RoleEngineer);
        admin |= lockedPerms(RoleAdmin);
        s->importLegacyMasks(op, eng, admin);
    }
    return FileLoadOk;
}

// load settings; on failure still return factory defaults
AppSettings VisionCfgMgr::loadSettings()
{
    AppSettings s;
    loadSettings(&s, 0);
    return s;
}

JsonValue compositeExportJob(const JobCfg& j)
{
    return jobToJson(j);
}
JobCfg compositeImportJob(const JsonValue& o)
{
    return jobFromJson(o);
}

}




