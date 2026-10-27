/**
 * @file HalTmpl.cpp
 * @brief HalTmpl实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "HalTmpl.h"
#include "TextEncoding.h"
#include "HalAlgoHelpers.h"
#include "HalImageConvert.h"
#include "HalObjectConvert.h"
#include "ParamKeys.h"
#include <cstdio>

namespace Vision {

// 角度范围（度）转弧度半宽
static double angleRad(const ParamMap& p)
{
    const double deg = p.value("angleRange").toDouble(15.0);
    return deg * 3.14159265358979323846 / 180.0;
}

// 默认相对模型名
static std::string defaultModelFile(const std::string& method)
{
    if (method == "shape") {
        return std::string("model.shm");
    }
    return std::string("model.ncm");
}

// 解析模型绝对路径
static std::string modelAbsPath(const AlgToolIn& in)
{
    const std::string method = in.run.value("method").toString();
    std::string rel = in.run.value("modelFile").toString();
    if (rel.empty()) {
        rel = defaultModelFile(method);
    }
    if (in.toolDataDir.empty()) {
        return std::string();
    }
    if (rel.size() >= 2 && rel[1] == ':') {
        return rel;
    }
    return in.toolDataDir + "\\" + rel;
}

// 文件是否存在
static bool fileExists(const std::string& path)
{
    if (path.empty()) {
        return false;
    }
    FILE* fp = nullptr;
    if (fopen_s(&fp, path.c_str(), "rb") != 0 || !fp) {
        return false;
    }
    fclose(fp);
    return true;
}

// NCC 或形状匹配：优先加载 toolDataDir 中模型，否则 ROI 现建并可落盘
namespace {
const char kVer_HalTmpl[] = "1.0.0.0";
const Vision::VerNoteRaw kNotes_HalTmpl[] = {
    { "1.0.0.0", "首版" },
};
}

std::string HalTmpl::version() const
{
    return kVer_HalTmpl;
}

// 版本变更，只追加
std::vector<VerNote> HalTmpl::versionNotes() const
{
    return verNotesFrom(kNotes_HalTmpl, static_cast<int>(sizeof(kNotes_HalTmpl) / sizeof(kNotes_HalTmpl[0])));
}
AlgToolOut HalTmpl::run(const AlgToolIn& in)
{
    try {
        HalconCpp::HImage gray;
        int w = 0;
        int h = 0;
        std::string err;
        HalconCpp::HImage* pGray = &gray;
        if (!loadGrayFromAlgIn(in, pGray, &w, &h, &err)) {
            return algorToolFail(err);
        }
        const std::string method = in.run.value("method").toString();
        const double minScore = in.run.value("minScore").toDouble(0.5);
        const double a = angleRad(in.run);
        const int num = in.run.value("numMatches").toInt(1);
        const bool rebuild = in.run.value("rebuildModel").toBool(false);
        const std::string modelPath = modelAbsPath(in);
        const bool tryLoad = !rebuild && fileExists(modelPath);
        bool usedLoaded = false;

        HalconCpp::HTuple row;
        HalconCpp::HTuple col;
        HalconCpp::HTuple ang;
        HalconCpp::HTuple score;

        if (method == "shape") {
            HalconCpp::HShapeModel model;
            if (tryLoad) {
                try {
                    model.ReadShapeModel(modelPath.c_str());
                    usedLoaded = true;
                } catch (const HalconCpp::HException&) {
                    usedLoaded = false;
                }
            }
            if (!usedLoaded) {
                const HalconCpp::HObject roi = toolRoisToHObject(plainRois(in.rois), w, h);
                HalconCpp::HObject reduced;
                HalconCpp::ReduceDomain(gray, roi, &reduced);
                HalconCpp::HObject cropped;
                HalconCpp::CropDomain(reduced, &cropped);
                HalconCpp::HImage crop(cropped);
                model.CreateShapeModel(crop, HalconCpp::HTuple("auto"), -a, 2.0 * a, HalconCpp::HTuple("auto"),
                                       HalconCpp::HTuple("auto"), HalconCpp::HString("use_polarity"),
                                       HalconCpp::HTuple("auto"), HalconCpp::HTuple("auto"));
                if (!modelPath.empty()) {
                    model.WriteShapeModel(modelPath.c_str());
                }
            }
            model.FindShapeModel(gray, -a, 2.0 * a, minScore, num < 1 ? 1 : num, 0.5,
                                 "least_squares", 0, 0.9, &row, &col, &ang, &score);
        } else {
            HalconCpp::HNCCModel model;
            if (tryLoad) {
                try {
                    model.ReadNccModel(modelPath.c_str());
                    usedLoaded = true;
                } catch (const HalconCpp::HException&) {
                    usedLoaded = false;
                }
            }
            if (!usedLoaded) {
                const HalconCpp::HObject roi = toolRoisToHObject(plainRois(in.rois), w, h);
                HalconCpp::HObject reduced;
                HalconCpp::ReduceDomain(gray, roi, &reduced);
                HalconCpp::HObject cropped;
                HalconCpp::CropDomain(reduced, &cropped);
                HalconCpp::HImage crop(cropped);
                model.CreateNccModel(crop, HalconCpp::HTuple("auto"), -a, 2.0 * a, HalconCpp::HTuple("auto"),
                                     HalconCpp::HString("use_polarity"));
                if (!modelPath.empty()) {
                    model.WriteNccModel(modelPath.c_str());
                }
            }
            model.FindNccModel(gray, -a, 2.0 * a, minScore, num < 1 ? 1 : num, 0.5, "true", 0,
                               &row, &col, &ang, &score);
        }

        AlgToolOut out;
        out.image.image = in.image.image;
        if (row.Length() < 1) {
            out.message = Vision::acpToUtf8("未匹配到模板");
            return out;
        }
        const double cx = col[0].D();
        const double cy = row[0].D();
        const double phi = ang[0].D();
        const double sc = score[0].D();
        out.ok = true;
        paramSet(&out.values, ValueKey::X, cx);
        paramSet(&out.values, ValueKey::Y, cy);
        paramSet(&out.values, ValueKey::Angle, phi);
        paramSet(&out.values, ValueKey::Score, sc);
        out.objects.push_back(crossObject(cx, cy));
        if (usedLoaded) {
            out.message = Vision::acpToUtf8("模板定位成功（已加载模型）");
        } else if (!modelPath.empty()) {
            out.message = Vision::acpToUtf8("模板定位成功（已创建并保存模型）");
        } else {
            out.message = Vision::acpToUtf8("模板定位成功（内存模型，请先保存方案）");
        }
        return out;
    } catch (const HalconCpp::HException& e) {
        return algorToolFail(hExceptionText(e));
    }
}

}
