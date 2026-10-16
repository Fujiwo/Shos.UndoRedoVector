/* MemoryLeakTest.h

Detects memory leaks with the debug heap of the Microsoft C runtime (MSVC only).
In debug builds, every allocation made with new records its source file and line,
and the leaked blocks are reported to the debugger output when the program exits.
In release builds, this header has no effect.

Usage:

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
// Makes malloc / free and friends record the source file and line of each allocation.
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif // _DEBUG

// DEBUG_NEW may already be defined (e.g. by MFC); then that definition is used.
#ifndef DEBUG_NEW

#ifdef _DEBUG
// The placement form of operator new provided by crtdbg.h, which records the source file and line.
#define DEBUG_NEW ::new(_NORMAL_BLOCK, __FILE__, __LINE__)
#else // _DEBUG
#define DEBUG_NEW new
#endif // _DEBUG

#endif // DEBUG_NEW

// Replaces every following new expression with DEBUG_NEW.
#define new DEBUG_NEW

// Call at the beginning of the application.
// Makes the C runtime check for leaks automatically when the program exits.
inline void CheckMemoryLeaksStart()
{
#ifdef _DEBUG
    // Adds _CRTDBG_LEAK_CHECK_DF to the current debug heap flags.
    _CrtSetDbgFlag(_CrtSetDbgFlag(_CRTDBG_REPORT_FLAG) | _CRTDBG_LEAK_CHECK_DF);
#endif // _DEBUG
}
