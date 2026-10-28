/**
 * @file Translate.h
 * @brief Translate头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_TRANSLATE_H
#define VISION_TRANSLATE_H
#include "VisionLangExport.h"

#include <string>
#include <vector>

namespace Vision {

/** @brief 界面语言。词条只存在本工程，其它工程禁止写用户可见中文 */
enum LangId {
    LangZhCN = 0, ///< 简体中文（源语言）
    LangEnUS      ///< English
};

/**
 * @class Translate
 * @brief 翻译入口。键为源语言（简体中文）字面量；英文在本工程词条表
 */
class VISION_LANGUAGE_API Translate {
public:
    /** @brief 切换当前语言（立即影响后续 tr） */
    static void setLanguage(LangId lang);
    /**
     * @brief 按语言键切换。认识 zh-CN、zh、en-US、en
     * @return 是否为已支持的键
     */
    static bool setLanguageKey(const std::string& key);
    /** @brief 当前语言 */
    static LangId language();
    /** @brief 存盘用键，如 zh-CN */
    static std::string languageKey();
    /** @brief 语言种数 */
    static int languageCount();
    /** @brief 第 i 种语言；越界回 zh-CN。列表来自 lang\\languages.txt，缺文件时只有中英 */
    static LangId languageAt(int i);
    /** @brief 第 i 种语言的存盘键；越界空串 */
    static std::string languageKeyAt(int i);
    /** @brief 存盘键 */
    static std::string languageKeyOf(LangId lang);
    /** @brief 语言本名（不随界面语言变：简体中文 或 English） */
    static std::string languageNativeName(LangId lang);
    /** @brief 第 i 种语言的本名；越界空串 */
    static std::string languageNativeNameAt(int i);

    /**
     * @brief 取当前语言文本
     * @param key 源语言字面量（中文或专业英文）。zh-CN 原样返回；其它语言查表，缺条回 key
     */
    static std::string tr(const char* key);

    /**
     * @brief 取译文并把 {0}、{1} 换成参数。参数在调用方先转成字符串
     * @param key 源语言句子，占位从 0 编号
     */
    static std::string format(const char* key, const std::vector<std::string>& args);
    /** @brief 用一个参数替换 {0} */
    static std::string format(const char* key, const std::string& a0);
    /** @brief 用两个参数替换 {0}、{1} */
    static std::string format(const char* key, const std::string& a0, const std::string& a1);
    /** @brief 用三个参数替换 {0}、{1}、{2} */
    static std::string format(const char* key, const std::string& a0, const std::string& a1, const std::string& a2);
};

}

#endif
