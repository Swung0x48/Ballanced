#include "VxSharedLibrary.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include "VxWindowFunctions.h"

VxSharedLibrary::VxSharedLibrary() : m_LibraryHandle(NULL) {}

void VxSharedLibrary::Attach(INSTANCE_HANDLE libraryHandle) {
    if (m_LibraryHandle != libraryHandle) {
        ReleaseLibrary();
        m_LibraryHandle = libraryHandle;
    }
}

INSTANCE_HANDLE VxSharedLibrary::Load(const char *libraryName) {
    ReleaseLibrary();
    if (!libraryName || !*libraryName) {
        return NULL;
    }

    const XWORD fpuControlWord = VxGetFPUControlWord();
    m_LibraryHandle = reinterpret_cast<INSTANCE_HANDLE>(LoadLibraryA(libraryName));
    VxSetFPUControlWord(fpuControlWord);
    return m_LibraryHandle;
}

void VxSharedLibrary::ReleaseLibrary() {
    if (m_LibraryHandle) {
        FreeLibrary(reinterpret_cast<HMODULE>(m_LibraryHandle));
        m_LibraryHandle = NULL;
    }
}

void *VxSharedLibrary::GetFunctionPtr(const char *functionName) {
    if (!m_LibraryHandle || !functionName) {
        return NULL;
    }
    return reinterpret_cast<void *>(
            GetProcAddress(reinterpret_cast<HMODULE>(m_LibraryHandle), functionName));
}
