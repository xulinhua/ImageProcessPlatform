/**
 * @file HsImgCtrlExport.h
 * @brief HsImageCtrl 的 DLL 导出宏。编 DLL 时定义 HSCTRL_LIB，静态链接时定义 HSCTRL_STATIC；
 *        协议头不依赖 Qt，所以不用 Q_DECL_EXPORT
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef HSIMAGE_HSIMGCTRLEXPORT_H
#define HSIMAGE_HSIMGCTRLEXPORT_H

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4251)
#endif


#ifdef HSCTRL_STATIC
#  define HSCTRL_EXPORT
#else
#  ifdef _WIN32
#    ifdef HSCTRL_LIB
#      define HSCTRL_EXPORT __declspec(dllexport)
#    else
#      define HSCTRL_EXPORT __declspec(dllimport)
#    endif
#  else
#    define HSCTRL_EXPORT
#  endif
#endif

#endif
