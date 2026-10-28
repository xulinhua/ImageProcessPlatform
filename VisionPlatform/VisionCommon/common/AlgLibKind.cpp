/**
 * @file AlgLibKind.cpp
 * @brief AlgLibKind实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "AlgLibKind.h"
#include <cctype>

namespace Vision {

namespace {

// 小写拷贝，便于比较
std::string toLowerAscii(const std::string& s)
{
    std::string o = s;
    for (size_t i = 0; i < o.size(); ++i) {
        o[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(o[i])));
    }
    return o;
}

} // namespace

// UI 短标题
const char* algLibKindTitle(AlgLibKind k)
{
    switch (k) {
    case AlgLibHal:  return "Hal";
    case AlgLibHik:  return "Hik";
    case AlgLibHray: return "Hray";
    case AlgLibCv:   return "CV";
    case AlgLibPcl:  return "PCL";
    case AlgLibOpen3d: return "Open3D";
    case AlgLibOnnx: return "Onnx";
    default:          return "";
    }
}

// 插件登记名
const char* algLibKindRegName(AlgLibKind k)
{
    switch (k) {
    case AlgLibHal:  return "halcon";
    case AlgLibHik:  return "hik";
    case AlgLibHray: return "hray";
    case AlgLibCv:   return "opencv";
    case AlgLibPcl:  return "pcl";
    case AlgLibOpen3d: return "open3d";
    case AlgLibOnnx: return "onnx";
    default:          return "";
    }
}

// 登记名 → 枚举
bool algLibKindFromRegName(const std::string& regName, AlgLibKind* out)
{
    if (!out || regName.empty()) {
        return false;
    }
    const std::string s = toLowerAscii(regName);
    if (s == "halcon" || s == "hal") {
        *out = AlgLibHal;
        return true;
    }
    if (s == "hik" || s == "hikvision") {
        *out = AlgLibHik;
        return true;
    }
    if (s == "hray" || s == "huary") {
        *out = AlgLibHray;
        return true;
    }
    if (s == "opencv" || s == "cv" || s == "open_cv") {
        *out = AlgLibCv;
        return true;
    }
    if (s == "pcl") {
        *out = AlgLibPcl;
        return true;
    }
    if (s == "open3d" || s == "open3D") {
        *out = AlgLibOpen3d;
        return true;
    }
    if (s == "onnx" || s == "onnxruntime") {
        *out = AlgLibOnnx;
        return true;
    }
    return false;
}

// 合法种类
bool algLibKindValid(int k)
{
    return k >= 0 && k < AlgLibKindMax;
}

}