/**
 * @file HalBlob.cpp
 * @brief ???????????????????Halcon??
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "HalBlob.h"
#include "ToolParaBlob.hpp"
#include "TextEncoding.h"
#include "HalAlgoHelpers.h"
#include "HalImageConvert.h"
#include "HalObjectConvert.h"
#include "HalOps.h"

using HalconCpp::HObject;
using HalconCpp::HImage;
using HalconCpp::HTuple;
using HalconCpp::HException;

namespace Vision {

namespace {
const char kVer_HalBlob[] = "1.0.0.0";
const Vision::VerNoteRaw kNotes_HalBlob[] = {
    { "1.0.0.0", "???" },
    { "1.0.0.1", "?????????????????????????" },
};

bool regionHasArea(const HObject& region)
{
    if (hoIsEmpty(region)) {
        return false;
    }
    try {
        HTuple area;
        HTuple row;
        HTuple col;
        HalconCpp::AreaCenter(region, &area, &row, &col);
        for (int i = 0; i < area.Length(); ++i) {
            if (area[i].D() > 0.0) {
                return true;
            }
        }
    } catch (const HException&) {
        return false;
    }
    return false;
}

void appendRegion(HObject* acc, bool* have, const HObject& one)
{
    if (!acc || !have) {
        return;
    }
    if (*have) {
        HObject joined;
        HalconCpp::ConcatObj(*acc, one, &joined);
        *acc = joined;
    } else {
        *acc = one;
        *have = true;
    }
}

void splitRegionByRoi(const HObject& region, const HObject& roi,
                      HObject* inside, bool* hasInside,
                      HObject* outside, bool* hasOutside)
{
    HTuple count;
    HalconCpp::CountObj(region, &count);
    const int total = count.Length() >= 1 ? static_cast<int>(count[0].L()) : 0;
    for (int i = 1; i <= total; ++i) {
        HObject one;
        HalconCpp::SelectObj(region, &one, i);
        bool hit = false;
        try {
            HObject inter;
            HalconCpp::Intersection(one, roi, &inter);
            hit = regionHasArea(inter);
        } catch (const HException&) {
            hit = false;
        }
        if (hit) {
            appendRegion(inside, hasInside, one);
        } else {
            appendRegion(outside, hasOutside, one);
        }
    }
}

bool applyMorphOp(const ToolParaBlob::Step& st, const HObject& src, HObject* dst)
{
    if (!dst) {
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

struct IndexedRegion {
    int index;
    HObject region;
};

void pushRegion(std::vector<IndexedRegion>* out, int index, const HsCommData::HsObject& shape, int imgW, int imgH)
{
    if (!shape.isInitialized() || shape.isEmpty()) {
        return;
    }
    try {
        const HObject piece = hsObjectToHObject(shape, imgW, imgH);
        if (hoIsEmpty(piece)) {
            return;
        }
        IndexedRegion row;
        row.index = index;
        row.region = piece;
        out->push_back(row);
    } catch (const HException&) {
    }
}

std::vector<HObject> collectStepRois(const AlgToolIn& in, int imgW, int imgH)
{
    std::vector<IndexedRegion> rows;
    for (size_t i = 0; i < in.regions.size(); ++i) {
        const PortRegion& one = in.regions[i];
        if (one.use != RegionUse::Normal || one.port != BlobKey::roi) {
            continue;
        }
        pushRegion(&rows, one.index, one.object, imgW, imgH);
    }
    for (size_t i = 0; i < in.rois.size(); ++i) {
        const PortRoi& one = in.rois[i];
        if (one.use != RegionUse::Normal) {
            continue;
        }
        if (!one.port.empty() && one.port != BlobKey::roi) {
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
    std::vector<HObject> ordered;
    for (size_t i = 0; i < rows.size(); ++i) {
        ordered.push_back(rows[i].region);
    }
    return ordered;
}

bool stepDomain(const std::vector<HObject>& rois, int roiIndex, int imgW, int imgH, HObject* roi)
{
    if (!roi || roiIndex < 0 || roiIndex >= static_cast<int>(rois.size())) {
        return false;
    }
    try {
        HObject frame = fullImageRegion(imgW, imgH);
        HObject inside;
        HalconCpp::Intersection(rois[static_cast<size_t>(roiIndex)], frame, &inside);
        if (!regionHasArea(inside)) {
            return false;
        }
        *roi = inside;
        return true;
    } catch (const HException&) {
        return false;
    }
}

bool segmentDomain(const ToolParaBlob::Step& st, const HObject& image, const HObject& domain, HObject* binary)
{
    if (!binary || !regionHasArea(domain)) {
        return false;
    }
    HObject reduced;
    HalconCpp::ReduceDomain(image, domain, &reduced);
    if (st.threshMode == BlobKey::relative) {
        HTuple mean;
        HTuple dev;
        HalconCpp::Intensity(reduced, domain, &mean, &dev);
        const double meanGray = mean.Length() > 0 ? mean[0].D() : 0.0;
        if (st.dark) {
            HalconCpp::Threshold(reduced, binary, 0.0, meanGray - st.relOffset);
        } else {
            HalconCpp::Threshold(reduced, binary, meanGray + st.relOffset, 255.0);
        }
    } else if (st.threshMode == BlobKey::dynamic) {
        int kernel = st.dynKernel;
        if (kernel < 3) {
            kernel = 3;
        }
        if ((kernel % 2) == 0) {
            ++kernel;
        }
        HObject background;
        HalconCpp::MeanImage(reduced, &background, kernel, kernel);
        HalconCpp::DynThreshold(reduced, background, binary, st.dynOffset, st.dark ? "dark" : "light");
    } else if (st.dark) {
        HalconCpp::Threshold(reduced, binary, 0.0, st.absMin);
    } else {
        HalconCpp::Threshold(reduced, binary, st.absMin, st.absMax);
    }
    return true;
}

void stepMorph(const ToolParaBlob::Step& st, const std::vector<HObject>& rois, int imgW, int imgH, HObject* region)
{
    if (!region || st.radius <= 0.4) {
        return;
    }
    HObject roi;
    if (stepDomain(rois, st.roiIndex, imgW, imgH, &roi)) {
        HObject inGroup;
        HObject outGroup;
        bool hasIn = false;
        bool hasOut = false;
        splitRegionByRoi(*region, roi, &inGroup, &hasIn, &outGroup, &hasOut);
        HObject result;
        HalconCpp::GenEmptyObj(&result);
        bool haveResult = false;
        if (hasIn) {
            HObject morphed;
            if (applyMorphOp(st, inGroup, &morphed)) {
                result = morphed;
                haveResult = true;
            }
        }
        if (hasOut) {
            if (haveResult) {
                HObject joined;
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
        return;
    }
    HObject out;
    if (applyMorphOp(st, *region, &out)) {
        *region = out;
    }
}

void stepSelect(const ToolParaBlob::Step& st, const std::vector<HObject>& rois, int imgW, int imgH, HObject* region)
{
    if (!region) {
        return;
    }
    const double hi = st.maxVal > 0.0 ? st.maxVal : 1.0e12;
    const char* feat = st.feature == BlobKey::featCirc ? "circularity" : "area";
    HObject roi;
    if (stepDomain(rois, st.roiIndex, imgW, imgH, &roi)) {
        HObject inGroup;
        HObject outGroup;
        bool hasIn = false;
        bool hasOut = false;
        splitRegionByRoi(*region, roi, &inGroup, &hasIn, &outGroup, &hasOut);
        HObject result;
        HalconCpp::GenEmptyObj(&result);
        bool haveResult = false;
        if (hasIn) {
            HObject kept;
            HalconCpp::SelectShape(inGroup, &kept, feat, "and", st.minVal, hi);
            HTuple n;
            HalconCpp::CountObj(kept, &n);
            if (n.Length() >= 1 && n[0].L() >= 1) {
                result = kept;
                haveResult = true;
            }
        }
        if (hasOut) {
            if (haveResult) {
                HObject joined;
                HalconCpp::ConcatObj(result, outGroup, &joined);
                result = joined;
            } else {
                result = outGroup;
                haveResult = true;
            }
        }
        *region = result;
        return;
    }
    HObject out;
    HalconCpp::SelectShape(*region, &out, feat, "and", st.minVal, hi);
    *region = out;
}

bool runSteps(const ToolParaBlob::Run& rp, const HObject& image, const HObject& detect,
              const std::vector<HObject>& rois, int imgW, int imgH, HObject* region)
{
    if (!region) {
        return false;
    }
    bool haveBinary = false;
    HObject unionBinary;
    for (size_t i = 0; i < rp.steps.size(); ++i) {
        const ToolParaBlob::Step& st = rp.steps[i];
        if (st.type != BlobKey::stepThresh) {
            continue;
        }
        HObject stepDomainObj = detect;
        HObject bound;
        if (stepDomain(rois, st.roiIndex, imgW, imgH, &bound)) {
            stepDomainObj = bound;
        }
        HObject binary;
        if (!segmentDomain(st, image, stepDomainObj, &binary)) {
            return false;
        }
        if (!haveBinary) {
            unionBinary = binary;
            haveBinary = true;
        } else {
            HObject joined;
            HalconCpp::Union2(unionBinary, binary, &joined);
            unionBinary = joined;
        }
    }
    if (!haveBinary) {
        return false;
    }
    HObject conn;
    HalconCpp::Connection(unionBinary, &conn);
    *region = conn;
    for (size_t i = 0; i < rp.steps.size(); ++i) {
        const ToolParaBlob::Step& st = rp.steps[i];
        if (st.type == BlobKey::stepMorph) {
            stepMorph(st, rois, imgW, imgH, region);
        } else if (st.type == BlobKey::stepSelect) {
            stepSelect(st, rois, imgW, imgH, region);
        }
    }
    return true;
}

void applyJudge(const ToolParaBlob::Run& rp, AlgToolOut* out)
{
    if (!out || !out->ok) {
        return;
    }
    const int count = out->values.value("count").toInt(0);
    const double area = out->values.value("area").toDouble(0.0);
    if (rp.okMinCount > 0 && count < rp.okMinCount) {
        out->ok = false;
        out->message = Vision::acpToUtf8("Blob ?????????§Ø?????");
    } else if (rp.okMaxCount > 0 && count > rp.okMaxCount) {
        out->ok = false;
        out->message = Vision::acpToUtf8("Blob ?????????§Ø?????");
    } else if (rp.okMinArea > 0.0 && area + 1e-9 < rp.okMinArea) {
        out->ok = false;
        out->message = Vision::acpToUtf8("Blob ??????????§Ø?????");
    } else if (rp.okMaxArea > 0.0 && area > rp.okMaxArea + 1e-9) {
        out->ok = false;
        out->message = Vision::acpToUtf8("Blob ??????????§Ø?????");
    }
}
}

std::string HalBlob::version() const
{
    return kVer_HalBlob;
}

std::vector<VerNote> HalBlob::versionNotes() const
{
    return verNotesFrom(kNotes_HalBlob, static_cast<int>(sizeof(kNotes_HalBlob) / sizeof(kNotes_HalBlob[0])));
}

AlgToolOut HalBlob::run(const AlgToolIn& in)
{
    try {
        HImage gray;
        int w = 0;
        int h = 0;
        std::string err;
        HImage* pGray = &gray;
        if (!loadGrayFromAlgIn(in, pGray, &w, &h, &err)) {
            return algorToolFail(err);
        }
        ToolParam bag;
        bag.run = in.run;
        bag.judge = in.judge;
        ToolParaBlob para;
        para.load(bag);

        const std::vector<HObject> stepRois = collectStepRois(in, w, h);
        HObject detect = fullImageRegion(w, h);
        if (!stepRois.empty()) {
            detect = stepRois[0];
            for (size_t i = 1; i < stepRois.size(); ++i) {
                HObject joined;
                HalconCpp::Union2(detect, stepRois[i], &joined);
                detect = joined;
            }
        }
        if (!regionHasArea(detect)) {
            return algorToolFail(Vision::acpToUtf8("???????????"));
        }

        HObject region;
        if (!runSteps(para.run, gray, detect, stepRois, w, h, &region)) {
            AlgToolOut emptyOut;
            emptyOut.image.image = in.image.image;
            emptyOut.message = Vision::acpToUtf8("¦Ä????????");
            return emptyOut;
        }
        AlgToolOut out = packConnectedBlobs(region, in, 0.0, 0.0, 0.0, 1.0, Vision::acpToUtf8("Blob ???"));
        applyJudge(para.run, &out);
        return out;
    } catch (const HException& e) {
        return algorToolFail(hExceptionText(e));
    }
}

}
