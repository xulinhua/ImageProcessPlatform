/**
 * @file HalAlgoHelpers.cpp
 * @brief HalAlgoHelpers实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "HalAlgoHelpers.h"

#include "TextEncoding.h"
#include "HalImageConvert.h"
#include "HalObjectConvert.h"

using HalconCpp::HObject;
using HalconCpp::HImage;
using HalconCpp::HTuple;
using HalconCpp::Connection;
using HalconCpp::SelectShape;
using HalconCpp::AreaCenter;
using HalconCpp::CountObj;
using HalconCpp::CountChannels;
using HalconCpp::Rgb1ToGray;
using HalconCpp::ReduceDomain;
using HalconCpp::Union2;

namespace Vision {

// 从输入取灰度与尺寸
bool loadGrayFromAlgIn(const AlgToolIn& in, HImage*& gray, int* w, int* h, std::string* err)
{
    if (in.image.image.isNull()) {
        if (err) {
            *err = Vision::acpToUtf8("无输入图");
        }
        return false;
    }
    HObject ho;
    HObject* pHo = &ho;
    if (!hsImageToHObject(in.image.image, pHo, err)) {
        return false;
    }
    try {
        HTuple ch;
        CountChannels(ho, &ch);
        if (ch.Length() >= 1 && ch[0].L() >= 3) {
            HObject grayHo;
            Rgb1ToGray(ho, &grayHo);
            ho = grayHo;
        }
    } catch (const HalconCpp::HException& e) {
        if (err) {
            *err = hExceptionText(e);
        }
        return false;
    }
    *gray = HImage(ho);
    HTuple tw;
    HTuple th;
    gray->GetImageSize(&tw, &th);
    *w = static_cast<int>(tw.L());
    *h = static_cast<int>(th.L());
    return true;
}

// 本地 ROI ∪ 上游区域（空 ROI 列表表示无本地约束）
HObject searchDomainFromAlgIn(const std::vector<HsCommData::HsObject>& rois,
                                const std::vector<HsCommData::HsObject>& regions,
                                int imgW, int imgH)
{
    if (regions.empty()) {
        return toolRoisToHObject(rois, imgW, imgH);
    }
    const HObject mask = hsRegionsToHObject(regions, imgW, imgH);
    if (rois.empty()) {
        return mask;
    }
    const HObject local = toolRoisToHObject(rois, imgW, imgH);
    HObject uni;
    Union2(local, mask, &uni);
    if (hoIsEmpty(uni)) {
        return local;
    }
    return uni;
}

// 连通域筛选打包
AlgToolOut packConnectedBlobs(const HObject& bin, const AlgToolIn& in,
                                double minArea, double maxArea, double minCirc, double maxCirc,
                                const std::string& okMsg)
{
    HObject conn;
    Connection(bin, &conn);
    HObject sel;
    SelectShape(conn, &sel, "area", "and", minArea, maxArea > 0.0 ? maxArea : 1.0e12);
    if (minCirc > 0.0 || maxCirc < 0.999) {
        HObject circ;
        SelectShape(sel, &circ, "circularity", "and", minCirc, maxCirc);
        sel = circ;
    }
    AlgToolOut out;
    out.image.image = in.image.image;
    HTuple n;
    CountObj(sel, &n);
    if (n.Length() < 1 || n[0].L() < 1) {
        out.message = Vision::acpToUtf8("未找到连通域");
        return out;
    }
    HTuple rows;
    HTuple cols;
    HTuple areas;
    AreaCenter(sel, &areas, &rows, &cols);
    double sumA = 0;
    for (Hlong i = 0; i < areas.Length(); ++i) {
        sumA += areas[i].D();
    }
    out.ok = true;
    out.values.insert("count", static_cast<int>(n[0].L()));
    out.values.insert("area", sumA);
    out.objects.push_back(hObjectToRegionObject(sel));
    out.message = okMsg;
    return out;
}

// ROI 内预处理底图
bool preparePreprocessWork(const AlgToolIn& in, HImage*& gray, HImage*& work, std::string* err)
{
    int w = 0;
    int h = 0;
    if (!loadGrayFromAlgIn(in, gray, &w, &h, err)) {
        return false;
    }
    const HObject roi = toolRoisToHObject(plainRois(in.rois), w, h);
    HObject reduced;
    ReduceDomain(*gray, roi, &reduced);
    *work = HImage(reduced);
    return true;
}

// FullDomain 写回
AlgToolOut finishPreprocessWork(const AlgToolIn& in, const HImage& work, const std::string& okMsg)
{
    AlgToolOut out;
    HImage full = work.FullDomain();
    std::string err;
    HsCommData::HsImage* pImage = &out.image.image;
    if (!hObjectToHsImage(full, in.image.image.frameId, pImage, &err)) {
        return algorToolFail(err);
    }
    out.ok = true;
    out.message = okMsg;
    return out;
}

// 统一失败包
AlgToolOut algorToolFail(const std::string& msg)
{
    AlgToolOut o;
    o.ok = false;
    o.errCode = 1; o.message = msg;
    return o;
}


// 兼容旧头：请改用 HalImageConvert.h
bool frameToHImageGray(const HsCommData::HsImage& fb, HalconCpp::HImage*& out, std::string* err)
{
    if (!out) {
        if (err) {
            *err = "HImage is null";
        }
        return false;
    }
    HalconCpp::HObject ho;
    HalconCpp::HObject* pHo = &ho;
    if (!hsImageToHObject(fb, pHo, err)) {
        return false;
    }
    try {
        HalconCpp::HTuple ch;
        HalconCpp::CountChannels(ho, &ch);
        if (ch.Length() >= 1 && ch[0].L() >= 3) {
            HalconCpp::HObject grayHo;
            HalconCpp::Rgb1ToGray(ho, &grayHo);
            ho = grayHo;
        }
    } catch (const HalconCpp::HException& e) {
        if (err) {
            *err = hExceptionText(e);
        }
        return false;
    }
    *out = HalconCpp::HImage(ho);
    return true;
}

}
