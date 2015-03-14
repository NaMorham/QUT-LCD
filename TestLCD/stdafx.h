// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//
#ifndef __AMH__TESTLCD__STDAFX_H__
#define __AMH__TESTLCD__STDAFX_H__

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files:
#include <windows.h>

// C RunTime Header Files
#include <stdlib.h>
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>


// TODO: reference additional headers your program requires here
#ifndef byte
typedef unsigned char byte;
#endif

const size_t LONG_BUF_SIZE = 1024;
const size_t BUF_SIZE = 256;
const size_t SHORT_BUF_SIZE = 64;

#endif // __AMH__TESTLCD__STDAFX_H__
