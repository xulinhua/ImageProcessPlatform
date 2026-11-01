/**
 * @file VarStore.cpp
 * @brief VarStore 实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "VarStore.h"
#include <cctype>
#include <cstdlib>

namespace Vision {

VarStore::VarStore()
{}

VarStore::VarStore(const VarStore& other)
{
    std::lock_guard<std::mutex> lock(other.m_mu);
    m_vars = other.m_vars;
}

VarStore::VarStore(VarStore&& other) noexcept
{
    std::lock_guard<std::mutex> lock(other.m_mu);
    m_vars.swap(other.m_vars);
}

VarStore& VarStore::operator=(const VarStore& other)
{
    if (this == &other) {
        return *this;
    }
    std::vector<GlobalVar> copy;
    {
        std::lock_guard<std::mutex> lock(other.m_mu);
        copy = other.m_vars;
    }
    {
        std::lock_guard<std::mutex> lock(m_mu);
        m_vars.swap(copy);
    }
    return *this;
}

VarStore& VarStore::operator=(VarStore&& other) noexcept
{
    if (this == &other) {
        return *this;
    }
    std::vector<GlobalVar> moved;
    {
        std::lock_guard<std::mutex> lock(other.m_mu);
        moved.swap(other.m_vars);
    }
    {
        std::lock_guard<std::mutex> lock(m_mu);
        m_vars.swap(moved);
    }
    return *this;
}

VarStore::~VarStore()
{}

int VarStore::size() const
{
    std::lock_guard<std::mutex> lock(m_mu);
    return static_cast<int>(m_vars.size());
}

GlobalVar VarStore::at(int index) const
{
    std::lock_guard<std::mutex> lock(m_mu);
    if (index < 0 || index >= static_cast<int>(m_vars.size())) {
        return GlobalVar();
    }
    return m_vars[static_cast<size_t>(index)];
}

std::vector<GlobalVar> VarStore::items() const
{
    std::lock_guard<std::mutex> lock(m_mu);
    return m_vars;
}

void VarStore::setItems(const std::vector<GlobalVar>& items)
{
    std::lock_guard<std::mutex> lock(m_mu);
    m_vars = items;
}

void VarStore::clear()
{
    std::lock_guard<std::mutex> lock(m_mu);
    m_vars.clear();
}

int VarStore::findIndexUnlocked(const std::string& name) const
{
    for (size_t i = 0; i < m_vars.size(); ++i) {
        if (m_vars[i].name == name) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int VarStore::findIndex(const std::string& name) const
{
    std::lock_guard<std::mutex> lock(m_mu);
    return findIndexUnlocked(name);
}

bool VarStore::contains(const std::string& name) const
{
    return findIndex(name) >= 0;
}

bool VarStore::add(const GlobalVar& v)
{
    if (v.name.empty()) {
        return false;
    }
    std::lock_guard<std::mutex> lock(m_mu);
    if (findIndexUnlocked(v.name) >= 0) {
        return false;
    }
    m_vars.push_back(v);
    return true;
}

bool VarStore::remove(const std::string& name)
{
    std::lock_guard<std::mutex> lock(m_mu);
    const int idx = findIndexUnlocked(name);
    if (idx < 0) {
        return false;
    }
    m_vars.erase(m_vars.begin() + idx);
    return true;
}

bool VarStore::removeAt(int index)
{
    std::lock_guard<std::mutex> lock(m_mu);
    if (index < 0 || index >= static_cast<int>(m_vars.size())) {
        return false;
    }
    m_vars.erase(m_vars.begin() + index);
    return true;
}

ParamValue VarStore::get(const std::string& name) const
{
    std::lock_guard<std::mutex> lock(m_mu);
    const int idx = findIndexUnlocked(name);
    if (idx < 0) {
        return ParamValue();
    }
    return m_vars[static_cast<size_t>(idx)].value;
}

std::string VarStore::getString(const std::string& name, const std::string& def) const
{
    const ParamValue v = get(name);
    if (v.isNull()) {
        return def;
    }
    return v.toString();
}

bool VarStore::setUnlocked(const std::string& name, const ParamValue& value)
{
    const int idx = findIndexUnlocked(name);
    if (idx < 0) {
        return false;
    }
    GlobalVar& g = m_vars[static_cast<size_t>(idx)];
    g.value = valueFromText(g.type, value.toString());
    return true;
}

bool VarStore::set(const std::string& name, const ParamValue& value)
{
    std::lock_guard<std::mutex> lock(m_mu);
    return setUnlocked(name, value);
}

bool VarStore::setFromString(const std::string& name, const std::string& text)
{
    std::lock_guard<std::mutex> lock(m_mu);
    const int idx = findIndexUnlocked(name);
    if (idx < 0) {
        return false;
    }
    GlobalVar& g = m_vars[static_cast<size_t>(idx)];
    g.value = valueFromText(g.type, text);
    return true;
}

ParamValue VarStore::valueFromText(GlobalVarType type, const std::string& text)
{
    switch (type) {
    case GlobalVarInt:
        return ParamValue(atoi(text.c_str()));
    case GlobalVarFloat:
        return ParamValue(atof(text.c_str()));
    case GlobalVarBool: {
        if (text == "1" || text == "true" || text == "True" || text == "TRUE") {
            return ParamValue(true);
        }
        if (text == "0" || text == "false" || text == "False" || text == "FALSE") {
            return ParamValue(false);
        }
        return ParamValue(atoi(text.c_str()) != 0);
    }
    case GlobalVarString:
    default:
        return ParamValue(text);
    }
}

std::string VarStore::globalNameFromPath(const std::string& path)
{
    const std::string prefix = "global:";
    if (path.size() <= prefix.size()) {
        return std::string();
    }
    // 容忍 Global: 大小写
    bool match = true;
    for (size_t i = 0; i < prefix.size(); ++i) {
        const char a = static_cast<char>(::tolower(static_cast<unsigned char>(path[i])));
        const char b = prefix[i];
        if (a != b) {
            match = false;
            break;
        }
    }
    if (!match) {
        return std::string();
    }
    return path.substr(prefix.size());
}

bool VarStore::applyCommLine(const std::string& line)
{
    // 去首尾空白
    size_t begin = 0;
    while (begin < line.size() && (line[begin] == ' ' || line[begin] == '\t' || line[begin] == '\r'
                                   || line[begin] == '\n')) {
        ++begin;
    }
    size_t end = line.size();
    while (end > begin && (line[end - 1] == ' ' || line[end - 1] == '\t' || line[end - 1] == '\r'
                           || line[end - 1] == '\n')) {
        --end;
    }
    if (end <= begin) {
        return false;
    }
    const std::string trimmed = line.substr(begin, end - begin);

    // 前缀 SetGlobalValue:（大小写不敏感）
    const std::string tag = "SetGlobalValue:";
    if (trimmed.size() < tag.size()) {
        return false;
    }
    for (size_t i = 0; i < tag.size(); ++i) {
        const char a = static_cast<char>(::tolower(static_cast<unsigned char>(trimmed[i])));
        const char b = static_cast<char>(::tolower(static_cast<unsigned char>(tag[i])));
        if (a != b) {
            return false;
        }
    }
    std::string body = trimmed.substr(tag.size());
    // 容忍 SetGlobalValue name=value（无冒号时已在上面失败；也容忍 name = value）
    size_t eq = body.find('=');
    if (eq == std::string::npos) {
        return false;
    }
    std::string name = body.substr(0, eq);
    std::string val = body.substr(eq + 1);
    // 修剪 name / val 两端空格
    while (!name.empty() && (name[0] == ' ' || name[0] == '\t')) {
        name.erase(name.begin());
    }
    while (!name.empty() && (name[name.size() - 1] == ' ' || name[name.size() - 1] == '\t')) {
        name.erase(name.end() - 1);
    }
    while (!val.empty() && (val[0] == ' ' || val[0] == '\t')) {
        val.erase(val.begin());
    }
    if (name.empty()) {
        return false;
    }
    return setFromString(name, val);
}

void VarStore::applySubscribeBinds(const std::vector<ParamBind>& binds, ParamMap* run) const
{
    if (!run) {
        return;
    }
    for (size_t i = 0; i < binds.size(); ++i) {
        const ParamBind& b = binds[i];
        if (b.direction != 0 || b.paramKey.empty()) {
            continue;
        }
        const std::string varName = globalNameFromPath(b.path);
        if (varName.empty()) {
            continue;
        }
        const ParamValue v = get(varName);
        if (v.isNull()) {
            continue;
        }
        run->insert(b.paramKey, v);
    }
}

void VarStore::applyWriteBinds(const std::vector<ParamBind>& binds, const ParamMap& run,
                               const ParamMap& outputValues)
{
    for (size_t i = 0; i < binds.size(); ++i) {
        const ParamBind& b = binds[i];
        if (b.direction != 1 || b.paramKey.empty()) {
            continue;
        }
        const std::string varName = globalNameFromPath(b.path);
        if (varName.empty()) {
            continue;
        }
        ParamValue v;
        if (outputValues.contains(b.paramKey)) {
            v = outputValues.value(b.paramKey);
        } else if (run.contains(b.paramKey)) {
            v = run.value(b.paramKey);
        } else {
            continue;
        }
        set(varName, v);
    }
}

}
