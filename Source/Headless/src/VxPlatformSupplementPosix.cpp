#include "VxWindowFunctions.h"

#include <filesystem>
#include <system_error>

#if defined(__linux__)
#include <sys/sysinfo.h>
#else
#include <unistd.h>
#endif

XBOOL VxCopyFile(const char *source, const char *destination, XBOOL failIfExists) {
    if (!source || !destination) {
        return FALSE;
    }
    std::error_code error;
    const std::filesystem::copy_options options = failIfExists
            ? std::filesystem::copy_options::none
            : std::filesystem::copy_options::overwrite_existing;
    return std::filesystem::copy_file(source, destination, options, error) && !error
            ? TRUE : FALSE;
}

XBOOL VxDeleteFile(const char *path) {
    if (!path) {
        return FALSE;
    }
    std::error_code error;
    return std::filesystem::remove(path, error) && !error ? TRUE : FALSE;
}

XBOOL VxGetMemoryStatus(VxMemoryStatus *status) {
    if (!status) {
        return FALSE;
    }

#if defined(__linux__)
    struct sysinfo info = {};
    if (sysinfo(&info) != 0) {
        return FALSE;
    }
    const uint64_t unit = info.mem_unit ? info.mem_unit : 1;
    status->TotalPhysical = static_cast<uint64_t>(info.totalram) * unit;
    status->AvailablePhysical = static_cast<uint64_t>(info.freeram) * unit;
    status->TotalVirtual = status->TotalPhysical + static_cast<uint64_t>(info.totalswap) * unit;
    status->AvailableVirtual = status->AvailablePhysical + static_cast<uint64_t>(info.freeswap) * unit;
#else
    const long pageSize = sysconf(_SC_PAGESIZE);
    const long totalPages = sysconf(_SC_PHYS_PAGES);
    const long availablePages = sysconf(_SC_AVPHYS_PAGES);
    if (pageSize <= 0 || totalPages <= 0 || availablePages < 0) {
        return FALSE;
    }
    status->TotalPhysical = static_cast<uint64_t>(pageSize) * static_cast<uint64_t>(totalPages);
    status->AvailablePhysical = static_cast<uint64_t>(pageSize) * static_cast<uint64_t>(availablePages);
    status->TotalVirtual = status->TotalPhysical;
    status->AvailableVirtual = status->AvailablePhysical;
#endif

    const uint64_t used = status->TotalPhysical - status->AvailablePhysical;
    status->MemoryLoad = status->TotalPhysical
            ? static_cast<XDWORD>((used * 100u) / status->TotalPhysical)
            : 0;
    return TRUE;
}
