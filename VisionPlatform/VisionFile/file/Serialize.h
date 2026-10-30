/**
 * @file Serialize.h
 * @brief ParamMap / ParamValue / JsonValue 的序列化与反序列化
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_SERIALIZE_H
#define VISION_SERIALIZE_H
#include "VisionFileExport.h"

#include "Json.h"
#include "ParamValue.h"
#include <string>

namespace Vision {

/**
 * @class Serialize
 * @brief 参数表与 JSON 互转，以及读写 UTF-8 文件。无 Qt
 */
class VISION_FILE_API Serialize {
public:
    /** @brief ParamValue → JsonValue；空值写 null */
    static JsonValue valueToJson(const ParamValue& v);
    /** @brief JsonValue → ParamValue；null/未知种类得到空值 */
    static ParamValue valueFromJson(const JsonValue& j);

    /** @brief ParamMap → JSON 对象 */
    static JsonValue mapToJson(const ParamMap& m);
    /** @brief JSON 对象 → ParamMap；非对象返回空表 */
    static ParamMap mapFromJson(const JsonValue& j);

    /** @brief ParamMap 写成 JSON 文本 */
    static std::string toText(const ParamMap& m, bool pretty = true);
    /**
     * @brief JSON 文本还原 ParamMap
     * @param out 成功时覆盖写出
     */
    static bool fromText(const std::string& text, ParamMap* out, std::string* error);

    /**
     * @brief ParamMap 原子写入 JSON 文件（UTF-8，无 BOM）
     */
    static bool saveMap(const std::string& path, const ParamMap& m, bool pretty, std::string* error);
    /**
     * @brief 从 JSON 文件读入 ParamMap
     */
    static bool loadMap(const std::string& path, ParamMap* out, std::string* error);

    /**
     * @brief JsonValue 原子写入 JSON 文件（UTF-8，无 BOM）
     */
    static bool saveJson(const std::string& path, const JsonValue& v, bool pretty, std::string* error);
    /**
     * @brief 从 JSON 文件读入 JsonValue
     */
    static bool loadJson(const std::string& path, JsonValue* out, std::string* error);

private:
    /** @brief 读整个文件为字节；失败写 error */
    static bool readFile(const std::string& path, std::string* bytes, std::string* error);
};

}

#endif
