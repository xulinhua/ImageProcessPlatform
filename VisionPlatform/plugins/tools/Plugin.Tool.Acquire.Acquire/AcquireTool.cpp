/**
 * @file AcquireTool.cpp
 * @brief AcquireTool实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "AcquireTool.h"
#include "FileGrabber.h"
#include "TextEncoding.h"
#include "VisionVersion.h"
#include <vector>

namespace {
// 当前版本。必须等于变更表最后一行
const char kToolVersion[] = "1.0.0.0";
// 变更表。只追加，不删旧行
const Vision::VerNoteRaw kVerNotes[] = {
    { "1.0.0.0", "首版" },
};

// 注册名
const char kToolName[] = "camera.acquire";
// 界面显示名
const char kDisplayName[] = "采集";
// 工具说明
const char kDescription[] = "从已配置的相机、文件夹或图像模块取图";

// 按换行拆开记录的图像路径
std::vector<std::string> splitLines(const std::string& packed)
{
    std::vector<std::string> out;
    std::string cur;
    for (size_t i = 0; i < packed.size(); ++i) {
        const char c = packed[i];
        if (c == '\n' || c == '\r') {
            if (!cur.empty()) {
                out.push_back(cur);
                cur.clear();
            }
        } else {
            cur += c;
        }
    }
    if (!cur.empty()) {
        out.push_back(cur);
    }
    return out;
}
}

// 注册名
std::string Vision::AcquireTool::toolName() const
{
    return kToolName;
}

// 工具大类
Vision::ToolType Vision::AcquireTool::type() const
{
    return Vision::ToolType::Acquire;
}

// 界面显示名
std::string Vision::AcquireTool::displayName() const
{
    return Vision::acpToUtf8(kDisplayName);
}

// 工具版本
std::string Vision::AcquireTool::version() const
{
    return std::string(kToolVersion);
}

// 版本变更，只追加
std::vector<Vision::VerNote> Vision::AcquireTool::versionNotes() const
{
    return Vision::verNotesFrom(kVerNotes, static_cast<int>(sizeof(kVerNotes) / sizeof(kVerNotes[0])));
}


// 工具说明
std::string Vision::AcquireTool::description() const
{
    return Vision::acpToUtf8(kDescription);
}

// 参数说明
std::vector<Vision::ParamField> Vision::AcquireTool::paramFields() const
{
        std::vector<Vision::ParamField> f;
        f.push_back(Vision::paramDevice("deviceId", "采集模块"));
        return f;
    }

// 按文件、图像列表或文件夹取图
Vision::ToolOut Vision::AcquireTool::execute(const Vision::ToolIn& in)
{
        Vision::ToolOut out;
        Vision::ParamMap runtime = in.runtime;
        const int64_t fid = static_cast<int64_t>(runtime.value("frameId").toDouble(1));
        const std::string mode = in.param.run.value("sourceMode").toString();
        std::string err;
        if (mode == "file") {
            const HsCommData::HsImage fb = Vision::FileGrabber::grab(in.param.run.value("path").toString(), "file", &runtime, fid, &err);
            out.runtime = runtime;
            if (fb.isNull()) {
                out.message = err;
                return out;
            }
            out.ok = true;
            out.image = fb;
            out.frameId = fb.frameId;
            out.message = runtime.value("lastFile").toString();
            return out;
        }
        const std::string st = in.param.run.value("deviceStatus").toString();
        if (st == "missing" || st.empty()) {
            out.message = Vision::acpToUtf8("未绑定采集模块");
            return out;
        }
        if (st == "not_acquire" || st == "not_camera") {
            out.message = Vision::acpToUtf8("绑定的不是采集模块");
            return out;
        }
        if (st == "no_sdk") {
            out.message = Vision::acpToUtf8("相机 SDK 未接入");
            return out;
        }
        if (st == "image") {
            const std::vector<std::string> files = splitLines(in.param.run.value("images").toString());
            if (files.empty()) {
                out.message = Vision::acpToUtf8("图像模块没有记录图像路径");
                return out;
            }
            int idx = static_cast<int>(runtime.value("index").toDouble(0));
            if (idx < 0 || idx >= static_cast<int>(files.size())) {
                idx = 0;
            }
            const HsCommData::HsImage fb = Vision::FileGrabber::grab(files[static_cast<size_t>(idx)], "file", &runtime, fid, &err);
            out.runtime = runtime;
            if (fb.isNull()) {
                out.message = err.empty() ? Vision::acpToUtf8("没有可用图像") : err;
                return out;
            }
            out.runtime.insert("index", static_cast<double>((idx + 1) % static_cast<int>(files.size())));
            out.ok = true;
            out.image = fb;
            out.frameId = fb.frameId;
            out.message = runtime.value("lastFile").toString();
            return out;
        }
        const HsCommData::HsImage fb = Vision::FileGrabber::grab(in.param.run.value("path").toString(), "folder", &runtime, fid, &err);
        out.runtime = runtime;
        if (fb.isNull()) {
            out.message = err.empty() ? Vision::acpToUtf8("没有可用图像") : err;
            return out;
        }
        out.ok = true;
        out.image = fb;
        out.frameId = fb.frameId;
        out.message = runtime.value("lastFile").toString();
        return out;
    }

// 克隆本实例
Vision::ToolBase* Vision::AcquireTool::clone() const
{
    AcquireTool* p = new AcquireTool();
    p->copyInstanceDataFrom(*this);
    return p;
}

// 返回本工具参数对象

const Vision::ToolParaBase* Vision::AcquireTool::toolParam() const
{
    return &m_param;
}
