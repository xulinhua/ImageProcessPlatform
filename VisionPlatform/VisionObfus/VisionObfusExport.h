/**
 * @file VisionObfusExport.h
 * @brief 混淆库导出。业务工程本期不链接本库
 */

#ifndef VISION_OBFUS_EXPORT_H
#define VISION_OBFUS_EXPORT_H

#ifdef VISION_OBFUS_EXPORTS
#define VISION_OBFUS_API __declspec(dllexport)
#else
#define VISION_OBFUS_API __declspec(dllimport)
#endif

#endif
