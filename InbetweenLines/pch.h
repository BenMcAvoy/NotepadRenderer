#pragma once

#define WIN32_LEAN_AND_MEAN 1   // Exclude rarely-used stuff from Windows headers
#define SOL_ALL_SAFETIES_ON 1   // Enable all safety features for sol
#define SOL_NO_EXCEPTIONS 1     // Disable exceptions in sol

// Win32 API
#include <Windows.h>

// Standard Library
#include <unordered_set>
#include <algorithm>
#include <cstdlib>
#include <string>
#include <memory>
#include <format>
#include <atomic>
#include <vector>
#include <ctime>

#undef ERROR
#define INFO(...) MessageBoxA(nullptr, std::format(__VA_ARGS__).c_str(), "InbetweenLines - Info", MB_OK)
#define ERROR(fmt, ...) MessageBoxA(nullptr, std::format("[!] {} ({}, {}:{})", std::format(fmt, ##__VA_ARGS__), __FUNCTION__, __FILE__, __LINE__).c_str(), "InbetweenLines - Error", MB_OK)