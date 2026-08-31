#include "VxMath.h"
#include "VxMemoryMappedFile.h"
#include "VxSharedLibrary.h"
#include "VxThread.h"
#include "VxTimeProfiler.h"
#include "VxWindowFunctions.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace {
unsigned int ProbeThread(void *argument) {
    int *value = static_cast<int *>(argument);
    *value = 42;
    return 7;
}

XBOOL DirectoryCallback(const VxDirectoryEntry *, void *argument) {
    int *count = static_cast<int *>(argument);
    ++*count;
    return TRUE;
}

int Fail(const char *message) {
    std::fprintf(stderr, "VxMathHeadlessProbe: %s\n", message);
    return 1;
}
}

int main() {
    VxVector a(1.0f, 2.0f, 3.0f);
    VxVector b(4.0f, 5.0f, 6.0f);
    if (std::fabs(DotProduct(a, b) - 32.0f) > 0.0001f) {
        return Fail("vector math failed");
    }

    VxTimeProfiler profiler;
    if (profiler.Current() < 0.0f) {
        return Fail("steady clock moved backwards");
    }
    const XWORD fpuControl = VxGetFPUControlWord();
    VxSetFPUControlWord(fpuControl);

    VxSharedLibrary systemLibrary;
#if defined(_WIN32)
    const char *systemLibraryName = "kernel32.dll";
    const char *systemFunctionName = "GetCurrentProcessId";
#else
    const char *systemLibraryName = "libc.so.6";
    const char *systemFunctionName = "getpid";
#endif
    if (!systemLibrary.Load(systemLibraryName) ||
            !systemLibrary.GetFunctionPtr(systemFunctionName)) {
        return Fail("shared library loading failed");
    }
    systemLibrary.ReleaseLibrary();

    char basePath[4096] = {};
    if (!VxGetApplicationBasePath(basePath, sizeof(basePath)) || !*basePath) {
        return Fail("application base path unavailable");
    }

    char modulePath[4096] = {};
    if (!VxGetModuleFileName(NULL, modulePath, sizeof(modulePath)) ||
            !VxFileExists(modulePath)) {
        return Fail("module path unavailable");
    }

    VxMemoryMappedFile mapped(modulePath);
    if (!mapped.IsValid() || !mapped.GetBase() || mapped.GetFileSize() == 0) {
        return Fail("read-only file mapping failed");
    }

    int directoryEntries = 0;
    if (!VxListDirectory(basePath, "*", TRUE, DirectoryCallback, &directoryEntries) ||
            directoryEntries == 0) {
        return Fail("directory enumeration failed");
    }

    VxMemoryStatus memory = {};
    if (!VxGetMemoryStatus(&memory) || memory.TotalPhysical == 0) {
        return Fail("memory status unavailable");
    }

    int threadValue = 0;
#if !defined(_WIN32)
    // The legacy POSIX VxThread contract uses the first instance to represent
    // the calling (main) thread and creates native threads from later objects.
    VxThread mainThread;
#endif
    VxThread thread;
    thread.SetName("headless-probe");
    if (!thread.CreateThread(ProbeThread, &threadValue)) {
        return Fail("thread creation failed");
    }
    unsigned int threadStatus = 0;
    if (thread.Wait(&threadStatus, 5000) != VXT_OK ||
            threadStatus != 7 || threadValue != 42) {
        return Fail("thread wait failed");
    }
    thread.Close();

    std::puts("VxMath headless probe passed");
    return 0;
}
