#pragma once

#define _HAS_STD_BYTE 0
#define _HAS_AUTO_PTR_ETC 1

#ifndef ACDB_PORT
#define ACDB_PORT
#endif
class AcGsGraphicsKernel;
class AcGsClassFactory;

#if defined(_DEBUG) && !defined(NC_FULL_DEBUG)
#define _DEBUG_WAS_DEFINED
#undef _DEBUG
#pragma message ("     Compiling header files in release mode.")
#endif

#pragma pack (push, 8)
#pragma warning(disable: 4786 4996)

#include <SDKDDKVer.h>

#define WIN32_LEAN_AND_MEAN

#if !defined(HOST_NO_MFC)
#define _AFXDLL
#include <afxwin.h>
#include <unknwn.h>
#else
#include <windows.h>
#endif
#include "arxHeaders.h"
#ifndef HELLONRX_MODULE
#define HELLONRX_MODULE
#endif
#pragma pack (pop)

#ifdef _DEBUG_WAS_DEFINED
#define _DEBUG
#undef _DEBUG_WAS_DEFINED
#endif
