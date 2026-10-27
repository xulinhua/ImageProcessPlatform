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

/** @brief UTF-8 to system ANSI (GBK on Chinese Windows) */
VISION_COMMON_API std::string utf8ToAcp(const std::string& utf8);
/** @brief System ANSI to UTF-8 (for JSON on disk) */
VISION_COMMON_API std::string acpToUtf8(const std::string& acp);
/** @brief Local path (ACP) to UTF-16 for CreateFileW / _wfopen */
VISION_COMMON_API std::wstring acpToWide(const std::string& acp);
/** @brief UTF-16 to local path (ACP) */
VISION_COMMON_API std::string wideToAcp(const wchar_t* wide);
/** @brief UTF-8路径转宽字符，供 CreateFileW / _wfopen */
VISION_COMMON_API std::wstring utf8ToWide(const std::string& utf8);
/** @brief 宽字符路径转 UTF-8 */
VISION_COMMON_API std::string wideToUtf8(const wchar_t* wide);

}

#endif
