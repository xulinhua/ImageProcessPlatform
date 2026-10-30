/**
 * @file HsCommDataExport.h
 * @brief HsCommData 的 DLL 导出宏。纯数据头文件只包含本文件，会抛异常的接口另含 HsException.h
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef HSCOMMDATA_HSCOMMDATAEXPORT_H
#define HSCOMMDATA_HSCOMMDATAEXPORT_H

#include <stddef.h>

#ifdef HSCOMMDATA_STATIC
#  define HSCOMMDATA_EXPORT
#else
#  ifdef _WIN32
#    ifdef HSCOMMDATA_LIB
#      define HSCOMMDATA_EXPORT __declspec(dllexport)
#    else
#      define HSCOMMDATA_EXPORT __declspec(dllimport)
#    endif
#  else
#    define HSCOMMDATA_EXPORT
#  endif
#endif

#endif
