/**
 * @file HalTexture.cpp
 * @brief HalTexture实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "HalTexture.h"
#include "TextEncoding.h"
#include "HalAlgoHelpers.h"
#include "HalImageConvert.h"
#include "HalObjectConvert.h"
#include "HalOps.h"

using HalconCpp::HObject;
using HalconCpp::HImage;
using HalconCpp::HException;
using HalconCpp::ReduceDomain;
using HalconCpp::Threshold;
using HalconCpp::MeanImage;
using HalconCpp::AbsDiffImage;
using HalconCpp::MedianRect;
using HalconCpp::GrayClosingRect;

namespace Vision {

// 大核背景差
namespace {
const char kVer_HalTexture[] = "1.0.0.0";
const Vision::VerNoteRaw kNotes_HalTexture[] = {
    { "1.0.0.0", "首版" },
};
}

std::string HalTexture::version() const
{
    return kVer_HalTexture;
}

// 版本变更，只追加
std::vector<VerNote> HalTexture::versionNotes() const
{
    return verNotesFrom(kNotes_HalTexture, static_cast<int>(sizeof(kNotes_HalTexture) / sizeof(kNotes_HalTexture[0])));
}
AlgToolOut HalTexture::run(const AlgToolIn& in)
{
    try {
        HImage gray;
        int w = 0;
        int h = 0;
        std::string err;
        HalconCpp::HImage* pGray = &gray;
        if (!loadGrayFromAlgIn(in, pGray, &w, &h, &err)) {
            return algorToolFail(err);
        }
        const HObject roi = toolRoisToHObject(plainRois(in.rois), w, h);
        HObject local;
        ReduceDomain(gray, roi, &local);
        int kw = in.run.value("kernelW").toInt(31);
        int kh = in.run.value("kernelH").toInt(31);
        if (kw < 3) {
            kw = 3;
        }
        if (kh < 3) {
            kh = 3;
        }
        const std::string method = in.run.value("bgMethod").toString();
        HObject bg;
        if (method == "median") {
            MedianRect(local, &bg, kw, kh);
        } else if (method == "closing") {
            GrayClosingRect(local, &bg, kh, kw);
        } else {
            MeanImage(local, &bg, kw, kh);
        }
        HObject diff;
        AbsDiffImage(local, bg, &diff, 1.0);
        const double th = in.run.value("residThresh").toDouble(18.0);
        HObject bin;
        Threshold(diff, &bin, th, 255.0);
        HObject morphed;
        HalconCpp::HObject* pMorphed = &morphed;
        morphOpenClose(bin, in.run.value("openR").toDouble(1.5), 0.0, pMorphed);
        return packConnectedBlobs(morphed, in,
                                  in.run.value("minArea").toDouble(30.0),
                                  in.run.value("maxArea").toDouble(0.0),
                                  0.0, 1.0,
                                  Vision::acpToUtf8("纹理缺陷完成"));
    } catch (const HException& e) {
        return algorToolFail(hExceptionText(e));
    }
}

}
