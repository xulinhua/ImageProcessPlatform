/**
 * @file HalTmpl.cpp
 * @brief NCC 与形状模板定位（Halcon）
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "HalTmpl.h"
#include "HalImageConvert.h"
#include "HalObjectConvert.h"
#include "HalOps.h"
#include "SolutionDataPaths.h"
#include "VisionVersion.h"
#include <HsCommData/HsConst.h>
#include <HsCommData/HsError.h>
#include <HsCommData/HsException.h>
#include <cmath>
#include <cstdio>

namespace Vision {

namespace {
const char kVer[] = "1.0.0.0";
const char kName[] = "locate.template";
const VerNoteRaw kNotes[] = {
    { "1.0.0.0", "Initial version" },
};
}

std::string HalTmpl::tool_name() const { return kName; }
AlgLibKind HalTmpl::alg_lib() const { return AlgLibHal; }
AlgToolBase* HalTmpl::clone() const { return new HalTmpl(); }
std::string HalTmpl::version() const { return kVer; }
std::vector<VerNote> HalTmpl::version_notes() const
{
    return verNotesFrom(kNotes, static_cast<int>(sizeof(kNotes) / sizeof(kNotes[0])));
}

HalTmpl::HalTmpl()
    : origin_row_(0)
    , origin_col_(0)
    , img_w_(0)
    , img_h_(0)
    , shot_err_(0)
    , built_(false)
    , pose_org_ok_(false)
    , origin_row_keep_(0)
    , origin_col_keep_(0)
    , teach_keep_(false)
{
    shape_model_ = -1;
    ncc_model_ = -1;
    tmp_shape_model_ = -1;
    tmp_ncc_model_ = -1;
    HalconCpp::GenEmptyObj(&image_);
    HalconCpp::GenEmptyObj(&image_teach_);
    HalconCpp::GenEmptyObj(&search_region_);
    HalconCpp::GenEmptyObj(&teach_region_);
    HalconCpp::GenEmptyObj(&follow_region_);
    HalconCpp::GenEmptyObj(&result_region_);
    HalconCpp::GenEmptyObj(&ref_region_);
    HalconCpp::GenEmptyObj(&image_tpl_keep_);
    HalconCpp::GenEmptyObj(&region_teach_keep_);
}

HalTmpl::~HalTmpl()
{
    clearShapeModel(shape_model_);
    clearNccModel(ncc_model_);
    clearShapeModel(tmp_shape_model_);
    clearNccModel(tmp_ncc_model_);
}

int HalTmpl::tool_reset()
{
    matches_.clear();
    tol_.reset();
    HalconCpp::GenEmptyObj(&result_region_);
    HalconCpp::GenEmptyObj(&ref_region_);
    shot_err_ = 0;
    shot_msg_.clear();
    built_ = false;
    return HS_OK;
}

void HalTmpl::take_region(const AlgToolIn& in, const char* port, HalconCpp::HObject* region)
{
    HalconCpp::GenEmptyObj(region);
    const std::string name = port != nullptr ? port : "";
    const bool search = name == TmplKey::search;
    // 先按用途分别收集普通区和屏蔽区，转换失败或为空的块跳过
    HalconCpp::HObject normals;
    HalconCpp::HObject masks;
    bool anyNormal = false;
    bool anyMask = false;
    for (size_t i = 0; i < in.rois.size(); ++i) {
        const PortRoi& one = in.rois[i];
        if (one.use == RegionUse::Invalid) {
            continue;
        }
        const bool hit = one.port == name
            || (search && (one.port.empty() || one.port == "roi"));
        if (!hit) {
            continue;
        }
        HalconCpp::HObject piece;
        try {
            piece = hsObjectToHObject(one.roi, img_w_, img_h_);
        } catch (const HalconCpp::HException&) {
            continue;
        }
        if (hoIsEmpty(piece)) {
            continue;
        }
        HalconCpp::HObject* acc = one.use == RegionUse::Mask ? &masks : &normals;
        bool* any = one.use == RegionUse::Mask ? &anyMask : &anyNormal;
        if (!*any) {
            *acc = piece;
            *any = true;
        } else {
            HalconCpp::HObject joined;
            HalconCpp::ConcatObj(*acc, piece, &joined);
            *acc = joined;
        }
    }
    // 普通区并集减去屏蔽区并集；只有屏蔽区时结果为空
    if (!anyNormal) {
        return;
    }
    HalconCpp::HObject merged;
    HalconCpp::Union1(normals, &merged);
    if (anyMask) {
        HalconCpp::HObject cover;
        HalconCpp::Union1(masks, &cover);
        HalconCpp::HObject left;
        HalconCpp::Difference(merged, cover, &left);
        merged = left;
    }
    *region = merged;
}

int HalTmpl::proc_input(const AlgToolIn& in)
{
    HalconCpp::HObject gray;
    int w = 0;
    int h = 0;
    std::string err;
    HalconCpp::HObject* pGray = &gray;
    if (!inputToGray(in.imageNative.ptr, static_cast<int>(in.imageNative.algLib), inputImage(in), pGray, &w, &h, &err)) {
        shot_err_ = HS_ERR_IMAGE_EMPTY;
        shot_msg_ = err;
        return shot_err_;
    }
    if (w < 1 || h < 1) {
        shot_err_ = HS_ERR_IMAGE_EMPTY;
        shot_msg_ = HsCommData::HsException(HS_ERR_IMAGE_EMPTY).errorMessage();
        return shot_err_;
    }
    image_ = gray;
    img_w_ = w;
    img_h_ = h;
    if (toolParaAs<ToolParaTmpl>(in.param) == nullptr) {
        shot_err_ = HS_ERR_TYPE;
        shot_msg_ = HsCommData::HsException(HS_ERR_TYPE).errorMessage();
        return shot_err_;
    }
    take_region(in, TmplKey::search, &search_region_);
    take_region(in, TmplKey::teach, &teach_region_);
    take_region(in, TmplKey::follow, &follow_region_);
    if (regionHasArea(teach_region_)) {
        HalconCpp::HObject reduced;
        HalconCpp::ReduceDomain(image_, teach_region_, &reduced);
        HalconCpp::CropDomain(reduced, &image_teach_);
        HalconCpp::HTuple area, row, col;
        HalconCpp::AreaCenter(teach_region_, &area, &row, &col);
        origin_row_ = row[0].D();
        origin_col_ = col[0].D();
    } else {
        HalconCpp::GenEmptyObj(&image_teach_);
    }
    return HS_OK;
}

HalconCpp::HTuple HalTmpl::create_levels(const NumOrString& levels) const
{
    if (levels.form == NumOrString::Word) {
        return HalconCpp::HTuple(levels.word.c_str());
    }
    return HalconCpp::HTuple(static_cast<int>(std::lround(levels.low)));
}

HalconCpp::HTuple HalTmpl::angle_step_of(const NumOrString& step) const
{
    if (step.form == NumOrString::Word) {
        return HalconCpp::HTuple(step.word.c_str());
    }
    return HalconCpp::HTuple(step.low * HsCommData::kPi / 180.0);
}

HalconCpp::HTuple HalTmpl::min_contrast_of(const NumOrString& contrast) const
{
    if (contrast.form == NumOrString::Word) {
        return HalconCpp::HTuple(contrast.word.c_str());
    }
    return HalconCpp::HTuple(static_cast<int>(std::lround(contrast.low)));
}

std::string HalTmpl::ncc_metric(const std::string& metric) const
{
    if (metric == "ignore_global_polarity") {
        return metric;
    }
    return "use_polarity";
}

std::string HalTmpl::ncc_sub_pixel(const std::string& sub) const
{
    if (sub == "none" || sub == "false") {
        return "false";
    }
    return "true";
}

HalconCpp::HTuple HalTmpl::contrast_tuple(const NumOrString& contrast) const
{
    if (contrast.form == NumOrString::Word) {
        return HalconCpp::HTuple(contrast.word.c_str());
    }
    if (contrast.form == NumOrString::Pair) {
        return HalconCpp::HTuple(contrast.low).Append(HalconCpp::HTuple(contrast.high));
    }
    return HalconCpp::HTuple(static_cast<int>(std::lround(contrast.low)));
}

int HalTmpl::build_model(const AlgToolIn& in)
{
    const ToolParaTmpl* para = toolParaAs<ToolParaTmpl>(in.param);
    if (para == nullptr) {
        shot_err_ = HS_ERR_TYPE;
        shot_msg_ = HsCommData::HsException(HS_ERR_TYPE).errorMessage();
        return shot_err_;
    }
    if (!regionHasArea(image_teach_)) {
        shot_err_ = HS_ERR_TMPL_NO_TEACH;
        shot_msg_ = HsCommData::HsException(HS_ERR_TMPL_NO_TEACH).errorMessage();
        return shot_err_;
    }
    const double angleRad = para->teach.angleRad;
    const bool shape = para->teach.method == TmplMethod::Shape;
    const HalconCpp::HTuple levels = create_levels(para->teach.numLevels);
    const HalconCpp::HTuple step = angle_step_of(para->teach.angleStep);
    if (shape) {
        clearShapeModel(tmp_shape_model_);
        HalconCpp::CreateShapeModel(image_teach_, levels, -angleRad, 2.0 * angleRad, step,
            HalconCpp::HTuple(para->teach.optimization.c_str()), para->teach.metric.c_str(),
            contrast_tuple(para->teach.contrast), min_contrast_of(para->teach.minContrast),
            &tmp_shape_model_);
    } else {
        clearNccModel(tmp_ncc_model_);
        HalconCpp::CreateNccModel(image_teach_, levels, -angleRad, 2.0 * angleRad, step,
            ncc_metric(para->teach.metric).c_str(), &tmp_ncc_model_);
    }
    built_ = true;
    keep_teach_snapshot();
    return HS_OK;
}

std::string HalTmpl::lib_dir(const std::string& toolDataDir) const
{
    return toolDataDir + "\\hal";
}

void HalTmpl::keep_teach_snapshot()
{
    image_tpl_keep_ = image_teach_;
    region_teach_keep_ = teach_region_;
    origin_row_keep_ = origin_row_;
    origin_col_keep_ = origin_col_;
    teach_keep_ = true;
}

int HalTmpl::locate(const AlgToolIn& in, const HalconCpp::HTuple& model, bool shape)
{
    const ToolParaTmpl* para = toolParaAs<ToolParaTmpl>(in.param);
    if (para == nullptr) {
        shot_err_ = HS_ERR_TYPE;
        shot_msg_ = HsCommData::HsException(HS_ERR_TYPE).errorMessage();
        return shot_err_;
    }
    matches_.clear();
    HalconCpp::HObject reduced;
    const HalconCpp::HObject* src = &image_;
    if (regionHasArea(search_region_)) {
        HalconCpp::ReduceDomain(image_, search_region_, &reduced);
        src = &reduced;
    }
    // 角度范围与建模一致；重叠度和贪婪度夹到 0~1
    HalconCpp::HTuple row, col, ang, score;
    const double angleRad = para->teach.angleRad;
    const double minScore = para->run.minScore;
    const int matchNum = para->run.numMatches < 1 ? 1 : para->run.numMatches;
    double overlap = para->run.maxOverlap;
    if (overlap < 0.0) overlap = 0.0;
    if (overlap > 1.0) overlap = 1.0;
    double greed = para->run.greediness;
    if (greed < 0.0) greed = 0.0;
    if (greed > 1.0) greed = 1.0;
    if (shape) {
        HalconCpp::FindShapeModel(*src, model, -angleRad, 2.0 * angleRad, minScore, matchNum, overlap,
            para->run.subPixel.c_str(), para->run.findLevels, greed, &row, &col, &ang, &score);
    } else {
        HalconCpp::FindNccModel(*src, model, -angleRad, 2.0 * angleRad, minScore, matchNum, overlap,
            ncc_sub_pixel(para->run.subPixel).c_str(), para->run.findLevels, &row, &col, &ang, &score);
    }
    // 匹配中心落在图外或搜索区外的结果丢掉
    HalconCpp::HTuple inside;
    if (regionHasArea(search_region_) && row.TupleLength() > 0) {
        HalconCpp::TestRegionPoint(search_region_, row, col, &inside);
    }
    const int found = static_cast<int>(row.TupleLength());
    for (int i = 0; i < found; ++i) {
        const double x = col[i].D();
        const double y = row[i].D();
        if (x < 0.0 || y < 0.0 || x >= img_w_ || y >= img_h_) {
            continue;
        }
        if (inside.TupleLength() > i && inside[i].I() == 0) {
            continue;
        }
        MatchOne one;
        one.score_ = score[i].D();
        one.pose_.tx = x;
        one.pose_.ty = y;
        one.pose_.setAngRad(ang[i].D());
        matches_.push_back(one);
    }
    if (matches_.empty()) {
        shot_err_ = HS_ERR_TMPL_NO_MATCH;
        shot_msg_ = HsCommData::HsException(HS_ERR_TMPL_NO_MATCH).errorMessage();
        return shot_err_;
    }
    shot_err_ = HS_OK;
    shot_msg_ = "Match ok";
    return HS_OK;
}

int HalTmpl::rebuild_from_org(const AlgToolIn& in, bool shape)
{
    const std::string orgPath = in.toolDataDir + "\\image_org.png";
    if (!pathFileExists(orgPath)) {
        return HS_ERR_TMPL_NO_MODEL;
    }
    // 先保存本拍图像和搜索区，任何出口都要还原
    HalconCpp::HObject shotImage = image_;
    HalconCpp::HObject shotSearch = search_region_;
    const int shotW = img_w_;
    const int shotH = img_h_;
    try {
        initFileNames();
        HalconCpp::HObject org;
        HalconCpp::ReadImage(&org, orgPath.c_str());
        HalconCpp::HTuple channels;
        HalconCpp::CountChannels(org, &channels);
        if (channels.Length() > 0 && channels[0].I() > 1) {
            HalconCpp::HObject gray;
            HalconCpp::Rgb1ToGray(org, &gray);
            org = gray;
        }
        // 原图转灰度后按当前示教区裁模板图；没有示教区时整幅原图作模板
        HalconCpp::HTuple width;
        HalconCpp::HTuple height;
        HalconCpp::GetImageSize(org, &width, &height);
        if (regionHasArea(teach_region_)) {
            HalconCpp::HObject reduced;
            HalconCpp::ReduceDomain(org, teach_region_, &reduced);
            HalconCpp::CropDomain(reduced, &image_teach_);
            HalconCpp::HTuple area, row, col;
            HalconCpp::AreaCenter(teach_region_, &area, &row, &col);
            origin_row_ = row[0].D();
            origin_col_ = col[0].D();
        } else {
            image_teach_ = org;
            HalconCpp::HTuple area, row, col;
            HalconCpp::AreaCenter(org, &area, &row, &col);
            origin_row_ = row[0].D();
            origin_col_ = col[0].D();
        }
        const int built = build_model(in);
        if (built != HS_OK) {
            image_ = shotImage;
            search_region_ = shotSearch;
            img_w_ = shotW;
            img_h_ = shotH;
            return built;
        }
        // 在整幅原图上定位一次，首个结果作为原点位姿
        image_ = org;
        img_w_ = width[0].I();
        img_h_ = height[0].I();
        HalconCpp::GenEmptyObj(&search_region_);
        const HalconCpp::HTuple& model = shape ? tmp_shape_model_ : tmp_ncc_model_;
        locate(in, model, shape);
        if (!matches_.empty()) {
            pose_org_ = matches_[0].pose_;
            pose_org_ok_ = true;
        }
        matches_.clear();
        shot_err_ = HS_OK;
        shot_msg_.clear();
    } catch (const HalconCpp::HException&) {
        image_ = shotImage;
        search_region_ = shotSearch;
        img_w_ = shotW;
        img_h_ = shotH;
        return HS_ERR_TMPL_NO_MODEL;
    }
    image_ = shotImage;
    search_region_ = shotSearch;
    img_w_ = shotW;
    img_h_ = shotH;
    return modelOk(shape ? tmp_shape_model_ : tmp_ncc_model_) ? HS_OK : HS_ERR_TMPL_NO_MODEL;
}

int HalTmpl::ensure_model(const AlgToolIn& in, bool shape, const HalconCpp::HTuple** model)
{
    const bool preferTmp = in.mode == RunMode::Learn || in.mode == RunMode::Test || built_;
    if (shape) {
        if (preferTmp && modelOk(tmp_shape_model_)) {
            *model = &tmp_shape_model_;
            return HS_OK;
        }
        if (!modelOk(shape_model_) && !in.toolDataDir.empty()) {
            const std::string neu = lib_dir(in.toolDataDir) + "\\model.shm";
            const std::string old = in.toolDataDir + "\\model.shm";
            read_model(&shape_model_, pathFileExists(neu) ? neu : old, true);
        }
        if (modelOk(shape_model_)) {
            *model = &shape_model_;
            return HS_OK;
        }
    } else {
        if (preferTmp && modelOk(tmp_ncc_model_)) {
            *model = &tmp_ncc_model_;
            return HS_OK;
        }
        if (!modelOk(ncc_model_) && !in.toolDataDir.empty()) {
            const std::string neu = lib_dir(in.toolDataDir) + "\\model.ncm";
            const std::string old = in.toolDataDir + "\\model.ncm";
            read_model(&ncc_model_, pathFileExists(neu) ? neu : old, false);
        }
        if (modelOk(ncc_model_)) {
            *model = &ncc_model_;
            return HS_OK;
        }
    }
    if (shape && modelOk(tmp_shape_model_)) {
        *model = &tmp_shape_model_;
        return HS_OK;
    }
    if (!shape && modelOk(tmp_ncc_model_)) {
        *model = &tmp_ncc_model_;
        return HS_OK;
    }
    if (in.mode == RunMode::Learn || in.toolDataDir.empty()) {
        return HS_ERR_TMPL_NO_MODEL;
    }
    const int rebuilt = rebuild_from_org(in, shape);
    if (rebuilt != HS_OK) {
        return rebuilt;
    }
    *model = shape ? &tmp_shape_model_ : &tmp_ncc_model_;
    return HS_OK;
}

int HalTmpl::alg_run(const AlgToolIn& in)
{
    const ToolParaTmpl* para = toolParaAs<ToolParaTmpl>(in.param);
    if (para == nullptr) {
        shot_err_ = HS_ERR_TYPE;
        shot_msg_ = HsCommData::HsException(HS_ERR_TYPE).errorMessage();
        return shot_err_;
    }
    const bool shape = para->teach.method == TmplMethod::Shape;
    const HalconCpp::HTuple* model = nullptr;
    const int ready = ensure_model(in, shape, &model);
    if (ready != HS_OK || model == nullptr) {
        shot_err_ = ready != HS_OK ? ready : HS_ERR_TMPL_NO_MODEL;
        shot_msg_ = HsCommData::HsException(shot_err_).errorMessage();
        return shot_err_;
    }
    return locate(in, *model, shape);
}

int HalTmpl::judge_tol(const AlgToolIn& in)
{
    tol_.reset();
    tol_.poseOk.assign(matches_.size(), 1);
    double rx = pose_org_ok_ ? pose_org_.tx : 0;
    double ry = pose_org_ok_ ? pose_org_.ty : 0;
    double ra = pose_org_ok_ ? pose_org_.angle : 0;
    if (in.teach.contains("refX")) {
        rx = in.teach.value("refX").toDouble(rx);
        ry = in.teach.value("refY").toDouble(ry);
        ra = in.teach.value("refA").toDouble(ra);
    }
    for (size_t i = 0; i < matches_.size(); ++i) {
        const MatchOne& one = matches_[i];
        const double dx = one.pose_.tx - rx;
        const double dy = one.pose_.ty - ry;
        const double da = one.pose_.angle - ra;
        const double dist = std::sqrt(dx * dx + dy * dy);
        if (i == 0) {
            tol_.put(TmplKey::offsetX, dx);
            tol_.put(TmplKey::offsetY, dy);
            tol_.put(TmplKey::offsetAngle, da);
            tol_.put(TmplKey::offset, dist);
            tol_.put("x", one.pose_.tx);
            tol_.put("y", one.pose_.ty);
            tol_.put("angle", one.pose_.angle);
            tol_.put(TmplKey::score, one.score_);
        }
        for (size_t j = 0; j < in.judge.size(); ++j) {
            const JudgeParam& item = in.judge[j];
            if (!item.enable) {
                continue;
            }
            double value = 0;
            if (item.name == TmplKey::offsetX) value = dx;
            else if (item.name == TmplKey::offsetY) value = dy;
            else if (item.name == TmplKey::offsetAngle) value = da;
            else if (item.name == TmplKey::offset) value = dist;
            else continue;
            if (value < item.minVal || value > item.maxVal) {
                tol_.poseOk[i] = 0;
                tol_.ok = false;
            }
        }
    }
    tol_.put_int(TmplKey::count, static_cast<int>(matches_.size()));
    tol_.judge(in.judge, nullptr);
    return HS_OK;
}

int HalTmpl::alg_rst_proc(const AlgToolIn&, AlgToolOut* out)
{
    if (out == nullptr) {
        return HS_ERR_TYPE;
    }
    // 判定：跑通且有匹配为 runOk，再叠加公差才算 ok
    AlgToolOut result;
    result.errCode = shot_err_;
    result.ok = shot_err_ == HS_OK && !matches_.empty() && tol_.ok;
    result.runOk = shot_err_ == HS_OK && !matches_.empty();
    result.message = shot_msg_;
    result.poseOrg = pose_org_;
    result.poseOrgOk = pose_org_ok_;
    tol_.apply(&result);
    if (regionHasArea(teach_region_)) {
        NamedObject teachOut;
        teachOut.port = TmplKey::teachRoi;
        teachOut.object = hObjectToRegionObject(teach_region_);
        result.portObjects.push_back(teachOut);
        result.objects.push_back(teachOut.object);
    }
    // 每个匹配输出位姿和十字；示教区、跟随区按「原点到匹配位姿」的刚体变换映射过去
    const bool hasTeach = regionHasArea(teach_region_);
    const bool hasFollow = regionHasArea(follow_region_);
    for (size_t i = 0; i < matches_.size(); ++i) {
        const MatchOne& one = matches_[i];
        result.poses.push_back(one.pose_);
        result.objects.push_back(crossObject(one.pose_.tx, one.pose_.ty));
        if (!hasTeach && !hasFollow) {
            continue;
        }
        HalconCpp::HTuple hom;
        HalconCpp::VectorAngleToRigid(origin_row_, origin_col_, 0.0, one.pose_.ty, one.pose_.tx, one.pose_.angle, &hom);
        if (hasTeach) {
            HalconCpp::HObject aff;
            HalconCpp::AffineTransRegion(teach_region_, &aff, hom, "nearest_neighbor");
            NamedObject named;
            named.port = TmplKey::resultRoi;
            named.index = static_cast<int>(i);
            named.object = hObjectToRegionObject(aff);
            result.portObjects.push_back(named);
            result.objects.push_back(named.object);
        }
        if (hasFollow) {
            HalconCpp::HObject aff;
            HalconCpp::AffineTransRegion(follow_region_, &aff, hom, "nearest_neighbor");
            NamedObject named;
            named.port = TmplKey::followRoi;
            named.index = static_cast<int>(i);
            named.object = hObjectToRegionObject(aff);
            result.portObjects.push_back(named);
            result.objects.push_back(named.object);
        }
    }
    if (!result.values.contains(TmplKey::count)) {
        result.values.insert(TmplKey::count, static_cast<int>(matches_.size()));
    }
    *out = result;
    return shot_err_;
}

int HalTmpl::execute(const AlgToolIn& in, AlgToolOut* out)
{
    if (out == 0) {
        return HS_ERR_TYPE;
    }
    try {
        run_timer_.start();
        tool_reset();
        run_timer_.mark("tool_reset");
        int stepRet = proc_input(in);
        run_timer_.mark("proc_input");
        if (stepRet == HS_OK) {
            stepRet = alg_run(in);
            run_timer_.mark("alg_run");
        }
        if (stepRet == HS_OK) {
            stepRet = judge_tol(in);
            run_timer_.mark("judge_tol");
        }
        alg_rst_proc(in, out);
        run_timer_.stop("alg_rst_proc");
        write_alg_steps(out, run_timer_.sections(), run_timer_.totalMs());
        return stepRet != HS_OK ? stepRet : shot_err_;
    } catch (const HalconCpp::HException& e) {
        if (out) {
            out->ok = false;
            out->errCode = HS_ERR_TMPL_OPERATOR;
            out->message = hExceptionText(e);
        }
        return HS_ERR_TMPL_OPERATOR;
    }
}

int HalTmpl::teach(const AlgToolIn& in, AlgToolOut* out)
{
    if (out == nullptr) {
        return HS_ERR_NULL;
    }
    if (in.toolDataDir.empty()) {
        out->ok = false;
        out->errCode = HS_ERR_TMPL_NO_DIR;
        out->message = HsCommData::HsException(HS_ERR_TMPL_NO_DIR).errorMessage();
        return HS_ERR_TMPL_NO_DIR;
    }
    ensureDirectoryTree(in.toolDataDir);
    try {
        tool_reset();
        int step = proc_input(in);
        if (step == HS_OK && !regionHasArea(teach_region_)) {
            shot_err_ = HS_ERR_TMPL_NO_TEACH;
            shot_msg_ = HsCommData::HsException(HS_ERR_TMPL_NO_TEACH).errorMessage();
            step = shot_err_;
        }
        if (step == HS_OK) {
            step = build_model(in);
        }
        AlgToolIn teachIn = in;
        teachIn.mode = RunMode::Learn;
        if (step == HS_OK) {
            step = alg_run(teachIn);
        }
        if (step == HS_OK && !matches_.empty()) {
            pose_org_ = matches_[0].pose_;
            pose_org_ok_ = true;
        }
        alg_rst_proc(teachIn, out);
        return step != HS_OK ? step : shot_err_;
    } catch (const HalconCpp::HException& e) {
        out->ok = false;
        out->errCode = HS_ERR_TMPL_OPERATOR;
        out->message = hExceptionText(e);
        return HS_ERR_TMPL_OPERATOR;
    }
}

int HalTmpl::action(const std::string& id, const AlgToolIn& in, AlgToolOut* out)
{
    if (id == TmplKey::teachModel) {
        return teach(in, out);
    }
    if (id == TmplKey::inspContour) {
        return inspect_contour(in, out);
    }
    return AlgToolBase::action(id, in, out);
}

int HalTmpl::inspect_contour(const AlgToolIn& in, AlgToolOut* out)
{
    if (out == nullptr) {
        return HS_ERR_NULL;
    }
    try {
        tool_reset();
        const int step = proc_input(in);
        if (step != HS_OK) {
            out->ok = false;
            out->errCode = step;
            out->message = shot_msg_.empty() ? HsCommData::HsException(step).errorMessage() : shot_msg_;
            return step;
        }
        const ToolParaTmpl* para = toolParaAs<ToolParaTmpl>(in.param);
        const NumOrString contrast = para == nullptr ? NumOrString::from_number(30) : para->teach.contrast;
        const HalconCpp::HObject* src = &image_;
        HalconCpp::HObject reduced;
        if (regionHasArea(teach_region_)) {
            HalconCpp::ReduceDomain(image_, teach_region_, &reduced);
            src = &reduced;
        } else if (regionHasArea(search_region_)) {
            HalconCpp::ReduceDomain(image_, search_region_, &reduced);
            src = &reduced;
        }
        HalconCpp::HObject modelImages;
        HalconCpp::HObject modelRegions;
        HalconCpp::InspectShapeModel(*src, &modelImages, &modelRegions, HalconCpp::HTuple(1), contrast_tuple(contrast));
        AlgToolOut result;
        result.errCode = HS_OK;
        result.runOk = true;
        const bool hasContour = regionHasArea(modelRegions);
        result.ok = hasContour;
        result.message = hasContour ? "轮廓预览" : "轮廓为空";
        if (hasContour) {
            NamedObject named;
            named.port = TmplKey::resultRoi;
            named.object = hObjectToRegionObject(modelRegions);
            result.portObjects.push_back(named);
            result.objects.push_back(named.object);
        }
        *out = result;
        return HS_OK;
    } catch (const HalconCpp::HException& e) {
        out->ok = false;
        out->errCode = HS_ERR_TMPL_OPERATOR;
        out->message = hExceptionText(e);
        return HS_ERR_TMPL_OPERATOR;
    }
}

int HalTmpl::read_model(HalconCpp::HTuple* modelId, const std::string& path, bool shape)
{
    if (modelId == nullptr) {
        return HS_ERR_TYPE;
    }
    *modelId = -1;
    if (path.empty() || !pathFileExists(path)) {
        return HS_ERR_FILE_OPEN;
    }
    try {
        initFileNames();
        if (shape) {
            clearShapeModel(*modelId);
            HalconCpp::ReadShapeModel(path.c_str(), modelId);
        } else {
            clearNccModel(*modelId);
            HalconCpp::ReadNccModel(path.c_str(), modelId);
        }
    } catch (const HalconCpp::HException&) {
        *modelId = -1;
        return HS_ERR_FILE_IO;
    }
    if (!modelOk(*modelId)) {
        *modelId = -1;
        return HS_ERR_FILE_FORMAT;
    }
    return HS_OK;
}

int HalTmpl::write_model(const HalconCpp::HTuple* modelId, const std::string& path, bool shape)
{
    if (modelId == nullptr || !modelOk(*modelId)) {
        return HS_ERR_TMPL_NO_MODEL;
    }
    if (path.empty()) {
        return HS_ERR_TMPL_NO_DIR;
    }
    try {
        initFileNames();
        if (shape) {
            HalconCpp::WriteShapeModel(*modelId, path.c_str());
        } else {
            HalconCpp::WriteNccModel(*modelId, path.c_str());
        }
    } catch (const HalconCpp::HException&) {
        return HS_ERR_FILE_IO;
    }
    return HS_OK;
}

void HalTmpl::init(const std::string& toolDataDir)
{
    clearShapeModel(shape_model_);
    clearNccModel(ncc_model_);
    clearShapeModel(tmp_shape_model_);
    clearNccModel(tmp_ncc_model_);
    tool_reset();
    pose_org_ok_ = false;
    pose_org_ = HsCommData::Pose2();
    teach_keep_ = false;
    HalconCpp::GenEmptyObj(&image_tpl_keep_);
    HalconCpp::GenEmptyObj(&region_teach_keep_);
    double tx = 0;
    double ty = 0;
    double angle = 0;
    pose_org_ok_ = read_numbers(lib_dir(toolDataDir) + "\\pose_org.txt", &tx, &ty, &angle)
        || read_numbers(toolDataDir + "\\pose_org.txt", &tx, &ty, &angle);
    if (pose_org_ok_) {
        pose_org_.tx = tx;
        pose_org_.ty = ty;
        pose_org_.setAngRad(angle);
    }
}

void HalTmpl::save(const std::string& toolDataDir)
{
    if (toolDataDir.empty()) {
        return;
    }
    const std::string dir = lib_dir(toolDataDir);
    ensureDirectoryTree(dir);
    if (modelOk(tmp_shape_model_)) {
        clearShapeModel(shape_model_);
        shape_model_ = tmp_shape_model_;
        tmp_shape_model_ = -1;
    }
    if (modelOk(tmp_ncc_model_)) {
        clearNccModel(ncc_model_);
        ncc_model_ = tmp_ncc_model_;
        tmp_ncc_model_ = -1;
    }
    if (modelOk(shape_model_)) {
        write_model(&shape_model_, dir + "\\model.shm", true);
    }
    if (modelOk(ncc_model_)) {
        write_model(&ncc_model_, dir + "\\model.ncm", false);
    }
    if (pose_org_ok_) {
        write_numbers(dir + "\\pose_org.txt", pose_org_.tx, pose_org_.ty, pose_org_.angle);
    }
    if (!teach_keep_) {
        return;
    }
    try {
        initFileNames();
        if (regionHasArea(image_tpl_keep_)) {
            HalconCpp::WriteImage(image_tpl_keep_, "bmp", 0, (dir + "\\image_tpl.bmp").c_str());
        }
        if (regionHasArea(region_teach_keep_)) {
            HalconCpp::WriteRegion(region_teach_keep_, (dir + "\\region_teach.hobj").c_str());
        }
        write_numbers(dir + "\\origin.txt", origin_row_keep_, origin_col_keep_, 0);
    } catch (const HalconCpp::HException&) {
    }
}

bool HalTmpl::read_numbers(const std::string& path, double* a, double* b, double* c)
{
    if (a == nullptr || b == nullptr || c == nullptr || path.empty() || !pathFileExists(path)) {
        return false;
    }
    try {
        initFileNames();
        HalconCpp::HTuple handle;
        HalconCpp::OpenFile(path.c_str(), "input", &handle);
        HalconCpp::HTuple line;
        HalconCpp::HTuple eof;
        HalconCpp::FreadLine(handle, &line, &eof);
        HalconCpp::CloseFile(handle);
        if (line.Length() < 1) {
            return false;
        }
        return sscanf_s(line[0].S().Text(), "%lf %lf %lf", a, b, c) == 3;
    } catch (const HalconCpp::HException&) {
        return false;
    }
}

void HalTmpl::write_numbers(const std::string& path, double a, double b, double c)
{
    if (path.empty()) {
        return;
    }
    initFileNames();
    HalconCpp::HTuple handle;
    HalconCpp::OpenFile(path.c_str(), "output", &handle);
    char line[160];
    sprintf_s(line, "%.17g %.17g %.17g\n", a, b, c);
    HalconCpp::FwriteString(handle, line);
    HalconCpp::CloseFile(handle);
}

}
