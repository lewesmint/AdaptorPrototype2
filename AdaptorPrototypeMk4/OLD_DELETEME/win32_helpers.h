#ifndef WIN32_HELPERS_H
#define WIN32_HELPERS_H

#include <windows.h>
#include <string>

class Win32Helpers {
public:
    static HANDLE CreateSharedMemory(const std::string& name, SIZE_T size);
    static HANDLE OpenSharedMemory(const std::string& name);
    static void* MapSharedMemory(HANDLE hMapFile, SIZE_T size);
    static void UnmapSharedMemory(void* pData);
    static void CloseSharedMemory(HANDLE hMapFile);
};

#endif // WIN32_HELPERS_H