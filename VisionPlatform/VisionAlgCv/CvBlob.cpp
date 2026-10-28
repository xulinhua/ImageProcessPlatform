/**
 * @file CvBlob.cpp
 * @brief CvBlob实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "CvBlob.h"
#include "TextEncoding.h"
#include "CvAlgoHelpers.h"
#include "CvObjectConvert.h"
#include "CvOps.h"
#include "ToolParaBlob.hpp"

namespace Vision {

// 绝对、相对、动态阈值
namespace {
const char kVer_CvBlob[] = "1.0.0.0";
const Vision::VerNoteRaw kNotes_CvBlob[] = {
    { "1.0.0.0", "首版" },
};
}

std::string CvBlob::version() const
{
    return kVer_CvBlob;
}

std::vector<Vision::VerNote> CvBlob::versionNotes() const
{
    return Vision::verNotesFrom(kNotes_CvBlob, static_cast<int>(sizeof(kNotes_CvBlob) / sizeof(kNotes_CvBlob[0])));
}
AlgToolOut CvBlob::run(const AlgToolIn& in)
{
    try {
        cv::Mat gray;
        int w = 0;
        int h = 0;
        std::string err;
        cv::Mat* pGray = &gray;
        if (!loadGrayFromAlgIn(in, pGray, &w, &h, &err)) {
            return algorToolFail(err);
        }
        cv::Mat roi = searchDomainFromAlgIn(plainRois(in.rois), plainRegions(in.regions), w, h);
        if (maskIsEmpty(roi)) {
            return algorToolFail("搜索区域为空");
        }
        cv::Mat local;
        gray.copyTo(local, roi);
                ToolParam bag;
        bag.run = in.run;
        ToolParaBlob para;
        para.load(bag);
        const ToolParaBlob::Step* th = 0;
        double openR = 0.0;
        double closeR = 0.0;
        double minArea = 50.0;
        double maxArea = 0.0;
        double minCirc = 0.0;
        double maxCirc = 1.0;
        bool haveArea = false;
        for (size_t si = 0; si < para.run.steps.size(); ++si) {
            const ToolParaBlob::Step& st = para.run.steps[si];
            if (st.type == BlobKey::stepThresh && th == 0) {
                th = &para.run.steps[si];
            } else if (st.type == BlobKey::stepMorph && st.morphOp == BlobKey::morphOpen) {
                openR = st.radius;
            } else if (st.type == BlobKey::stepMorph && st.morphOp == BlobKey::morphClose) {
                closeR = st.radius;
            } else if (st.type == BlobKey::stepSelect && st.feature == BlobKey::featArea && !haveArea) {
                minArea = st.minVal;
                maxArea = st.maxVal;
                haveArea = true;
            } else if (st.type == BlobKey::stepSelect && st.feature == BlobKey::featCirc) {
                minCirc = st.minVal;
                maxCirc = st.maxVal;
            }
        }
        ToolParaBlob::Step fallback;
        if (th == 0) {
            th = &fallback;
        }
        const std::string mode = th->threshMode;
        const bool dark = th->dark;
        cv::Mat bin;
        if (mode == "relative") {
            const cv::Scalar mean = cv::mean(local, roi);
            const double m = mean[0];
            const double off = th->relOffset;
            if (dark) {
                cv::threshold(local, bin, m - off, 255.0, cv::THRESH_BINARY_INV);
            } else {
                cv::threshold(local, bin, m + off, 255.0, cv::THRESH_BINARY);
            }
        } else if (mode == "dynamic") {
            int k = th->dynKernel;
            if (k < 3) {
                k = 3;
            }
            if ((k % 2) == 0) {
                ++k;
            }
            cv::Mat bg;
            cv::blur(local, bg, cv::Size(k, k));
            const double off = th->dynOffset;
            cv::Mat diff;
            if (dark) {
                cv::subtract(bg, local, diff);
            } else {
                cv::subtract(local, bg, diff);
            }
            cv::threshold(diff, bin, off, 255.0, cv::THRESH_BINARY);
        } else {
            const double amin = th->absMin;
            const double amax = th->absMax;
            if (dark) {
                cv::threshold(local, bin, amin, 255.0, cv::THRESH_BINARY_INV);
            } else {
                cv::inRange(local, cv::Scalar(amin), cv::Scalar(amax), bin);
            }
        }
        cv::bitwise_and(bin, roi, bin);
        if (maskIsEmpty(bin)) {
            AlgToolOut emptyOut;
            emptyOut.image.image = inputImage(in);
            emptyOut.message = "未找到连通域";
            return emptyOut;
        }
        cv::Mat morphed;
        cv::Mat* pMorphed = &morphed;
        morphOpenClose(bin, openR, closeR, pMorphed);
        AlgToolOut out = packConnectedBlobs(morphed, in,
                                  minArea,
                                  maxArea,
                                  minCirc,
                                  maxCirc,
                                  "Blob 完成");
        // 结果判定：数量/总面积超出窗口则 NG
        if (out.ok) {
            const int count = out.values.value("count").toInt(0);
            const double area = out.values.value("area").toDouble(0.0);
            const int okMinCount = para.run.okMinCount;
            const int okMaxCount = para.run.okMaxCount;
            const double okMinArea = para.run.okMinArea;
            const double okMaxArea = para.run.okMaxArea;
            if (okMinCount > 0 && count < okMinCount) {
                out.ok = false;
                out.message = "Blob 数量低于判定下限";
            } else if (okMaxCount > 0 && count > okMaxCount) {
                out.ok = false;
                out.message = "Blob 数量超过判定上限";
            } else if (okMinArea > 0.0 && area + 1e-9 < okMinArea) {
                out.ok = false;
                out.message = "Blob 总面积低于判定下限";
            } else if (okMaxArea > 0.0 && area > okMaxArea + 1e-9) {
                out.ok = false;
                out.message = "Blob 总面积超过判定上限";
            }
        }
        return out;
    } catch (const cv::Exception& e) {
        return algorToolFail(e.what() ? std::string(e.what()) : std::string("OpenCV error"));
    }
}

}
