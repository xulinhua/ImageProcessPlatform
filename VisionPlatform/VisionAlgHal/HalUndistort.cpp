/**
 * @file HalUndistort.cpp
 * @brief HalUndistort实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "HalUndistort.h"
#include "HalImageConvert.h"
#include "HalconCpp.h"
#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::string U(const wchar_t* w)
{
    std::string o;
    if (!w) {
        return o;
    }
    for (; *w; ++w) {
        unsigned int c = static_cast<unsigned int>(*w);
        if (c < 0x80u) {
            o.push_back(static_cast<char>(c));
        } else if (c < 0x800u) {
            o.push_back(static_cast<char>(0xC0u | (c >> 6)));
            o.push_back(static_cast<char>(0x80u | (c & 0x3Fu)));
        } else {
            o.push_back(static_cast<char>(0xE0u | (c >> 12)));
            o.push_back(static_cast<char>(0x80u | ((c >> 6) & 0x3Fu)));
            o.push_back(static_cast<char>(0x80u | (c & 0x3Fu)));
        }
    }
    return o;
}

HalconCpp::HTuple parseCam(const std::string& text)
{
    HalconCpp::HTuple t;
    std::istringstream in(text);
    std::string tok;
    bool first = true;
    while (in >> tok) {
        if (first) {
            t.Append(tok.c_str());
            first = false;
            continue;
        }
        t.Append(atof(tok.c_str()));
    }
    return t;
}

}

// 用 Halcon 相机参数做径向畸变校正
namespace {
const char kVer_HalUndistort[] = "1.0.0.0";
const Vision::VerNoteRaw kNotes_HalUndistort[] = {
    { "1.0.0.0", "首版" },
};
}

std::string Vision::HalUndistort::version() const
{
    return kVer_HalUndistort;
}

// 版本变更，只追加
std::vector<Vision::VerNote> Vision::HalUndistort::versionNotes() const
{
    return Vision::verNotesFrom(kNotes_HalUndistort, static_cast<int>(sizeof(kNotes_HalUndistort) / sizeof(kNotes_HalUndistort[0])));
}
Vision::AlgToolOut Vision::HalUndistort::run(const Vision::AlgToolIn& in)
{
    AlgToolOut out;
    if (inputImage(in).isNull()) {
        out.message = U(L"\u6ca1\u6709\u8f93\u5165\u56fe");
        return out;
    }
    const std::string camText = in.run.value("halconCam").toString();
    if (camText.empty()) {
        out.message = U(L"\u6ca1\u6709 Halcon \u76f8\u673a\u53c2\u6570");
        return out;
    }
    try {
        HalconCpp::HObject raw;
        HalconCpp::HObject* rawPtr = &raw;
        std::string err;
        if (!hsImageToHObject(inputImage(in), rawPtr, &err)) {
            out.message = err;
            return out;
        }
        HalconCpp::HImage img(raw);
        HalconCpp::HCamPar camIn(parseCam(camText));
        HalconCpp::HCamPar camOut = camIn.ChangeRadialDistortionCamPar("adaptive", 0.0);
        HalconCpp::HImage fixed = img.ChangeRadialDistortionImage(img.GetDomain(), camIn, camOut);
        HsCommData::HsImage image;
        HsCommData::HsImage* imagePtr = &image;
        if (!hObjectToHsImage(fixed, inputImage(in).frameId, imagePtr, &err)) {
            out.message = err;
            return out;
        }
        out.image.image = image;
        out.ok = true;
        out.message = U(L"\u5df2\u6821\u6b63");
    } catch (const HalconCpp::HException& e) {
        out.message = e.ErrorMessage().Text();
    }
    return out;
}
