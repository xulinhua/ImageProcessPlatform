/**
 * @file HalBlob.cpp
 * @brief 绝对、相对、动态阈值 Blob（Halcon）
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "HalBlob.h"
#include "HalImageConvert.h"
#include "HalObjectConvert.h"
#include "HalOps.h"
#include <HsCommData/HsError.h>
#include <HsCommData/HsException.h>

namespace Vision {

namespace {
const char kVer[] = "1.0.0.1";
const char kName[] = "detect.blob";
const char kSearchPort[] = "search";
const char kResultRoi[] = "resultRoi";
const char kMaxRegion[] = "maxRegion";
const char kRegionArea[] = "regionArea";
const char kPreviewThresh[] = "preview.thresh";
const VerNoteRaw kNotes[] = {
    { "1.0.0.0", "Initial version" },
    { "1.0.0.1", "HalTmpl 单拍流程；步骤管线收为成员方法" },
};
}

std::string HalBlob::tool_name() const { return kName; }
AlgLibKind HalBlob::alg_lib() const { return AlgLibHal; }
AlgToolBase* HalBlob::clone() const { return new HalBlob(); }
std::string HalBlob::version() const { return kVer; }
std::vector<VerNote> HalBlob::version_notes() const
{
    return verNotesFrom(kNotes, static_cast<int>(sizeof(kNotes) / sizeof(kNotes[0])));
}

HalBlob::HalBlob()
    : img_w_(0)
    , img_h_(0)
    , blob_count_(0)
    , area_sum_(0)
    , area_max_(0)
    , shot_err_(0)
{
    HalconCpp::GenEmptyObj(&image_);
    HalconCpp::GenEmptyObj(&detect_region_);
    HalconCpp::GenEmptyObj(&search_region_);
    HalconCpp::GenEmptyObj(&result_region_);
    HalconCpp::GenEmptyObj(&max_region_);
}

HalBlob::~HalBlob()
{
}

void HalBlob::init(const std::string&)
{
}

void HalBlob::save(const std::string&)
{
}

int HalBlob::tool_reset()
{
    blob_count_ = 0;
    area_sum_ = 0;
    area_max_ = 0;
    area_ = HalconCpp::HTuple();
    tol_.reset();
    shot_err_ = HS_OK;
    shot_msg_.clear();
    HalconCpp::GenEmptyObj(&result_region_);
    HalconCpp::GenEmptyObj(&max_region_);
    return HS_OK;
}

void HalBlob::take_region(const AlgToolIn& in, const char* port, HalconCpp::HObject* region)
{
    HalconCpp::GenEmptyObj(region);
    const std::string name = port != nullptr ? port : "";
    const bool search = name == kSearchPort;
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
            || (search && (one.port.empty() || one.port == BlobKey::roi));
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
    for (size_t i = 0; i < in.regions.size(); ++i) {
        const PortRegion& one = in.regions[i];
        if (one.use == RegionUse::Invalid || one.port != name) {
            continue;
        }
        HalconCpp::HObject piece;
        try {
            piece = hsObjectToHObject(one.object, img_w_, img_h_);
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

void HalBlob::collect_step_rois(const AlgToolIn& in)
{
    step_rois_.clear();
    struct IndexedRegion {
        int index;
        HalconCpp::HObject region;
    };
    std::vector<IndexedRegion> rows;
    auto pushOne = [&](int index, const HsCommData::HsObject& shape) {
        if (!shape.isInitialized() || shape.isEmpty()) {
            return;
        }
        try {
            const HalconCpp::HObject piece = hsObjectToHObject(shape, img_w_, img_h_);
            if (hoIsEmpty(piece)) {
                return;
            }
            IndexedRegion row;
            row.index = index;
            row.region = piece;
            rows.push_back(row);
        } catch (const HalconCpp::HException&) {
        }
    };
    for (size_t i = 0; i < in.regions.size(); ++i) {
        const PortRegion& one = in.regions[i];
        if (one.use != RegionUse::Normal || one.port != BlobKey::roi) {
            continue;
        }
        pushOne(one.index, one.object);
    }
    for (size_t i = 0; i < in.rois.size(); ++i) {
        const PortRoi& one = in.rois[i];
        if (one.use != RegionUse::Normal) {
            continue;
        }
        if (!one.port.empty() && one.port != BlobKey::roi) {
            continue;
        }
        pushOne(one.index, one.roi);
    }
    for (size_t i = 1; i < rows.size(); ++i) {
        const IndexedRegion cur = rows[i];
        size_t j = i;
        while (j > 0 && rows[j - 1].index > cur.index) {
            rows[j] = rows[j - 1];
            --j;
        }
        rows[j] = cur;
    }
    for (size_t i = 0; i < rows.size(); ++i) {
        step_rois_.push_back(rows[i].region);
    }
}

int HalBlob::proc_input(const AlgToolIn& in)
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
    if (in.param != nullptr && toolParaAs<ToolParaBlob>(in.param) == nullptr) {
        shot_err_ = HS_ERR_TYPE;
        shot_msg_ = HsCommData::HsException(HS_ERR_TYPE).errorMessage();
        return shot_err_;
    }
    take_region(in, BlobKey::roi, &detect_region_);
    take_region(in, kSearchPort, &search_region_);
    collect_step_rois(in);

    bool detectConfigured = false;
    for (size_t i = 0; i < in.regions.size(); ++i) {
        const PortRegion& one = in.regions[i];
        if (one.use != RegionUse::Invalid && one.port == BlobKey::roi) {
            detectConfigured = true;
            break;
        }
    }
    if (!detectConfigured) {
        for (size_t i = 0; i < in.rois.size(); ++i) {
            const PortRoi& one = in.rois[i];
            if (one.use == RegionUse::Invalid) {
                continue;
            }
            if (!one.port.empty() && one.port != BlobKey::roi) {
                continue;
            }
            detectConfigured = true;
            break;
        }
    }
    if (!detectConfigured) {
        shot_err_ = HS_ERR_BLOB_NO_REGION;
        shot_msg_ = "检测区为空";
        return shot_err_;
    }
    HalconCpp::HObject frame = fullImageRegion(img_w_, img_h_);
    HalconCpp::HObject inside;
    if (regionHasArea(detect_region_)) {
        HalconCpp::Intersection(detect_region_, frame, &inside);
    }
    if (!regionHasArea(inside)) {
        shot_err_ = HS_ERR_BLOB_NO_REGION;
        shot_msg_ = "检测区超出图像";
        return shot_err_;
    }
    detect_region_ = inside;
    return HS_OK;
}

void HalBlob::append_region(HalconCpp::HObject* acc, bool* have, const HalconCpp::HObject& one) const
{
    if (acc == nullptr || have == nullptr) {
        return;
    }
    if (*have) {
        HalconCpp::HObject joined;
        HalconCpp::ConcatObj(*acc, one, &joined);
        *acc = joined;
    } else {
        *acc = one;
        *have = true;
    }
}

void HalBlob::split_region_by_roi(const HalconCpp::HObject& region, const HalconCpp::HObject& roi,
                                  HalconCpp::HObject* inside, bool* hasInside,
                                  HalconCpp::HObject* outside, bool* hasOutside) const
{
    HalconCpp::HTuple count;
    HalconCpp::CountObj(region, &count);
    const int total = count.Length() >= 1 ? static_cast<int>(count[0].L()) : 0;
    for (int i = 1; i <= total; ++i) {
        HalconCpp::HObject one;
        HalconCpp::SelectObj(region, &one, i);
        bool hit = false;
        try {
            HalconCpp::HObject inter;
            HalconCpp::Intersection(one, roi, &inter);
            hit = regionHasArea(inter);
        } catch (const HalconCpp::HException&) {
            hit = false;
        }
        if (hit) {
            append_region(inside, hasInside, one);
        } else {
            append_region(outside, hasOutside, one);
        }
    }
}

bool HalBlob::apply_morph_op(const ToolParaBlob::Step& st, const HalconCpp::HObject& src,
                             HalconCpp::HObject* dst) const
{
    if (dst == nullptr) {
        return false;
    }
    if (st.morphOp == BlobKey::morphOpen) {
        HalconCpp::OpeningCircle(src, dst, st.radius);
    } else if (st.morphOp == BlobKey::morphClose) {
        HalconCpp::ClosingCircle(src, dst, st.radius);
    } else if (st.morphOp == BlobKey::morphErode) {
        HalconCpp::ErosionCircle(src, dst, st.radius);
    } else if (st.morphOp == BlobKey::morphDilate) {
        HalconCpp::DilationCircle(src, dst, st.radius);
    } else {
        return false;
    }
    return true;
}

int HalBlob::segment_domain(const ToolParaBlob::Step& st, const HalconCpp::HObject& image,
                            const HalconCpp::HObject& domain, HalconCpp::HObject* binary)
{
    if (binary == nullptr) {
        return HS_ERR_TYPE;
    }
    if (!regionHasArea(domain)) {
        return HS_ERR_BLOB_NO_REGION;
    }
    HalconCpp::HObject reduced;
    HalconCpp::ReduceDomain(image, domain, &reduced);
    const HalconCpp::HObject* work = &reduced;
    if (st.threshMode == BlobKey::relative) {
        HalconCpp::HTuple mean;
        HalconCpp::HTuple dev;
        HalconCpp::Intensity(*work, domain, &mean, &dev);
        const double meanGray = mean.Length() > 0 ? mean[0].D() : 0.0;
        if (st.dark) {
            HalconCpp::Threshold(*work, binary, 0.0, meanGray - st.relOffset);
        } else {
            HalconCpp::Threshold(*work, binary, meanGray + st.relOffset, 255.0);
        }
    } else if (st.threshMode == BlobKey::dynamic) {
        int kernel = st.dynKernel;
        if (kernel < 3) {
            kernel = 3;
        }
        if ((kernel % 2) == 0) {
            ++kernel;
        }
        HalconCpp::HObject background;
        HalconCpp::MeanImage(*work, &background, kernel, kernel);
        HalconCpp::DynThreshold(*work, background, binary, st.dynOffset, st.dark ? "dark" : "light");
    } else if (st.dark) {
        HalconCpp::Threshold(*work, binary, 0.0, st.absMin);
    } else {
        HalconCpp::Threshold(*work, binary, st.absMin, st.absMax);
    }
    return HS_OK;
}

bool HalBlob::step_domain(int roiIndex, HalconCpp::HObject* roi) const
{
    if (roi == nullptr || roiIndex < 0 || roiIndex >= static_cast<int>(step_rois_.size())) {
        return false;
    }
    try {
        HalconCpp::HObject frame = fullImageRegion(img_w_, img_h_);
        HalconCpp::HObject inside;
        HalconCpp::Intersection(step_rois_[static_cast<size_t>(roiIndex)], frame, &inside);
        if (!regionHasArea(inside)) {
            return false;
        }
        *roi = inside;
        return true;
    } catch (const HalconCpp::HException&) {
        return false;
    }
}

int HalBlob::step_morph(const ToolParaBlob::Step& st, HalconCpp::HObject* region)
{
    if (region == nullptr || st.radius <= 0.4) {
        return HS_OK;
    }
    HalconCpp::HObject roi;
    if (step_domain(st.roiIndex, &roi)) {
        HalconCpp::HObject inGroup;
        HalconCpp::HObject outGroup;
        bool hasIn = false;
        bool hasOut = false;
        split_region_by_roi(*region, roi, &inGroup, &hasIn, &outGroup, &hasOut);
        HalconCpp::HObject result;
        HalconCpp::GenEmptyObj(&result);
        bool haveResult = false;
        if (hasIn) {
            HalconCpp::HObject morphed;
            if (apply_morph_op(st, inGroup, &morphed)) {
                result = morphed;
                haveResult = true;
            }
        }
        if (hasOut) {
            if (haveResult) {
                HalconCpp::HObject joined;
                HalconCpp::ConcatObj(result, outGroup, &joined);
                result = joined;
            } else {
                result = outGroup;
                haveResult = true;
            }
        }
        if (haveResult) {
            *region = result;
        }
        return HS_OK;
    }
    HalconCpp::HObject out;
    if (!apply_morph_op(st, *region, &out)) {
        return HS_OK;
    }
    *region = out;
    return HS_OK;
}

int HalBlob::step_select(const ToolParaBlob::Step& st, HalconCpp::HObject* region)
{
    if (region == nullptr) {
        return HS_ERR_TYPE;
    }
    const double hi = st.maxVal > 0.0 ? st.maxVal : 1.0e12;
    const char* feat = st.feature == BlobKey::featCirc ? "circularity" : "area";
    HalconCpp::HObject roi;
    if (step_domain(st.roiIndex, &roi)) {
        HalconCpp::HObject inGroup;
        HalconCpp::HObject outGroup;
        bool hasIn = false;
        bool hasOut = false;
        split_region_by_roi(*region, roi, &inGroup, &hasIn, &outGroup, &hasOut);
        HalconCpp::HObject result;
        HalconCpp::GenEmptyObj(&result);
        bool haveResult = false;
        if (hasIn) {
            HalconCpp::HObject kept;
            HalconCpp::SelectShape(inGroup, &kept, feat, "and", st.minVal, hi);
            HalconCpp::HTuple n;
            HalconCpp::CountObj(kept, &n);
            if (n.Length() >= 1 && n[0].L() >= 1) {
                result = kept;
                haveResult = true;
            }
        }
        if (hasOut) {
            if (haveResult) {
                HalconCpp::HObject joined;
                HalconCpp::ConcatObj(result, outGroup, &joined);
                result = joined;
            } else {
                result = outGroup;
                haveResult = true;
            }
        }
        *region = result;
        return HS_OK;
    }
    HalconCpp::HObject out;
    HalconCpp::SelectShape(*region, &out, feat, "and", st.minVal, hi);
    *region = out;
    return HS_OK;
}

int HalBlob::run_steps(const ToolParaBlob::Run& rp, HalconCpp::HObject* region)
{
    if (region == nullptr) {
        return HS_ERR_TYPE;
    }
    bool haveBinary = false;
    HalconCpp::HObject unionBinary;
    for (size_t i = 0; i < rp.steps.size(); ++i) {
        const ToolParaBlob::Step& st = rp.steps[i];
        if (st.type != BlobKey::stepThresh) {
            continue;
        }
        HalconCpp::HObject stepDomainObj = detect_region_;
        HalconCpp::HObject bound;
        if (step_domain(st.roiIndex, &bound)) {
            stepDomainObj = bound;
        }
        HalconCpp::HObject binary;
        const int seg = segment_domain(st, image_, stepDomainObj, &binary);
        if (seg != HS_OK) {
            return seg;
        }
        if (!haveBinary) {
            unionBinary = binary;
            haveBinary = true;
        } else {
            HalconCpp::HObject joined;
            HalconCpp::Union2(unionBinary, binary, &joined);
            unionBinary = joined;
        }
    }
    if (!haveBinary) {
        return HS_ERR_BLOB_NO_REGION;
    }
    HalconCpp::HObject conn;
    HalconCpp::Connection(unionBinary, &conn);
    *region = conn;
    for (size_t i = 0; i < rp.steps.size(); ++i) {
        const ToolParaBlob::Step& st = rp.steps[i];
        if (st.type == BlobKey::stepMorph) {
            const int ret = step_morph(st, region);
            if (ret != HS_OK) {
                return ret;
            }
        } else if (st.type == BlobKey::stepSelect) {
            const int ret = step_select(st, region);
            if (ret != HS_OK) {
                return ret;
            }
        }
    }
    HalconCpp::HTuple n;
    HalconCpp::CountObj(*region, &n);
    if (n.Length() < 1 || n[0].L() < 1) {
        return HS_ERR_BLOB_NO_REGION;
    }
    return HS_OK;
}

int HalBlob::alg_run(const AlgToolIn& in)
{
    ToolParaBlob local;
    const ToolParaBlob* para = paraOf(in, &local);
    if (para == nullptr) {
        shot_err_ = HS_ERR_TYPE;
        shot_msg_ = HsCommData::HsException(HS_ERR_TYPE).errorMessage();
        return shot_err_;
    }
    HalconCpp::HObject region;
    const int runRet = run_steps(para->run, &region);
    if (runRet != HS_OK) {
        shot_err_ = runRet;
        shot_msg_ = HsCommData::HsException(runRet).errorMessage();
        return runRet;
    }
    result_region_ = region;
    if (regionHasArea(search_region_)) {
        HalconCpp::HObject clipped;
        HalconCpp::Intersection(result_region_, search_region_, &clipped);
        HalconCpp::HObject kept;
        HalconCpp::SelectShape(clipped, &kept, "area", "and", 0.5, 1.0e12);
        result_region_ = kept;
    }
    HalconCpp::HTuple row;
    HalconCpp::HTuple col;
    HalconCpp::AreaCenter(result_region_, &area_, &row, &col);
    blob_count_ = static_cast<int>(area_.Length());
    if (blob_count_ < 1) {
        shot_err_ = HS_ERR_BLOB_NO_REGION;
        shot_msg_ = HsCommData::HsException(HS_ERR_BLOB_NO_REGION).errorMessage();
        return shot_err_;
    }
    area_sum_ = area_.TupleSum().D();
    const HalconCpp::HTuple maxVal = area_.TupleMax();
    area_max_ = maxVal.D();
    const HalconCpp::HTuple maxValIdx = area_.TupleFind(maxVal);
    HalconCpp::SelectObj(result_region_, &max_region_, maxValIdx.TupleAdd(HalconCpp::HTuple(1)));
    shot_err_ = HS_OK;
    shot_msg_ = "Blob done";
    return HS_OK;
}

int HalBlob::judge_tol(const AlgToolIn& in)
{
    tol_.reset();
    tol_.put_int("count", blob_count_);
    tol_.put("area", area_sum_);
    tol_.put("areaSum", area_sum_);
    bool on = false;
    double lo = 0.0;
    double hi = 0.0;
    for (size_t i = 0; i < in.judge.size(); ++i) {
        const JudgeParam& item = in.judge[i];
        if (!item.enable || item.name != kRegionArea) {
            continue;
        }
        on = true;
        lo = item.minVal;
        hi = item.maxVal;
    }
    if (on && area_.Length() >= 1) {
        const HalconCpp::HTuple inside = area_.TupleGreaterEqualElem(lo).TupleAnd(area_.TupleLessEqualElem(hi));
        const HalconCpp::HTuple bad = inside.TupleFind(0);
        if (bad.Length() >= 1 && bad[0].I() >= 0) {
            const int index = bad[0].I();
            tol_.add_miss(kRegionArea, area_[index].D(), lo, hi);
        }
    } else if (on) {
        tol_.add_missing(kRegionArea);
    }
    tol_.judge(in.judge, kRegionArea);
    return HS_OK;
}

int HalBlob::alg_rst_proc(const AlgToolIn&, AlgToolOut* out)
{
    if (out == nullptr) {
        return HS_ERR_TYPE;
    }
    AlgToolOut result;
    result.errCode = shot_err_;
    result.ok = shot_err_ == HS_OK && tol_.ok;
    result.runOk = shot_err_ == HS_OK;
    result.message = shot_msg_;
    if (shot_err_ == HS_OK) {
        tol_.apply(&result);
        NamedObject all;
        all.port = kResultRoi;
        all.object = hObjectToRegionObject(result_region_);
        result.portObjects.push_back(all);
        result.objects.push_back(all.object);
        if (regionHasArea(max_region_)) {
            NamedObject largest;
            largest.port = kMaxRegion;
            largest.object = hObjectToRegionObject(max_region_);
            result.portObjects.push_back(largest);
            result.objects.push_back(largest.object);
        }
    }
    *out = result;
    return shot_err_;
}

int HalBlob::action(const std::string& id, const AlgToolIn& in, AlgToolOut* out)
{
    if (id != kPreviewThresh) {
        return AlgToolBase::action(id, in, out);
    }
    if (out == nullptr) {
        return HS_ERR_NULL;
    }
    try {
        tool_reset();
        int step = proc_input(in);
        if (step == HS_OK) {
            ToolParaBlob local;
            const ToolParaBlob* para = paraOf(in, &local);
            if (para == nullptr) {
                step = HS_ERR_TYPE;
                shot_err_ = step;
                shot_msg_ = HsCommData::HsException(step).errorMessage();
            } else {
                HalconCpp::HObject region;
                step = run_steps(para->run, &region);
                if (step == HS_OK) {
                    result_region_ = region;
                    shot_err_ = HS_OK;
                    shot_msg_ = "Preview";
                } else {
                    shot_err_ = step;
                    shot_msg_ = HsCommData::HsException(step).errorMessage();
                }
            }
        }
        AlgToolOut result;
        result.errCode = step;
        result.ok = step == HS_OK;
        result.message = shot_msg_;
        if (step == HS_OK && regionHasArea(result_region_)) {
            NamedObject all;
            all.port = kResultRoi;
            all.object = hObjectToRegionObject(result_region_);
            result.portObjects.push_back(all);
            result.objects.push_back(all.object);
        }
        *out = result;
        return step;
    } catch (const HalconCpp::HException& e) {
        out->ok = false;
        out->errCode = HS_ERR_BLOB_OPERATOR;
        out->message = hExceptionText(e);
        return HS_ERR_BLOB_OPERATOR;
    }
}

int HalBlob::execute(const AlgToolIn& in, AlgToolOut* out)
{
    if (out == nullptr) {
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
            out->errCode = HS_ERR_BLOB_OPERATOR;
            out->message = hExceptionText(e);
        }
        return HS_ERR_BLOB_OPERATOR;
    }
}

}
