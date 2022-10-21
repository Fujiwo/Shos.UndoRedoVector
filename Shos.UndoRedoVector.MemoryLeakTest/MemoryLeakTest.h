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

/* Usage:
 
// Main.cpp
#include <iostream>
// Include "MemoryLeakTest.h" in each source and header.
#include "MemoryLeakTest.h"
#include "Sub.h"
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
#include "MemoryLeakTest.h"
#include "Sub.h"
int Sub()
{
    // ...
}
*/