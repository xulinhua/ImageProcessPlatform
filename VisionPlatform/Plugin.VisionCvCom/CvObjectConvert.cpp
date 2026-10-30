/**
 * @file CvObjectConvert.cpp
 * @brief CvObjectConvert实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "CvObjectConvert.h"

#include <HsCommData/Geom.h>
#include <HsCommData/HsContour.h>
#include <cstring>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace Vision {

// 掩膜是否无有效前景像素
bool maskIsEmpty(const cv::Mat& mask)
{
    if (mask.empty()) {
        return true;
    }
    return cv::countNonZero(mask) < 1;
}

// 整幅白掩膜
cv::Mat fullImageMask(int imgW, int imgH)
{
    if (imgW <= 0 || imgH <= 0) {
        return cv::Mat();
    }
    return cv::Mat(imgH, imgW, CV_8UC1, cv::Scalar(255));
}

// 线段扩成条带
static void drawLineStrip(cv::Mat* mask, double x1, double y1, double x2, double y2, double dilateR)
{
    const int t = dilateR > 0.1 ? static_cast<int>(dilateR * 2.0 + 0.5) : 1;
    cv::line(*mask, cv::Point(cvRound(x1), cvRound(y1)), cv::Point(cvRound(x2), cvRound(y2)),
             cv::Scalar(255), t < 1 ? 1 : t, 8);
}

// HsRegion 行程转掩膜
cv::Mat hsRegionToMask(const HsCommData::HsRegion& reg, int imgW, int imgH)
{
    cv::Mat mask = cv::Mat::zeros(imgH > 0 ? imgH : 0, imgW > 0 ? imgW : 0, CV_8UC1);
    if (imgW <= 0 || imgH <= 0 || reg.runsEmpty()) {
        return mask;
    }
    for (size_t i = 0; i < reg.runCount(); ++i) {
        const HsCommData::Chord& ch = reg.runAt(i);
        if (ch.row < 0 || ch.row >= imgH) {
            continue;
        }
        int cb = ch.cb < 0 ? 0 : ch.cb;
        int ce = ch.ce >= imgW ? imgW - 1 : ch.ce;
        if (ce < cb) {
            continue;
        }
        unsigned char* row = mask.ptr<unsigned char>(ch.row);
        memset(row + cb, 255, static_cast<size_t>(ce - cb + 1));
    }
    return mask;
}

// 单格几何画进已分配的零掩膜
static void drawGeomCell(cv::Mat* mask, const HsCommData::HsObject& obj, int index, int imgW, int imgH)
{
    using namespace HsCommData;
    if (!mask || mask->empty() || !obj.isInitialized() || index < 0 || index >= obj.count()) {
        return;
    }
    const ObjectKind k = obj.kindAt(index);
    // 面状几何直接填充；角度存弧度，OpenCV 要角度
    if (k == ObjRect1) {
        const Rect1P* rc = obj.rect1At(index);
        if (!rc) {
            return;
        }
        const int x = cvRound(rc->r.x);
        const int y = cvRound(rc->r.y);
        const int w = cvRound(rc->r.w);
        const int h = cvRound(rc->r.h);
        cv::rectangle(*mask, cv::Rect(x, y, w < 1 ? 1 : w, h < 1 ? 1 : h), cv::Scalar(255), cv::FILLED);
        return;
    }
    if (k == ObjRect2) {
        const Rect2P* rc = obj.rect2At(index);
        if (!rc) {
            return;
        }
        cv::RotatedRect rr(cv::Point2f(static_cast<float>(rc->c.x), static_cast<float>(rc->c.y)),
                           cv::Size2f(static_cast<float>(rc->l1 * 2.0), static_cast<float>(rc->l2 * 2.0)),
                           static_cast<float>(rc->phi * 180.0 / M_PI));
        cv::Point2f pts[4];
        rr.points(pts);
        cv::Point ipt[4];
        for (int i = 0; i < 4; ++i) {
            ipt[i] = cv::Point(cvRound(pts[i].x), cvRound(pts[i].y));
        }
        cv::fillConvexPoly(*mask, ipt, 4, cv::Scalar(255));
        return;
    }
    if (k == ObjEllipse) {
        const EllipseP* e = obj.ellipseAt(index);
        if (!e) {
            return;
        }
        cv::ellipse(*mask,
                    cv::Point(cvRound(e->c.x), cvRound(e->c.y)),
                    cv::Size(cvRound(e->ra), cvRound(e->rb)),
                    e->phi * 180.0 / M_PI, 0.0, 360.0, cv::Scalar(255), cv::FILLED);
        return;
    }
    if (k == ObjCircle) {
        const CircleP* c = obj.circleAt(index);
        if (!c) {
            return;
        }
        cv::circle(*mask, cv::Point(cvRound(c->c.x), cvRound(c->c.y)),
                   cvRound(c->r), cv::Scalar(255), cv::FILLED);
        return;
    }
    // 线和两点轮廓画成约 16 像素宽的条带；多点轮廓按闭合多边形填充
    if (k == ObjLine) {
        const LineP* ln = obj.lineAt(index);
        if (!ln) {
            return;
        }
        drawLineStrip(mask, ln->a.x, ln->a.y, ln->b.x, ln->b.y, 8.0);
        return;
    }
    if (k == ObjContour) {
        const HsContour* c = obj.contourAt(index);
        if (!c || c->ptCount() < 2) {
            return;
        }
        if (c->ptCount() == 2) {
            drawLineStrip(mask, c->ptAt(0).x, c->ptAt(0).y, c->ptAt(1).x, c->ptAt(1).y, 8.0);
            return;
        }
        std::vector<cv::Point> pts;
        pts.reserve(c->ptCount());
        for (size_t i = 0; i < c->ptCount(); ++i) {
            pts.push_back(cv::Point(cvRound(c->ptAt(i).x), cvRound(c->ptAt(i).y)));
        }
        const cv::Point* p0 = &pts[0];
        int n = static_cast<int>(pts.size());
        cv::fillPoly(*mask, &p0, &n, 1, cv::Scalar(255));
        return;
    }
    // 行程区域并入已有掩膜
    if (k == ObjRegion) {
        const HsRegion* reg = obj.regionAt(index);
        if (!reg || reg->runsEmpty()) {
            return;
        }
        cv::Mat one = hsRegionToMask(*reg, imgW, imgH);
        cv::bitwise_or(*mask, one, *mask);
    }
}

// 单格或整袋 HsObject 转掩膜
cv::Mat hsObjectToMask(const HsCommData::HsObject& obj, int imgW, int imgH)
{
    if (imgW <= 0 || imgH <= 0 || !obj.isInitialized() || obj.isEmpty()) {
        return fullImageMask(imgW, imgH);
    }
    cv::Mat acc = cv::Mat::zeros(imgH, imgW, CV_8UC1);
    const int n = obj.count();
    for (int i = 0; i < n; ++i) {
        if (obj.kindAt(i) == HsCommData::ObjImage) {
            continue;
        }
        drawGeomCell(&acc, obj, i, imgW, imgH);
    }
    if (maskIsEmpty(acc)) {
        return fullImageMask(imgW, imgH);
    }
    return acc;
}

// 多只上游区域对象并集；空则整幅图
cv::Mat hsRegionsToMask(const std::vector<HsCommData::HsObject>& regs, int imgW, int imgH)
{
    if (regs.empty() || imgW <= 0 || imgH <= 0) {
        return fullImageMask(imgW, imgH);
    }
    cv::Mat acc = cv::Mat::zeros(imgH, imgW, CV_8UC1);
    for (size_t i = 0; i < regs.size(); ++i) {
        const HsCommData::HsObject& obj = regs[i];
        if (!obj.isInitialized()) {
            continue;
        }
        const int n = obj.count();
        for (int ei = 0; ei < n; ++ei) {
            if (obj.kindAt(ei) != HsCommData::ObjRegion) {
                continue;
            }
            const HsCommData::HsRegion* reg = obj.regionAt(ei);
            if (!reg || reg->runsEmpty()) {
                continue;
            }
            cv::Mat one = hsRegionToMask(*reg, imgW, imgH);
            cv::bitwise_or(acc, one, acc);
        }
    }
    if (maskIsEmpty(acc)) {
        return fullImageMask(imgW, imgH);
    }
    return acc;
}

// 单只 HsObject 转掩膜；无效则整幅图
cv::Mat toolRoiToMask(const HsCommData::HsObject& roi, int imgW, int imgH)
{
    if (!roi.isInitialized() || roi.isEmpty() || imgW <= 0 || imgH <= 0) {
        return fullImageMask(imgW, imgH);
    }
    return hsObjectToMask(roi, imgW, imgH);
}

// 多 ROI 并集；空则整幅图
cv::Mat toolRoisToMask(const std::vector<HsCommData::HsObject>& rois, int imgW, int imgH)
{
    if (rois.empty()) {
        return fullImageMask(imgW, imgH);
    }
    cv::Mat acc = cv::Mat::zeros(imgH > 0 ? imgH : 0, imgW > 0 ? imgW : 0, CV_8UC1);
    for (size_t i = 0; i < rois.size(); ++i) {
        cv::Mat one = toolRoiToMask(rois[i], imgW, imgH);
        if (one.size() != acc.size() || one.type() != acc.type()) {
            continue;
        }
        cv::bitwise_or(acc, one, acc);
    }
    if (maskIsEmpty(acc)) {
        return fullImageMask(imgW, imgH);
    }
    return acc;
}

// 二值掩膜转 HsObject
HsCommData::HsObject maskToRegionObject(const cv::Mat& mask)
{
    HsCommData::HsRegion reg;
    if (maskIsEmpty(mask)) {
        return HsCommData::HsObject(reg);
    }
    int32_t minR = 0;
    int32_t maxR = 0;
    int32_t minC = 0;
    int32_t maxC = 0;
    bool first = true;
    for (int y = 0; y < mask.rows; ++y) {
        const unsigned char* row = mask.ptr<unsigned char>(y);
        int x = 0;
        while (x < mask.cols) {
            while (x < mask.cols && row[x] == 0) {
                ++x;
            }
            if (x >= mask.cols) {
                break;
            }
            const int cb = x;
            while (x < mask.cols && row[x] != 0) {
                ++x;
            }
            const int ce = x - 1;
            reg.pushRun(HsCommData::Chord(y, cb, ce));
            if (first) {
                minR = maxR = y;
                minC = cb;
                maxC = ce;
                first = false;
            } else {
                if (y < minR) minR = y;
                if (y > maxR) maxR = y;
                if (cb < minC) minC = cb;
                if (ce > maxC) maxC = ce;
            }
        }
    }
    if (!reg.runsEmpty()) {
        reg.bounds = HsCommData::fromInclusive(minC, minR, maxC, maxR);
    }
    return HsCommData::HsObject(reg);
}

// 结果圆转 HsObject
HsCommData::HsObject circleObject(double cx, double cy, double r)
{
    HsCommData::CircleP c;
    c.c = HsCommData::Point2d(cx, cy);
    c.r = r;
    return HsCommData::HsObject(c);
}

// 结果线段转 HsObject
HsCommData::HsObject lineObject(double x1, double y1, double x2, double y2)
{
    HsCommData::LineP ln;
    ln.a = HsCommData::Point2d(x1, y1);
    ln.b = HsCommData::Point2d(x2, y2);
    return HsCommData::HsObject(ln);
}

}
