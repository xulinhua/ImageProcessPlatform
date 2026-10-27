#pragma once
#define USE_OPENCV_480

#ifdef USE_OPENCV_480
#include "OpenCV/include/opencv.hpp" 
#include "OpenCV/include/opencv2/calib3d/calib3d.hpp"
#include "OpenCV/include/opencv2/core/core.hpp"
#include "OpenCV/include/opencv2/imgproc/imgproc.hpp"
#include "OpenCV/include/opencv2/highgui/highgui.hpp"
#include "OpenCV/include/opencv.hpp" 
#include "OpenCV/include/opencv2/calib3d/calib3d_c.h" 
#define _LIBPATH_(p,f)   p##f

#ifdef _DEBUG
#ifdef _M_X64
#pragma comment(lib,"hsvisbasall64d.lib")
#else
#pragma comment(lib,"hsvisbasalld.lib")
#endif 
#else
#ifdef _M_X64
#pragma comment(lib,"hsvisbasall64.lib")
#else
#pragma comment(lib,"hsvisbasall.lib")
#endif 
#endif
#endif

#ifdef USE_OPENCV_440
#include "OpenCV/include/opencv.hpp" 
#include "OpenCV/include/opencv2/calib3d/calib3d.hpp"
#include "OpenCV/include/opencv2/core/core.hpp"
#include "OpenCV/include/opencv2/imgproc/imgproc.hpp"
#include "OpenCV/include/opencv2/highgui/highgui.hpp"
#include "OpenCV/include/opencv.hpp" 
#include "OpenCV/include/opencv2/calib3d/calib3d_c.h" 
#define _LIBPATH_(p,f)   p##f

#ifdef _DEBUG
#ifdef _M_X64
//#pragma comment(lib, _LIBPATH_(__FILE__, "../../OpenCV/x64/lib/Debug/hsvisbasall64d.lib"))
#pragma comment(lib,"hsvisbasall64d.lib")
#else
//#pragma comment(lib, _LIBPATH_(__FILE__, "../../OpenCV/Win32/lib/Debug/hsvisbasalld.lib"))
#pragma comment(lib,"hsvisbasalld.lib")
#endif 
#else
#ifdef _M_X64
//#pragma comment(lib, _LIBPATH_(__FILE__, "../../OpenCV/x64/lib/Release/hsvisbasall64.lib"))
#pragma comment(lib,"hsvisbasall64.lib")
#else
//#pragma comment(lib, _LIBPATH_(__FILE__, "../../OpenCV/Win32/lib/Release/hsvisbasall.lib"))
#pragma comment(lib,"hsvisbasall.lib")
#endif 
#endif
#endif

#ifdef USE_OPENCV_246
#include "OpenCV246/include/opencv.hpp"
#include "OpenCV246/include/opencv2/core/core.hpp"
#include "OpenCV246/include/opencv2/objdetect/objdetect.hpp"
#include "OpenCV246/include/opencv2/highgui/highgui.hpp"
#include "OpenCV246/include/opencv2/imgproc/imgproc.hpp"
#ifdef _DEBUG
#ifdef _M_X64

#else
#pragma comment(lib,"opencv_core_d.lib")
#pragma comment(lib,"opencv_highgui_d.lib")
#pragma comment(lib,"opencv_imgproc_d.lib")
#pragma comment(lib,"opencv_objdetect_d.lib")
#endif 
#else
#ifdef _M_X64

#else
#pragma comment(lib,"opencv_core_r.lib")
#pragma comment(lib,"opencv_highgui_r.lib")
#pragma comment(lib,"opencv_imgproc_r.lib")
#pragma comment(lib,"opencv_objdetect_r.lib")
#endif 
#endif
#endif