#pragma once
#include <cstdarg>
#include <cstdio>
#include <windows.h>

// 输出到调试器和工作目录下的 SekiroDog.log
inline void Log(const char* fmt, ...)
{
    char buf[2048];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    OutputDebugStringA(buf);
    OutputDebugStringA("\n");

    static FILE* file = [] {
        FILE* f = nullptr;
        fopen_s(&f, "SekiroDog.log", "w");
        return f;
    }();
    if (file)
    {
        fprintf(file, "%s\n", buf);
        fflush(file);
    }
}
