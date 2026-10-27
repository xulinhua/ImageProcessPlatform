#ifndef HSCOMMDATA_HSCOMMDATAEXPORT_H
#define HSCOMMDATA_HSCOMMDATAEXPORT_H

/**
 * @file HsCommDataExport.h
 * @brief DLL export macro for HsCommData.
 * Leaf POD headers include this only. Throwing APIs also include HsException.h.
 */

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
