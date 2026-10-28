/**
 * @file HalCommon.cpp
 * @brief HalCommon实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "HalCommon.h"
#include "TextEncoding.h"
#include "HalConvert.h"
#include "HalGeom.h"

using HalconCpp::HObject;
using HalconCpp::HImage;
using HalconCpp::HTuple;
using HalconCpp::OpeningCircle;
using HalconCpp::ClosingCircle;
using HalconCpp::Connection;
using HalconCpp::SelectShape;
using HalconCpp::AreaCenter;
using HalconCpp::CountObj;
using HalconCpp::ReduceDomain;

namespace Vision {

// 从输入取灰度与尺寸
bool loadGrayFromAlgIn(const AlgToolIn& in, HImage* gray, int* w, int* h, std::string* err)
{
    if (!in.image || in.image->isNull()) {
        if (err) {
            *err = "无输入图";
        }
        return false;
    }
    if (!frameToHImageGray(*in.image, gray, err)) {
        return false;
    }
    HTuple tw;
    HTuple th;
    gray->GetImageSize(&tw, &th);
    *w = static_cast<int>(tw.L());
    *h = static_cast<int>(th.L());
    return true;
}

// 形态学开闭
void morphRegion(const HObject& bin, double openR, double closeR, HObject* out)
{
    *out = bin;
    if (hoIsEmpty(*out)) {
        return;
    }
    if (openR > 0.4) {
        HObject tmp;
        OpeningCircle(*out, &tmp, openR);
        *out = tmp;
    }
    if (closeR > 0.4 && !hoIsEmpty(*out)) {
        HObject tmp;
        ClosingCircle(*out, &tmp, closeR);
        *out = tmp;
    }
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
    out.image = *in.image;
    HTuple n;
    CountObj(sel, &n);
    if (n.Length() < 1 || n[0].L() < 1) {
        out.message = "未找到连通域";
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
    out.objects.push_back(regionObject(sel));
    out.message = okMsg;
    return out;
}

// ROI 内预处理底图
bool preparePreprocessWork(const AlgToolIn& in, HImage* gray, HImage* work, std::string* err)
{
    int w = 0;
    int h = 0;
    if (!loadGrayFromAlgIn(in, gray, &w, &h, err)) {
        return false;
    }
    const HObject roi = roisToRegion(in.rois, w, h);
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
    if (!hImageToFrame(full, in.image->frameId, &out.image, &err)) {
        return algorToolFail(err);
    }
    out.ok = true;
    out.message = okMsg;
    return out;
}

}
