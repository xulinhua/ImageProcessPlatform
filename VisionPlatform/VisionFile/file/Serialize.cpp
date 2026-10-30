/**
 * @file Serialize.cpp
 * @brief Serialize 实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "Serialize.h"
#include "FileDocument.h"
#include "TextEncoding.h"
#include <cstdio>

namespace Vision {

/************************************************************
 * ParamValue ↔ JsonValue
 ************************************************************/

JsonValue Serialize::valueToJson(const ParamValue& v)
{
    switch (v.kind()) {
    case ParamValue::KindBool:
        return JsonValue(v.toBool());
    case ParamValue::KindInt:
        return JsonValue(v.toInt());
    case ParamValue::KindDouble:
        return JsonValue(v.toDouble());
    case ParamValue::KindString:
        return JsonValue(v.toString());
    case ParamValue::KindNull:
    default:
        return JsonValue();
    }
}

ParamValue Serialize::valueFromJson(const JsonValue& j)
{
    if (j.isBool()) {
        return ParamValue(j.toBool());
    }
    if (j.isNumber()) {
        const double d = j.toDouble();
        if (d == static_cast<double>(static_cast<int>(d))) {
            return ParamValue(j.toInt());
        }
        return ParamValue(d);
    }
    if (j.isString()) {
        return ParamValue(j.toString());
    }
    return ParamValue();
}

/************************************************************
 * ParamMap ↔ JsonValue
 ************************************************************/

JsonValue Serialize::mapToJson(const ParamMap& m)
{
    JsonValue o;
    o.makeObject();
    const std::vector<std::string> keys = m.keys();
    for (size_t i = 0; i < keys.size(); ++i) {
        const ParamValue v = m.value(keys[i]);
        if (v.isNull()) {
            continue;
        }
        o.set(keys[i], valueToJson(v));
    }
    return o;
}

ParamMap Serialize::mapFromJson(const JsonValue& j)
{
    ParamMap p;
    if (!j.isObject()) {
        return p;
    }
    const std::map<std::string, JsonValue>& obj = j.object();
    for (std::map<std::string, JsonValue>::const_iterator it = obj.begin(); it != obj.end(); ++it) {
        const ParamValue v = valueFromJson(it->second);
        if (!v.isNull()) {
            p.insert(it->first, v);
        }
    }
    return p;
}

std::string Serialize::toText(const ParamMap& m, bool pretty)
{
    return JsonValue::stringify(mapToJson(m), pretty);
}

bool Serialize::fromText(const std::string& text, ParamMap* out, std::string* error)
{
    if (!out) {
        if (error) {
            *error = "null out";
        }
        return false;
    }
    const JsonValue j = JsonValue::parse(text, error);
    if (!j.isObject()) {
        if (error && error->empty()) {
            *error = "root is not object";
        }
        return false;
    }
    *out = mapFromJson(j);
    return true;
}

/************************************************************
 * 文件读写
 ************************************************************/

bool Serialize::readFile(const std::string& path, std::string* bytes, std::string* error)
{
    if (!bytes) {
        if (error) {
            *error = "null bytes";
        }
        return false;
    }
    FILE* f = _wfopen(utf8ToWide(path).c_str(), L"rb");
    if (!f) {
        if (error) {
            *error = "open failed";
        }
        return false;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        if (error) {
            *error = "seek failed";
        }
        return false;
    }
    const long n = ftell(f);
    if (n < 0) {
        fclose(f);
        if (error) {
            *error = "size failed";
        }
        return false;
    }
    rewind(f);
    bytes->assign(static_cast<size_t>(n), '\0');
    if (n > 0) {
        const size_t got = fread(&(*bytes)[0], 1, static_cast<size_t>(n), f);
        if (got != static_cast<size_t>(n)) {
            fclose(f);
            if (error) {
                *error = "read failed";
            }
            return false;
        }
    }
    fclose(f);
    // 去掉 UTF-8 BOM
    if (bytes->size() >= 3
        && static_cast<unsigned char>((*bytes)[0]) == 0xEF
        && static_cast<unsigned char>((*bytes)[1]) == 0xBB
        && static_cast<unsigned char>((*bytes)[2]) == 0xBF) {
        bytes->erase(0, 3);
    }
    return true;
}

bool Serialize::saveMap(const std::string& path, const ParamMap& m, bool pretty, std::string* error)
{
    return FileUtil::writeAtomic(path, toText(m, pretty), error);
}

bool Serialize::loadMap(const std::string& path, ParamMap* out, std::string* error)
{
    std::string text;
    if (!readFile(path, &text, error)) {
        return false;
    }
    return fromText(text, out, error);
}

bool Serialize::saveJson(const std::string& path, const JsonValue& v, bool pretty, std::string* error)
{
    return FileUtil::writeAtomic(path, JsonValue::stringify(v, pretty), error);
}

bool Serialize::loadJson(const std::string& path, JsonValue* out, std::string* error)
{
    if (!out) {
        if (error) {
            *error = "null out";
        }
        return false;
    }
    std::string text;
    if (!readFile(path, &text, error)) {
        return false;
    }
    *out = JsonValue::parse(text, error);
    if (out->isNull() && error && !error->empty()) {
        return false;
    }
    return true;
}

}
