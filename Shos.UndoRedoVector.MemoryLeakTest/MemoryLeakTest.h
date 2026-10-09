/* Usage:

1. Include "MemoryLeakTest.h" in each source and header, after all the other headers
   (it redefines new, which must not be done before including the standard headers).
2. Call CheckMemoryLeaksStart() at the beginning of main().

// Main.cpp
#include <iostream>
#include "Sub.h"
#include "MemoryLeakTest.h"

int main()
{
    CheckMemoryLeaksStart();
    // ...
    Sub();
    // ...
    return 0;
}

// Sub.h
int Sub();

// Sub.cpp
#include <iostream>
#include "Sub.h"
#include "MemoryLeakTest.h"
int Sub()
{
    // ...
}
*/

#pragma once

#ifdef _DEBUG
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif // _DEBUG

#ifndef DEBUG_NEW

#ifdef _DEBUG
#define DEBUG_NEW ::new(_NORMAL_BLOCK, __FILE__, __LINE__)
#else // _DEBUG
#define DEBUG_NEW new
#endif // _DEBUG

#endif // DEBUG_NEW

#define new DEBUG_NEW

// call at begining of the application
inline void CheckMemoryLeaksStart()
{
#ifdef _DEBUG
    _CrtSetDbgFlag(_CrtSetDbgFlag(_CRTDBG_REPORT_FLAG) | _CRTDBG_LEAK_CHECK_DF);
#endif // _DEBUG
}
