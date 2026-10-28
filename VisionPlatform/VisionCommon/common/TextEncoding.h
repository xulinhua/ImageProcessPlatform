/**
 * @file TextEncoding.h
 * @brief TextEncoding头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_TEXTENCODING_H
#define VISION_TEXTENCODING_H
#include "VisionCommonExport.h"

#include <string>

namespace Vision {

/** @brief UTF-8路径转宽字符，供 CreateFileW / _wfopen */
VISION_COMMON_API std::wstring utf8ToWide(const std::string& utf8);
/** @brief 宽字符路径转 UTF-8 */
VISION_COMMON_API std::string wideToUtf8(const wchar_t* wide);

}

#endif
