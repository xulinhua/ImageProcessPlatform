/**
 * @file VisionCfgCryptExport.h
 * @brief 配置加密库导出
 */

#ifndef VISION_CFGCRYPT_EXPORT_H
#define VISION_CFGCRYPT_EXPORT_H

#ifdef VISION_CFGCRYPT_EXPORTS
#define VISION_CFGCRYPT_API __declspec(dllexport)
#else
#define VISION_CFGCRYPT_API __declspec(dllimport)
#endif

#endif
