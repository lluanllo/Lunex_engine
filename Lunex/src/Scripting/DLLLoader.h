#pragma once

/**
 * @file DLLLoader.h
 * @brief AAA Architecture: Cross-platform dynamic library loader
 *
 * Provides safe loading/unloading of DLLs with error handling
 * and function pointer retrieval.
 */

#include <string>
#include <functional>

#ifdef _WIN32
#include <Windows.h>
using ModuleHandle = HMODULE;
#else
#include <dlfcn.h>