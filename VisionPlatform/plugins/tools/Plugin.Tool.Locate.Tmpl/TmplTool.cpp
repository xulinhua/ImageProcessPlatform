/**
 * @file TmplTool.cpp
 * @brief 模板定位：登记参数，执行交给算法插件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "TmplTool.h"
#include "SolutionDataPaths.h"
#include "VisionVersion.h"
#include <HsCommData/Graphic.h>
#include <HsCommData/HsError.h>
#include <HsCommData/HsException.h>
#include <HsCommData/HsConst.h>
#include <cstdio>
#include <vector>

namespace {
const char kToolVersion[] = "1.0.0.0"; ///< 当前版本。必须等于变更表最后一行
const Vision::VerNoteRaw kVerNotes[] = {
    { "1.0.0.0", "Initial version" },
};
const char kToolName[] = "locate.template"; ///< 注册名
const char kDisplayName[] = "Template"; ///< 界面显示名
const char kDescription[] = "NCC or shape template locate. Model is stored beside the solution. Output is a pose";

Vision::ParamMap defaultsFrom(const std::vector<Vision::ParamField>& fields)
{
    Vision::ParamMap m;
    for (size_t i = 0; i < fields.size(); ++i) {
        if (fields[i].type == Vision::ParamFieldType::Device) {
            continue;
        }
        m.insert(fields[i].key, fields[i].def);
    }
    return m;
}

void fillSlot(Vision::RoiSlotRule* slot)
{
    slot->maxCount = 1;
    slot->shapes.push_back(Vision::RoiShapeRule(HsCommData::ObjRect1, 0));
    slot->shapes.push_back(Vision::RoiShapeRule(HsCommData::ObjRect2, 0));
    slot->shapes.push_back(Vision::RoiShapeRule(HsCommData::ObjCircle, 0));
    slot->shapes.push_back(Vision::RoiShapeRule(HsCommData::ObjEllipse, 0));
    slot->shapes.push_back(Vision::RoiShapeRule(HsCommData::ObjPoly, 0));
    slot->shapes.push_back(Vision::RoiShapeRule(HsCommData::ObjRegion, 0));
    slot->itemKinds.push_back(Vision::RoiItemKind::DrawRoi);
    slot->itemKinds.push_back(Vision::RoiItemKind::DrawRegion);
}
}

// 骨架
Vision::TmplTool::TmplTool()
{
}

Vision::TmplTool::~TmplTool()
{
}

// 克隆本实例（本 DLL 内 new）
Vision::ToolBase* Vision::TmplTool::clone() const
{
    TmplTool* p = new TmplTool();
    p->copyInstanceDataFrom(*this);
    p->image_org_ = image_org_;
    p->image_org_ok_ = image_org_ok_;
    p->data_dir_ = data_dir_;
    return p;
}

// 描述、版本
std::string Vision::TmplTool::tool_name() const
{
    return kToolName;
}

// 工具大类
Vision::ToolType Vision::TmplTool::type() const
{
    return Vision::ToolType::Locate;
}

// 界面显示名
std::string Vision::TmplTool::display_name() const
{
    return kDisplayName;
}

// 工具说明
std::string Vision::TmplTool::description() const
{
    return kDescription;
}

// 工具版本
std::string Vision::TmplTool::version() const
{
    return std::string(kToolVersion);
}

// 版本变更，只追加
std::vector<Vision::VerNote> Vision::TmplTool::version_notes() const
{
    return Vision::verNotesFrom(kVerNotes, static_cast<int>(sizeof(kVerNotes) / sizeof(kVerNotes[0])));
}

// 缺省参数
Vision::ParamMap Vision::TmplTool::default_params() const
{
    Vision::ParamMap m = defaultsFrom(cfg_teach());
    const Vision::ParamMap run = defaultsFrom(cfg_param());
    const std::vector<std::string> keys = run.keys();
    for (size_t i = 0; i < keys.size(); ++i) {
        m.insert(keys[i], run.value(keys[i]));
    }
    return m;
}

// 运行参数说明
std::vector<Vision::ParamField> Vision::TmplTool::cfg_param() const
{
    std::vector<Vision::ParamField> f;
    f.push_back(Vision::paramDouble(Vision::TmplKey::minScore, "Min score", 0.5, true, 0.0, true, 1.0)); // 最低分数
    f.push_back(Vision::paramInt(Vision::TmplKey::numMatches, "Match count", 1, true, 1, true, 999)); // 匹配个数
    f.push_back(Vision::paramDouble(Vision::TmplKey::maxOverlap, "Max overlap", 0.5, true, 0.0, true, 1.0)); // 最大重叠
    f.push_back(Vision::paramString(Vision::TmplKey::subPixel, "Subpixel", "least_squares",
        Vision::paramChoices("none", "interpolation", "least_squares", "least_squares_high"))); // 亚像素
    f.push_back(Vision::paramInt(Vision::TmplKey::findLevels, "Find levels", 0, true, 0, true, 10)); // 定位层数。0 用模板层数
    f.push_back(Vision::paramDouble(Vision::TmplKey::greediness, "Greediness", 0.9, true, 0.0, true, 1.0)); // 形状模板贪婪度
    f.back().showKey = Vision::TmplKey::method;
    f.back().showValues.push_back(Vision::tmplMethodName(Vision::TmplMethod::Shape));
    return f;
}

// 示教参数说明
std::vector<Vision::ParamField> Vision::TmplTool::cfg_teach() const
{
    std::vector<Vision::ParamField> f;
    f.push_back(Vision::paramString(Vision::TmplKey::method, "Method", Vision::tmplMethodName(Vision::TmplMethod::Ncc),
        Vision::paramChoices(Vision::tmplMethodName(Vision::TmplMethod::Ncc), Vision::tmplMethodName(Vision::TmplMethod::Shape)))); // 建模方式
    f.push_back(Vision::paramDouble(Vision::TmplKey::angleRange, "Angle range", 15.0, true, 0.0, true, 180.0)); // 角度范围，度
    f.push_back(Vision::paramString(Vision::TmplKey::angleStep, "Angle step", "auto", Vision::paramChoices("auto"))); // 角度步长，度。auto 或一个角度
    f.push_back(Vision::paramString(Vision::TmplKey::numLevels, "Num levels", "auto", Vision::paramChoices("auto"))); // 金字塔层数。auto 或 1 到 10
    f.push_back(Vision::paramString(Vision::TmplKey::metric, "Metric", "use_polarity",
        Vision::paramChoices("use_polarity", "ignore_global_polarity", "ignore_local_polarity", "ignore_color_polarity"))); // 极性
    f.push_back(Vision::paramString(Vision::TmplKey::optimization, "Optimization", "auto",
        Vision::paramChoices("auto", "none", "point_reduction_low", "point_reduction_medium"))); // 形状模板优化
    f.back().choices.push_back("point_reduction_high");
    f.back().showKey = Vision::TmplKey::method;
    f.back().showValues.push_back(Vision::tmplMethodName(Vision::TmplMethod::Shape));
    f.push_back(Vision::paramString(Vision::TmplKey::contrast, "Contrast", "30",
        Vision::paramChoices("auto", "auto_contrast", "auto_contrast_hyst"))); // 对比度。词、一个数，或下限,上限
    f.back().liveAction = Vision::TmplKey::inspContour;
    f.back().showKey = Vision::TmplKey::method;
    f.back().showValues.push_back(Vision::tmplMethodName(Vision::TmplMethod::Shape));
    f.push_back(Vision::paramString(Vision::TmplKey::minContrast, "Min contrast", "auto", Vision::paramChoices("auto"))); // 最小对比度。auto 或 1 到 255
    f.back().showKey = Vision::TmplKey::method;
    f.back().showValues.push_back(Vision::tmplMethodName(Vision::TmplMethod::Shape));
    f.push_back(Vision::paramBool(Vision::TmplKey::rebuildModel, "Rebuild model", false)); // 重新建模
    return f;
}

// 本工具参数对象
const Vision::ToolParaBase* Vision::TmplTool::tool_param() const
{
    return &param_;
}

// load 之后核对建模方式和键表
int Vision::TmplTool::check_param(std::string* detail) const
{
    if (param_.loadCode() != HS_OK) {
        if (detail != nullptr) {
            *detail = HsCommData::HsException(param_.loadCode()).errorMessage();
        }
        return param_.loadCode();
    }
    return HS_OK;
}

// 输出项
Vision::OutputSpec Vision::TmplTool::cfg_output() const
{
    Vision::OutputSpec spec;
    Vision::OutputField score;
    score.key = Vision::TmplKey::score;
    score.title = Vision::TmplKey::score;
    spec.fields.push_back(score); // 匹配分数
    Vision::OutputField count;
    count.key = Vision::TmplKey::count;
    count.title = Vision::TmplKey::count;
    spec.fields.push_back(count); // 匹配个数
    return spec;
}

// 统计规格
Vision::TolSpec Vision::TmplTool::cfg_tol() const
{
    Vision::TolSpec spec;
    Vision::TolField dist;
    dist.key = Vision::TmplKey::offset;
    dist.title = Vision::TmplKey::offset;
    dist.defMin = 0;
    dist.defMax = 14;
    spec.items.push_back(dist); // 偏移距离
    Vision::TolField ox;
    ox.key = Vision::TmplKey::offsetX;
    ox.title = Vision::TmplKey::offsetX;
    ox.parent = Vision::TmplKey::offset;
    ox.defMin = -10;
    ox.defMax = 10;
    spec.items.push_back(ox); // 偏移 X
    Vision::TolField oy;
    oy.key = Vision::TmplKey::offsetY;
    oy.title = Vision::TmplKey::offsetY;
    oy.parent = Vision::TmplKey::offset;
    oy.defMin = -10;
    oy.defMax = 10;
    spec.items.push_back(oy); // 偏移 Y
    Vision::TolField oa;
    oa.key = Vision::TmplKey::offsetAngle;
    oa.title = Vision::TmplKey::offsetAngle;
    oa.parent = Vision::TmplKey::offset;
    oa.defMin = -0.2;
    oa.defMax = 0.2;
    spec.items.push_back(oa); // 偏移角度
    return spec;
}

// 显示规格
Vision::DisplaySpec Vision::TmplTool::cfg_display() const
{
    Vision::DisplaySpec spec;
    Vision::GraphicSlot teach;
    teach.key = Vision::TmplKey::teachRoi;
    teach.title = Vision::TmplKey::teachRoi;
    teach.defEnable = true;
    teach.region = true;
    spec.graphics.push_back(teach); // 示教区
    Vision::GraphicSlot result;
    result.key = Vision::TmplKey::resultRoi;
    result.title = Vision::TmplKey::resultRoi;
    result.defEnable = true;
    result.region = true;
    spec.graphics.push_back(result); // 结果区域
    Vision::GraphicSlot follow;
    follow.key = Vision::TmplKey::followRoi;
    follow.title = Vision::TmplKey::followRoi;
    follow.defEnable = true;
    follow.region = true;
    spec.graphics.push_back(follow); // 跟随区
    Vision::TextSlot pose;
    pose.key = Vision::TmplKey::pose;
    pose.title = Vision::TmplKey::pose;
    pose.defAnchor = Vision::TextAnchor::ResultPoint;
    pose.defEnable = true;
    spec.texts.push_back(pose); // 位姿文字
    Vision::TextSlot verdict;
    verdict.key = Vision::TmplKey::verdict;
    verdict.title = Vision::TmplKey::verdict;
    verdict.defAnchor = Vision::TextAnchor::ImageCorner;
    verdict.defEnable = true;
    spec.texts.push_back(verdict); // 判定文字
    return spec;
}

// 画布 ROI 等能力。示教、搜索、跟随都是矩形、角度矩形、圆、椭圆、多边形、区域
Vision::ToolCaps Vision::TmplTool::cfg_roi() const
{
    Vision::ToolCaps c;
    Vision::RoiSlotRule search;
    search.slot = Vision::TmplKey::search;
    search.title = "Search";
    fillSlot(&search);
    c.slotRules.push_back(search); // 搜索区
    Vision::RoiSlotRule follow;
    follow.slot = Vision::TmplKey::follow;
    follow.title = "Follow";
    fillSlot(&follow);
    c.slotRules.push_back(follow); // 跟随区
    Vision::RoiSlotRule teach;
    teach.slot = Vision::TmplKey::teach;
    teach.title = "Teach";
    fillSlot(&teach);
    c.slotRules.push_back(teach); // 示教区
    return c;
}

// 调算法插件执行
int Vision::TmplTool::execute(const Vision::ToolIn& in, Vision::ToolOut* out)
{
    return shot(in, std::string(), out);
}

// 调试动作
std::vector<Vision::ToolAction> Vision::TmplTool::cfg_action() const
{
    std::vector<Vision::ToolAction> actions;
    Vision::ToolAction teachAct;
    teachAct.id = Vision::TmplKey::teachModel;
    teachAct.title = "Teach model";
    teachAct.place = Vision::ActionPlace::Teach;
    actions.push_back(teachAct); // 学习模板
    Vision::ToolAction contour;
    contour.id = Vision::TmplKey::inspContour;
    contour.title = "Contour preview";
    contour.place = Vision::ActionPlace::Teach;
    contour.live = true;
    actions.push_back(contour); // 查看轮廓
    return actions;
}

// 按动作 id 执行调试动作
int Vision::TmplTool::action(const std::string& id, const Vision::ToolIn& in, Vision::ToolOut* out)
{
    if (out == nullptr) {
        return HS_ERR_NULL;
    }
    if (id == Vision::TmplKey::teachModel) {
        return teach_model(in, out);
    }
    if (id == Vision::TmplKey::inspContour) {
        return inspect_contour(in, out);
    }
    return Vision::ToolBase::action(id, in, out);
}

// 保存原图，再让算法保存本库模型。返回错误码
int Vision::TmplTool::save()
{
    if (image_org_ok_ && !data_dir_.empty()) {
        ensureDirectoryTree(data_dir_);
        if (!image_org_.save(data_dir_ + "\\image_org", HsCommData::ImageFormatPng)) {
            return HS_ERR_FILE_IO;
        }
    }
    return ToolBase::save();
}

// 丢掉示教原图并清掉本工具数据目录
void Vision::TmplTool::clear(const std::string& toolDataDir)
{
    image_org_ = HsCommData::HsImage();
    image_org_ok_ = false;
    data_dir_.clear();
    ToolBase::clear(toolDataDir);
}

// 把每个定位结果写入日志
void Vision::TmplTool::log_result(Vision::ToolOut* out) const
{
    if (out == nullptr) {
        return;
    }
    if (out->io.poses.empty()) {
        Vision::ToolTrace trace;
        trace.action = "结果";
        trace.detail = out->message.empty() ? "无定位结果" : out->message;
        out->traces.push_back(trace);
        return;
    }
    const bool haveScore = out->values.contains(TmplKey::score);
    const double score = haveScore ? out->values.value(TmplKey::score).toDouble(0) : 0;
    for (size_t i = 0; i < out->io.poses.size(); ++i) {
        const HsCommData::Pose2& pose = out->io.poses[i].pose;
        const double deg = pose.angle * 180.0 / HsCommData::kPi;
        char line[160];
        if (i == 0 && haveScore) {
            sprintf_s(line, "目标%u : X %.2f  Y %.2f  Angle %.2f  Score %.2f",
                static_cast<unsigned>(i), pose.tx, pose.ty, deg, score);
        } else {
            sprintf_s(line, "目标%u : X %.2f  Y %.2f  Angle %.2f",
                static_cast<unsigned>(i), pose.tx, pose.ty, deg);
        }
        Vision::ToolTrace trace;
        trace.action = "结果";
        trace.detail = line;
        out->traces.push_back(trace);
    }
}

// 在结果位置补十字
void Vision::TmplTool::disp_extra(const Vision::ToolIn&, Vision::ToolOut* out) const
{
    if (out == nullptr) {
        return;
    }
    for (size_t i = 0; i < out->io.poses.size(); ++i) {
        const HsCommData::Pose2& pose = out->io.poses[i].pose;
        HsCommData::Graphic cross;
        cross.kind = HsCommData::KindCross;
        HsCommData::HsCross one;
        one.center = HsCommData::Point2d(pose.tx, pose.ty);
        one.size = 12.0;
        one.angle = pose.angle;
        cross.cross.push_back(one);
        std::vector<HsCommData::Graphic> gfx;
        gfx.push_back(cross);
        out->io.appendFromGraphics(gfx);
    }
}

// 学习模板。错误码返回，结果写入 out
int Vision::TmplTool::teach_model(const Vision::ToolIn& in, Vision::ToolOut* out)
{
    const int code = shot(in, Vision::TmplKey::teachModel, out);
    if (out != nullptr && out->errCode == HS_OK) {
        keep_org_image(in);
    }
    return code;
}

// 查看轮廓。错误码返回，结果写入 out
int Vision::TmplTool::inspect_contour(const Vision::ToolIn& in, Vision::ToolOut* out)
{
    return shot(in, Vision::TmplKey::inspContour, out);
}

// 示教成功后留下整图，点保存时写入工具目录
void Vision::TmplTool::keep_org_image(const Vision::ToolIn& in)
{
    HsCommData::HsImage shotImage;
    if (!in.input.isNull()) {
        shotImage = in.input;
    } else if (!in.inputIo.images.empty()) {
        shotImage = in.inputIo.images[0].image;
    }
    if (shotImage.isNull()) {
        return;
    }
    image_org_ = shotImage;
    image_org_ok_ = true;
    if (!in.toolDataDir.empty()) {
        data_dir_ = in.toolDataDir;
    }
}

void Vision::TmplTool::fill_alg_in(const Vision::ToolIn& in, Vision::AlgToolIn* ain) const
{
    ain->mode = in.mode;
    if (!in.inputIo.images.empty()) {
        ain->imagePtr = &in.inputIo.images[0].image;
        ain->imageNative = in.inputIo.images[0].native;
    } else {
        ain->imagePtr = &in.input;
        ain->imageNative = in.inputNative;
    }
    if (in.ownRoisResolved) {
        ain->rois = in.ownRois;
    } else {
        for (size_t i = 0; i < in.rois.size(); ++i) {
            Vision::PortRoi one;
            one.roi = in.rois[i];
            one.port = Vision::TmplKey::search;
            one.index = static_cast<int>(i);
            ain->rois.push_back(one);
        }
    }
    ain->param = &param_;
    ain->run = in.param.run;
    ain->teach = in.param.learn;
    ain->judge = in.param.judge;
    ain->solutionDir = in.solutionDir;
    ain->toolDataDir = in.toolDataDir;
}

int Vision::TmplTool::shot(const Vision::ToolIn& in, const std::string& actionId, Vision::ToolOut* out)
{
    if (out == nullptr) {
        return HS_ERR_NULL;
    }
    *out = Vision::ToolOut();
    attachAlg(in.toolDataDir);
    param_.load(in.param);
    if (param_.loadCode() != HS_OK) {
        out->ok = false;
        out->errCode = param_.loadCode();
        out->message = HsCommData::HsException(param_.loadCode()).errorMessage();
        return out->errCode;
    }
    Vision::AlgToolBase* alg = attachedAlg();
    if (alg == nullptr) {
        out->ok = false;
        out->errCode = HS_ERR_NO_ALG;
        out->message = "Algorithm not registered";
        return out->errCode;
    }
    Vision::AlgToolIn ain;
    fill_alg_in(in, &ain);
    Vision::AlgToolOut algOut;
    alg->shot_init(ain, &algOut);
    const int code = actionId.empty()
        ? alg->run(Vision::ExecMode::Auto, ain, &algOut)
        : alg->action(actionId, ain, &algOut);
    if (code != HS_OK && algOut.errCode == HS_OK) {
        algOut.errCode = code;
        algOut.ok = false;
    }
    out->ok = algOut.ok;
    out->errCode = algOut.errCode;
    out->error = algOut.errCode != HS_OK;
    out->message = algOut.message;
    out->values = algOut.values;
    out->objects = algOut.objects;
    out->traces = algOut.traces;
    for (size_t i = 0; i < algOut.poses.size(); ++i) {
        Vision::PortPose one;
        one.index = static_cast<int>(i);
        one.pose = algOut.poses[i];
        out->io.poses.push_back(one);
    }
    out->syncIo();
    log_result(out);
    if (!Vision::toolTypeIsImageSource(type())) {
        out->io.images.clear();
    }
    return out->errCode;
}

std::vector<Vision::ParamField> Vision::TmplTool::paramFields() const
{
    std::vector<Vision::ParamField> f = cfg_teach();
    const std::vector<Vision::ParamField> run = cfg_param();
    f.insert(f.end(), run.begin(), run.end());
    return f;
}

Vision::ToolOut Vision::TmplTool::execute(const Vision::ToolIn& in)
{
    Vision::ToolOut out;
    execute(in, &out);
    return out;
}

Vision::ToolOut Vision::TmplTool::teach(const Vision::ToolIn& in)
{
    Vision::ToolOut out;
    teach_model(in, &out);
    return out;
}
