/**
 * @file JudgeTool.cpp
 * @brief JudgeTool实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "JudgeTool.h"
#include "ParamKeys.h"
#include "TextEncoding.h"
#include "VisionVersion.h"
#include <cmath>
#include <cstdio>
#include <map>

namespace {
// 当前版本。必须等于变更表最后一行
const char kToolVersion[] = "1.0.0.0";
// 变更表。只追加，不删旧行
const Vision::VerNoteRaw kVerNotes[] = {
    { "1.0.0.0", "首版" },
};

// 注册名
const char kToolName[] = "logic.judge";
// 界面显示名
const char kDisplayName[] = "判定";
// 工具说明
const char kDescription[] = "汇总上游 OK/NG，或按引用字段（score/area/count…）做数值判定";

// 一条数值判定条件
struct JudgeTerm {
    std::string srcId;
    std::string key;
    std::string op;
    double value;
    JudgeTerm() : value(0.0) {}
};

// 读取第 i 条数值条件
bool loadTerm(const Vision::ParamMap& p, int i, JudgeTerm* out)
{
    if (!out) {
        return false;
    }
    char idKey[32];
    char keyKey[32];
    char opKey[32];
    char valKey[32];
    sprintf_s(idKey, "term%d_id", i);
    sprintf_s(keyKey, "term%d_key", i);
    sprintf_s(opKey, "term%d_op", i);
    sprintf_s(valKey, "term%d_val", i);
    out->srcId = p.value(idKey).toString();
    if (out->srcId.empty()) {
        return false;
    }
    out->key = p.value(keyKey).toString();
    if (out->key.empty()) {
        out->key = "score";
    }
    out->op = p.value(opKey).toString();
    if (out->op.empty()) {
        out->op = ">=";
    }
    out->value = p.value(valKey).toDouble(0.0);
    return true;
}

// 从上游结果读取数值字段
bool readField(const Vision::ParamMap& v, const std::string& key, double* out)
{
    if (!out) {
        return false;
    }
    if (v.contains(key)) {
        *out = v.value(key).toDouble();
        return true;
    }
    Vision::ValueKey vk;
    if (Vision::valueKeyFromName(key, &vk) && Vision::paramHas(v, vk)) {
        *out = Vision::paramGet(v, vk).toDouble();
        return true;
    }
    if (key == "ok" || key == "pass") {
        if (Vision::paramHas(v, Vision::ValueKey::Ok)) {
            *out = Vision::paramGet(v, Vision::ValueKey::Ok).toBool() ? 1.0 : 0.0;
            return true;
        }
        if (Vision::paramHas(v, Vision::ValueKey::Pass)) {
            *out = Vision::paramGet(v, Vision::ValueKey::Pass).toBool() ? 1.0 : 0.0;
            return true;
        }
    }
    return false;
}

// 按运算符比较左右值
bool compareOp(double lhs, const std::string& op, double rhs)
{
    if (op == ">=") {
        return lhs + 1e-9 >= rhs;
    }
    if (op == "<=") {
        return lhs <= rhs + 1e-9;
    }
    if (op == ">") {
        return lhs > rhs;
    }
    if (op == "<") {
        return lhs < rhs;
    }
    if (op == "!=") {
        return std::fabs(lhs - rhs) > 1e-9;
    }
    return std::fabs(lhs - rhs) <= 1e-9;
}
}

// 注册名
std::string Vision::JudgeTool::toolName() const
{
    return kToolName;
}

// 工具大类
Vision::ToolType Vision::JudgeTool::type() const
{
    return Vision::ToolType::Logic;
}

// 界面显示名
std::string Vision::JudgeTool::displayName() const
{
    return kDisplayName;
}

// 工具版本
std::string Vision::JudgeTool::version() const
{
    return std::string(kToolVersion);
}

// 版本变更，只追加
std::vector<Vision::VerNote> Vision::JudgeTool::versionNotes() const
{
    return Vision::verNotesFrom(kVerNotes, static_cast<int>(sizeof(kVerNotes) / sizeof(kVerNotes[0])));
}


// 工具说明
std::string Vision::JudgeTool::description() const
{
    return kDescription;
}

// 缺省参数
Vision::ParamMap Vision::JudgeTool::defaultParams() const
{
        Vision::ParamMap p;
        p.insert("useOkAnd", true);
        p.insert("termCount", 0);
        return p;
    }

// 参数说明
std::vector<Vision::ParamField> Vision::JudgeTool::paramFields() const
{
        std::vector<Vision::ParamField> f;
        f.push_back(Vision::paramBool("useOkAnd", "汇总上游OK", true));
        f.push_back(Vision::paramInt("termCount", "数值条件数", 0, true, 0, true, 16));
        return f;
    }

// 汇总上游结果并按数值条件判定
Vision::ToolOut Vision::JudgeTool::execute(const Vision::ToolIn& in)
{
        Vision::ToolOut out;
        bool ok = true;
        std::string parts;
        const bool useOkAnd = in.param.run.value("useOkAnd").toBool(true);
        if (useOkAnd) {
            for (int i = 0; i < static_cast<int>(in.inputIo.logics.size()); ++i) {
                const Vision::PortLogic& L = in.inputIo.logics[static_cast<size_t>(i)];
                if (!L.valid) {
                    continue;
                }
                ok = ok && L.ok;
                if (!parts.empty()) {
                    parts.push_back(' ');
                }
                if (!L.message.empty()) {
                    parts += L.message;
                } else {
                    parts += L.ok ? "OK" : "NG";
                }
            }
        }
        const int termCount = in.param.run.value("termCount").toInt(0);
        for (int i = 0; i < termCount; ++i) {
            JudgeTerm term;
            if (!loadTerm(in.param.run, i, &term)) {
                continue;
            }
            std::map<std::string, std::string>::const_iterator nameIt = in.refNames.find(term.srcId);
            const std::string srcName = nameIt != in.refNames.end() ? nameIt->second : term.srcId;
            double lhs = 0.0;
            bool has = false;
            std::map<std::string, Vision::ParamMap>::const_iterator it = in.refValues.find(term.srcId);
            if (it != in.refValues.end()) {
                has = readField(it->second, term.key, &lhs);
            }
            if (!has && in.upstream.contains(term.key)) {
                lhs = in.upstream.value(term.key).toDouble();
                has = true;
            }
            const bool pass = has && compareOp(lhs, term.op, term.value);
            ok = ok && pass;
            if (!parts.empty()) {
                parts.push_back(' ');
            }
            char buf[160];
            if (has) {
                sprintf_s(buf, "%s.%s%s%.4g%s", srcName.c_str(), term.key.c_str(), term.op.c_str(),
                          term.value, pass ? "=OK" : "=NG");
            } else {
                sprintf_s(buf, "%s.%s(无数据)=NG", srcName.c_str(), term.key.c_str());
            }
            parts += buf;
        }
        out.ok = ok;
        Vision::paramSet(&out.values, Vision::ValueKey::Ok, ok);
        Vision::PortLogic L;
        L.ok = ok;
        L.valid = true;
        L.message = parts.empty() ? "无判定条件" : parts;
        out.io.logics.push_back(L);
        out.message = L.message;
        out.image = in.input;
        return out;
    }

// 克隆本实例
Vision::ToolBase* Vision::JudgeTool::clone() const
{
    JudgeTool* p = new JudgeTool();
    p->copyInstanceDataFrom(*this);
    return p;
}

// 返回本工具参数对象

const Vision::ToolParaBase* Vision::JudgeTool::toolParam() const
{
    return &m_param;
}
