/**
 * @file ParamKeys.cpp
 * @brief ParamKeys实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "ParamKeys.h"
#include "VpUtil.h"

namespace Vision {

// ValueKey 序列化名
const char* valueKeyName(ValueKey k)
{
    switch (k) {
    case ValueKey::X:     return "x";
    case ValueKey::Y:     return "y";
    case ValueKey::Cx:    return "cx";
    case ValueKey::Cy:    return "cy";
    case ValueKey::Angle: return "angle";
    case ValueKey::Score: return "score";
    case ValueKey::Ok:    return "ok";
    case ValueKey::Pass:  return "pass";
    case ValueKey::R:     return "r";
    default:              return "x";
    }
}

// 从序列化名解析 ValueKey
bool valueKeyFromName(const std::string& s, ValueKey* out)
{
    if (!out) {
        return false;
    }
    const std::string n = vpLower(s);
    if (n == "x") { *out = ValueKey::X; return true; }
    if (n == "y") { *out = ValueKey::Y; return true; }
    if (n == "cx") { *out = ValueKey::Cx; return true; }
    if (n == "cy") { *out = ValueKey::Cy; return true; }
    if (n == "angle") { *out = ValueKey::Angle; return true; }
    if (n == "score") { *out = ValueKey::Score; return true; }
    if (n == "ok") { *out = ValueKey::Ok; return true; }
    if (n == "pass") { *out = ValueKey::Pass; return true; }
    if (n == "r") { *out = ValueKey::R; return true; }
    return false;
}

// RuntimeKey 序列化名
const char* runtimeKeyName(RuntimeKey k)
{
    switch (k) {
    case RuntimeKey::Index:       return "index";
    case RuntimeKey::LastFile:    return "lastFile";
    case RuntimeKey::GrabPath:    return "grabPath";
    case RuntimeKey::LastX:       return "lastX";
    case RuntimeKey::LastY:       return "lastY";
    case RuntimeKey::LastA:       return "lastA";
    case RuntimeKey::LastScore:   return "lastScore";
    case RuntimeKey::PathChanged: return "pathChanged";
    default:                      return "index";
    }
}

// 从序列化名解析 RuntimeKey
bool runtimeKeyFromName(const std::string& s, RuntimeKey* out)
{
    if (!out) {
        return false;
    }
    const std::string n = vpLower(s);
    if (n == "index") { *out = RuntimeKey::Index; return true; }
    if (n == "lastfile") { *out = RuntimeKey::LastFile; return true; }
    if (n == "grabpath") { *out = RuntimeKey::GrabPath; return true; }
    if (n == "lastx") { *out = RuntimeKey::LastX; return true; }
    if (n == "lasty") { *out = RuntimeKey::LastY; return true; }
    if (n == "lasta") { *out = RuntimeKey::LastA; return true; }
    if (n == "lastscore") { *out = RuntimeKey::LastScore; return true; }
    if (n == "pathchanged") { *out = RuntimeKey::PathChanged; return true; }
    return false;
}

// TeachKey 序列化名
const char* teachKeyName(TeachKey k)
{
    switch (k) {
    case TeachKey::RefX: return "refX";
    case TeachKey::RefY: return "refY";
    case TeachKey::RefA: return "refA";
    default:             return "refX";
    }
}

// 从序列化名解析 TeachKey
bool teachKeyFromName(const std::string& s, TeachKey* out)
{
    if (!out) {
        return false;
    }
    const std::string n = vpLower(s);
    if (n == "refx") { *out = TeachKey::RefX; return true; }
    if (n == "refy") { *out = TeachKey::RefY; return true; }
    if (n == "refa") { *out = TeachKey::RefA; return true; }
    return false;
}

// ParamMap 是否含 ValueKey
bool paramHas(const ParamMap& m, ValueKey k)
{
    return m.contains(valueKeyName(k));
}

// 读取 ValueKey
ParamValue paramGet(const ParamMap& m, ValueKey k, const ParamValue& def)
{
    const std::string key = valueKeyName(k);
    if (!m.contains(key)) {
        return def;
    }
    return m.value(key);
}

// 写入 ValueKey
void paramSet(ParamMap* m, ValueKey k, const ParamValue& v)
{
    if (!m) {
        return;
    }
    m->insert(valueKeyName(k), v);
}

// ParamMap 是否含 RuntimeKey
bool runtimeHas(const ParamMap& m, RuntimeKey k)
{
    return m.contains(runtimeKeyName(k));
}

// 读取 RuntimeKey
ParamValue runtimeGet(const ParamMap& m, RuntimeKey k, const ParamValue& def)
{
    const std::string key = runtimeKeyName(k);
    if (!m.contains(key)) {
        return def;
    }
    return m.value(key);
}

// 写入 RuntimeKey
void runtimeSet(ParamMap* m, RuntimeKey k, const ParamValue& v)
{
    if (!m) {
        return;
    }
    m->insert(runtimeKeyName(k), v);
}

// ParamMap 是否含 TeachKey
bool teachHas(const ParamMap& m, TeachKey k)
{
    return m.contains(teachKeyName(k));
}

// 读取 TeachKey
ParamValue teachGet(const ParamMap& m, TeachKey k, const ParamValue& def)
{
    const std::string key = teachKeyName(k);
    if (!m.contains(key)) {
        return def;
    }
    return m.value(key);
}

// 写入 TeachKey
void teachSet(ParamMap* m, TeachKey k, const ParamValue& v)
{
    if (!m) {
        return;
    }
    m->insert(teachKeyName(k), v);
}

}
