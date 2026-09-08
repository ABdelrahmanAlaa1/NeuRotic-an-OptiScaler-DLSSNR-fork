#pragma once
// Standalone scanner test: use Windows/D3D12 types without the application's PCH.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d3d12.h>
#include <cstdint>
#include <cstdio>
#define LOG_INFO(...) ((void)0)
#define LOG_WARN(...) ((void)0)
