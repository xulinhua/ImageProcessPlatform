/**
 * @file HalOps.cpp
 * @brief 模板句柄和区域的共用操作
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "HalOps.h"
#include "HalObjectConvert.h"

using HalconCpp::HObject;
using HalconCpp::HTuple;

namespace Vision {

void initFileNames()
{
    static bool done = false;
    if (done) {
        return;
    }
    HalconCpp::SetSystem("filename_encoding", "utf8");
    done = true;
}

bool modelOk(const HTuple& model)
{
    const Hlong count = model.TupleLength();
    for (Hlong i = 0; i < count; ++i) {
        try {
            if (model[i].L() == -1) {
                continue;
            }
        } catch (const HalconCpp::HException&) {
        }
        return true;
    }
    return false;
}

void clearShapeModel(HTuple& model)
{
    const Hlong count = model.TupleLength();
    for (Hlong i = 0; i < count; ++i) {
        try {
            if (model[i].L() != -1) {
                HalconCpp::ClearShapeModel(model[i]);
            }
        } catch (const HalconCpp::HException&) {
        }
    }
    model = -1;
}

void clearNccModel(HTuple& model)
{
    const Hlong count = model.TupleLength();
    for (Hlong i = 0; i < count; ++i) {
        try {
            if (model[i].L() != -1) {
                HalconCpp::ClearNccModel(model[i]);
            }
        } catch (const HalconCpp::HException&) {
        }
    }
    model = -1;
}

void morphOpenClose(const HObject& bin, double openR, double closeR, HObject*& out)
{
    if (!out) {
        return;
    }
    *out = bin;
    if (hoIsEmpty(*out)) {
        return;
    }
    if (openR > 0.4) {
        HObject tmp;
        HalconCpp::OpeningCircle(*out, &tmp, openR);
        *out = tmp;
    }
    if (closeR > 0.4 && !hoIsEmpty(*out)) {
        HObject tmp;
        HalconCpp::ClosingCircle(*out, &tmp, closeR);
        *out = tmp;
    }
}

bool regionHasArea(const HObject& region)
{
    try {
        HTuple area;
        HTuple row;
        HTuple col;
        HalconCpp::AreaCenter(region, &area, &row, &col);
        if (area.TupleLength() < 1) {
            return false;
        }
        return area[0].D() > 0.0;
    } catch (const HalconCpp::HException&) {
        return false;
    }
}

}
