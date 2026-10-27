#ifndef HSIMAGE_HSIMGCTRLEXPORT_H
#define HSIMAGE_HSIMGCTRLEXPORT_H

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4251)
#endif

/**
 * @file HsImgCtrlExport.h
 * @brief DLL export macro for HsImageCtrl.
 * Define HSCTRL_LIB when building the DLL. Define HSCTRL_STATIC for static link.
 * Protocol headers do not depend on Qt, so this does not use Q_DECL_EXPORT.
 */

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
