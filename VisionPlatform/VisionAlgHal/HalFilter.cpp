/**
 * @file HalFilter.cpp
 * @brief 图像滤波流水。增强、平滑和灰度形态学按步骤做（Halcon）
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "HalFilter.h"
#include "ToolParaFilter.hpp"
#include "TextEncoding.h"
#include "HalImageConvert.h"
#include "HalObjectConvert.h"
#include "HalOps.h"

namespace Vision {

namespace {
const char kVer_HalFilter[] = "1.0.0.0";
const Vision::VerNoteRaw kNotes_HalFilter[] = {
    { "1.0.0.0", "首版：多步骤滤波，支持形态学和步骤绑定区域" },
};

enum class FilterOp {
    EquHisto = 0,
    Emphasize,
    Illuminate,
    Scale,
    Invert,
    Mean,
    Median,
    Gauss,
    Bilateral,
    GrayOpen,
    GrayClose,
    GrayErode,
    GrayDilate,
    TopHat,
    BottomHat
};

struct FilterStep {
    FilterOp op;
    int maskW;
    int maskH;
    double coeff;
    double coeff2;
    bool scaleMax;
    std::string shape;
    int roiIndex;
    FilterStep()
        : op(FilterOp::EquHisto)
        , maskW(7)
        , maskH(7)
        , coeff(1.0)
        , coeff2(3.0)
        , scaleMax(false)
        , shape(FilterKey::shapeRect)
        , roiIndex(-1)
    {}
};

bool filterOpFromString(const std::string& v, FilterOp* out)
{
    if (!out) {
        return false;
    }
    if (v == FilterKey::opEquHisto)   { *out = FilterOp::EquHisto;   return true; }
    if (v == FilterKey::opEmphasize)  { *out = FilterOp::Emphasize;  return true; }
    if (v == FilterKey::opIlluminate) { *out = FilterOp::Illuminate; return true; }
    if (v == FilterKey::opScale)      { *out = FilterOp::Scale;      return true; }
    if (v == FilterKey::opInvert)     { *out = FilterOp::Invert;     return true; }
    if (v == FilterKey::opMean)       { *out = FilterOp::Mean;       return true; }
    if (v == FilterKey::opMedian)     { *out = FilterOp::Median;     return true; }
    if (v == FilterKey::opGauss)      { *out = FilterOp::Gauss;      return true; }
    if (v == FilterKey::opBilateral)  { *out = FilterOp::Bilateral;  return true; }
    if (v == FilterKey::opGrayOpen)   { *out = FilterOp::GrayOpen;   return true; }
    if (v == FilterKey::opGrayClose)  { *out = FilterOp::GrayClose;  return true; }
    if (v == FilterKey::opGrayErode)  { *out = FilterOp::GrayErode;  return true; }
    if (v == FilterKey::opGrayDilate) { *out = FilterOp::GrayDilate; return true; }
    if (v == FilterKey::opTopHat)     { *out = FilterOp::TopHat;     return true; }
    if (v == FilterKey::opBottomHat)  { *out = FilterOp::BottomHat;  return true; }
    return false;
}

void morphMask(const FilterStep& st, int* maskH, int* maskW, const char** shape)
{
    int side = st.maskW > st.maskH ? st.maskW : st.maskH;
    if (side < 1) {
        side = 1;
    }
    if (side % 2 == 0) {
        ++side;
    }
    *maskH = side;
    *maskW = side;
    if (st.shape == FilterKey::shapeOctagon) {
        *shape = "octagon";
    } else if (st.shape == FilterKey::shapeRhombus) {
        *shape = "rhombus";
    } else {
        *shape = "rectangle";
    }
}

bool applyFilterOp(HalconCpp::HObject* img, const FilterStep& st, std::string* err)
{
    switch (st.op) {
    case FilterOp::EquHisto: {
        HalconCpp::HObject out;
        HalconCpp::EquHistoImage(*img, &out);
        *img = out;
        return true;
    }
    case FilterOp::Emphasize: {
        const int mw = st.maskW < 1 ? 1 : st.maskW;
        const int mh = st.maskH < 1 ? 1 : st.maskH;
        HalconCpp::HObject out;
        HalconCpp::Emphasize(*img, &out, mw, mh, st.coeff);
        *img = out;
        return true;
    }
    case FilterOp::Illuminate: {
        const int mw = st.maskW < 1 ? 1 : st.maskW;
        const int mh = st.maskH < 1 ? 1 : st.maskH;
        HalconCpp::HObject out;
        HalconCpp::Illuminate(*img, &out, mw, mh, st.coeff);
        *img = out;
        return true;
    }
    case FilterOp::Scale:
        if (st.scaleMax) {
            HalconCpp::HObject out;
            HalconCpp::ScaleImageMax(*img, &out);
            *img = out;
        } else {
            HalconCpp::HObject out;
            HalconCpp::ScaleImage(*img, &out, st.coeff, st.coeff2);
            *img = out;
        }
        return true;
    case FilterOp::Invert: {
        HalconCpp::HObject out;
        HalconCpp::InvertImage(*img, &out);
        *img = out;
        return true;
    }
    case FilterOp::Mean: {
        const int w = st.maskW < 1 ? 1 : st.maskW;
        const int h = st.maskH < 1 ? 1 : st.maskH;
        HalconCpp::HObject out;
        HalconCpp::MeanImage(*img, &out, w, h);
        *img = out;
        return true;
    }
    case FilterOp::Median: {
        const int radius = st.maskW < 1 ? 1 : st.maskW;
        HalconCpp::HObject out;
        HalconCpp::MedianImage(*img, &out, "circle", radius, "mirrored");
        *img = out;
        return true;
    }
    case FilterOp::Gauss: {
        static const int kGaussSizes[] = { 3, 5, 7, 9, 11 };
        int best = st.maskW < 3 ? 3 : st.maskW;
        if (best > kGaussSizes[4]) {
            best = kGaussSizes[4];
        }
        int chosen = kGaussSizes[0];
        for (int i = 0; i < 5; ++i) {
            if (kGaussSizes[i] <= best) {
                chosen = kGaussSizes[i];
            }
        }
        HalconCpp::HObject out;
        HalconCpp::GaussFilter(*img, &out, chosen);
        *img = out;
        return true;
    }
    case FilterOp::Bilateral: {
        HalconCpp::HTuple empty;
        HalconCpp::HObject out;
        HalconCpp::BilateralFilter(*img, *img, &out, st.coeff2, st.coeff, empty, empty);
        *img = out;
        return true;
    }
    case FilterOp::GrayOpen: {
        int mh = 0;
        int mw = 0;
        const char* shape = 0;
        morphMask(st, &mh, &mw, &shape);
        HalconCpp::HObject out;
        HalconCpp::GrayOpeningShape(*img, &out, mh, mw, shape);
        *img = out;
        return true;
    }
    case FilterOp::GrayClose: {
        int mh = 0;
        int mw = 0;
        const char* shape = 0;
        morphMask(st, &mh, &mw, &shape);
        HalconCpp::HObject out;
        HalconCpp::GrayClosingShape(*img, &out, mh, mw, shape);
        *img = out;
        return true;
    }
    case FilterOp::GrayErode: {
        int mh = 0;
        int mw = 0;
        const char* shape = 0;
        morphMask(st, &mh, &mw, &shape);
        HalconCpp::HObject out;
        HalconCpp::GrayErosionShape(*img, &out, mh, mw, shape);
        *img = out;
        return true;
    }
    case FilterOp::GrayDilate: {
        int mh = 0;
        int mw = 0;
        const char* shape = 0;
        morphMask(st, &mh, &mw, &shape);
        HalconCpp::HObject out;
        HalconCpp::GrayDilationShape(*img, &out, mh, mw, shape);
        *img = out;
        return true;
    }
    case FilterOp::TopHat: {
        int mh = 0;
        int mw = 0;
        const char* shape = 0;
        morphMask(st, &mh, &mw, &shape);
        HalconCpp::HObject opened;
        HalconCpp::GrayOpeningShape(*img, &opened, mh, mw, shape);
        HalconCpp::HObject out;
        HalconCpp::SubImage(*img, opened, &out, 1.0, 0.0);
        *img = out;
        return true;
    }
    case FilterOp::BottomHat: {
        int mh = 0;
        int mw = 0;
        const char* shape = 0;
        morphMask(st, &mh, &mw, &shape);
        HalconCpp::HObject closed;
        HalconCpp::GrayClosingShape(*img, &closed, mh, mw, shape);
        HalconCpp::HObject out;
        HalconCpp::SubImage(closed, *img, &out, 1.0, 0.0);
        *img = out;
        return true;
    }
    default:
        if (err) {
            *err = "未知滤波算子";
        }
        return false;
    }
}

struct IndexedRegion {
    int index;
    HalconCpp::HObject region;
};

void pushRegion(std::vector<IndexedRegion>* out, int index, const HsCommData::HsObject& shape, int imgW, int imgH)
{
    if (!shape.isInitialized() || shape.isEmpty()) {
        return;
    }
    try {
        const HalconCpp::HObject piece = hsObjectToHObject(shape, imgW, imgH);
        if (hoIsEmpty(piece)) {
            return;
        }
        IndexedRegion row;
        row.index = index;
        row.region = piece;
        out->push_back(row);
    } catch (const HalconCpp::HException&) {
    }
}

std::vector<HalconCpp::HObject> collectStepRois(const AlgToolIn& in, int imgW, int imgH)
{
    std::vector<IndexedRegion> rows;
    for (size_t i = 0; i < in.regions.size(); ++i) {
        const PortRegion& one = in.regions[i];
        if (one.use != RegionUse::Normal || one.port != FilterKey::roi) {
            continue;
        }
        pushRegion(&rows, one.index, one.object, imgW, imgH);
    }
    for (size_t i = 0; i < in.rois.size(); ++i) {
        const PortRoi& one = in.rois[i];
        if (one.use != RegionUse::Normal) {
            continue;
        }
        if (!one.port.empty() && one.port != FilterKey::roi) {
            continue;
        }
        pushRegion(&rows, one.index, one.roi, imgW, imgH);
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
    std::vector<HalconCpp::HObject> ordered;
    ordered.reserve(rows.size());
    for (size_t i = 0; i < rows.size(); ++i) {
        ordered.push_back(rows[i].region);
    }
    return ordered;
}
}

std::string HalFilter::version() const
{
    return kVer_HalFilter;
}

std::vector<VerNote> HalFilter::versionNotes() const
{
    return verNotesFrom(kNotes_HalFilter, static_cast<int>(sizeof(kNotes_HalFilter) / sizeof(kNotes_HalFilter[0])));
}

AlgToolOut HalFilter::writeFullImage(const AlgToolIn& in, const HalconCpp::HImage& work, const std::string& okMsg) const
{
    AlgToolOut out;
    const HalconCpp::HImage full = work.FullDomain();
    std::string err;
    HsCommData::HsImage* pImage = &out.image.image;
    if (!hObjectToHsImage(full, inputImage(in).frameId, pImage, &err)) {
        return AlgToolOut::fail(err);
    }
    out.ok = true;
    out.message = okMsg;
    return out;
}

AlgToolOut HalFilter::run(const AlgToolIn& in)
{
    ToolParam bag;
    bag.run = in.run;
    bag.judge = in.judge;
    ToolParaFilter para;
    para.load(bag);

    std::vector<FilterStep> steps;
    for (size_t i = 0; i < para.run.steps.size(); ++i) {
        const ToolParaFilter::Step& src = para.run.steps[i];
        FilterStep st;
        if (!filterOpFromString(src.op, &st.op)) {
            return AlgToolOut::fail("未知滤波算子");
        }
        if (st.op == FilterOp::Bilateral && (src.coeff <= 0.0 || src.coeff2 <= 0.0)) {
            return AlgToolOut::fail("双边滤波的标准差必须大于 0");
        }
        st.maskW = src.maskW;
        st.maskH = src.maskH;
        st.coeff = src.coeff;
        st.coeff2 = src.coeff2;
        st.scaleMax = src.scaleMax;
        st.shape = src.shape;
        st.roiIndex = src.roiIndex;
        steps.push_back(st);
    }

    try {
        HalconCpp::HImage gray;
        int imgW = 0;
        int imgH = 0;
        std::string err;
        HalconCpp::HImage* pGray = &gray;
        if (!hsImageToGray(inputImage(in), pGray, &imgW, &imgH, &err)) {
            return AlgToolOut::fail(err);
        }
        if (imgW < 1 || imgH < 1) {
            return AlgToolOut::fail("输入图像为空");
        }
        const std::vector<HalconCpp::HObject> stepRois = collectStepRois(in, imgW, imgH);
        HalconCpp::HObject work = gray;
        for (size_t i = 0; i < steps.size(); ++i) {
            const int ri = steps[i].roiIndex;
            if (ri >= 0) {
                if (ri >= static_cast<int>(stepRois.size())) {
                    continue;
                }
                HalconCpp::HObject frame = fullImageRegion(imgW, imgH);
                HalconCpp::HObject inside;
                HalconCpp::Intersection(stepRois[static_cast<size_t>(ri)], frame, &inside);
                if (!regionHasArea(inside)) {
                    continue;
                }
                HalconCpp::HObject reduced;
                HalconCpp::ReduceDomain(work, inside, &reduced);
                if (!applyFilterOp(&reduced, steps[i], &err)) {
                    return AlgToolOut::fail(err);
                }
                HalconCpp::OverpaintGray(work, reduced);
                continue;
            }
            if (!applyFilterOp(&work, steps[i], &err)) {
                return AlgToolOut::fail(err);
            }
        }
        AlgToolOut out = writeFullImage(in, HalconCpp::HImage(work), "图像滤波完成");
        if (out.ok) {
            out.values.insert(FilterKey::outWidth, out.image.image.width);
            out.values.insert(FilterKey::outHeight, out.image.image.height);
            for (size_t i = 0; i < steps.size(); ++i) {
                const int ri = steps[i].roiIndex;
                if (ri < 0 || ri >= static_cast<int>(stepRois.size())) {
                    continue;
                }
                out.objects.push_back(hObjectToRegionObject(stepRois[static_cast<size_t>(ri)]));
            }
        }
        return out;
    } catch (const HalconCpp::HException& e) {
        return AlgToolOut::fail(hExceptionText(e));
    }
}

}
