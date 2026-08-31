#include "VxMemoryMappedFile.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

VxMemoryMappedFile::VxMemoryMappedFile(const char *fileName)
    : m_hFile(INVALID_HANDLE_VALUE),
      m_hFileMapping(NULL),
      m_pMemoryMappedFileBase(NULL),
      m_cbFile(0),
      m_errCode(VxMMF_FileOpen) {
    if (!fileName || !*fileName) {
        return;
    }

    m_hFile = CreateFileA(fileName, GENERIC_READ, FILE_SHARE_READ, NULL,
                         OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (m_hFile == INVALID_HANDLE_VALUE) {
        return;
    }

    LARGE_INTEGER fileSize = {};
    if (!GetFileSizeEx(static_cast<HANDLE>(m_hFile), &fileSize) ||
            fileSize.QuadPart <= 0 ||
            static_cast<unsigned long long>(fileSize.QuadPart) > SIZE_MAX) {
        CloseHandle(static_cast<HANDLE>(m_hFile));
        m_hFile = INVALID_HANDLE_VALUE;
        m_errCode = VxMMF_FileMapping;
        return;
    }
    m_cbFile = static_cast<size_t>(fileSize.QuadPart);

    m_hFileMapping = CreateFileMappingA(static_cast<HANDLE>(m_hFile), NULL,
                                        PAGE_READONLY, 0, 0, NULL);
    if (!m_hFileMapping) {
        CloseHandle(static_cast<HANDLE>(m_hFile));
        m_hFile = INVALID_HANDLE_VALUE;
        m_errCode = VxMMF_FileMapping;
        return;
    }

    m_pMemoryMappedFileBase = MapViewOfFile(static_cast<HANDLE>(m_hFileMapping),
                                           FILE_MAP_READ, 0, 0, 0);
    if (!m_pMemoryMappedFileBase) {
        CloseHandle(static_cast<HANDLE>(m_hFileMapping));
        CloseHandle(static_cast<HANDLE>(m_hFile));
        m_hFileMapping = NULL;
        m_hFile = INVALID_HANDLE_VALUE;
        m_errCode = VxMMF_MapView;
        return;
    }

    m_errCode = VxMMF_NoError;
}

VxMemoryMappedFile::~VxMemoryMappedFile() {
    if (m_pMemoryMappedFileBase) {
        UnmapViewOfFile(m_pMemoryMappedFileBase);
    }
    if (m_hFileMapping) {
        CloseHandle(static_cast<HANDLE>(m_hFileMapping));
    }
    if (m_hFile != INVALID_HANDLE_VALUE) {
        CloseHandle(static_cast<HANDLE>(m_hFile));
    }
}

void *VxMemoryMappedFile::GetBase() { return m_pMemoryMappedFileBase; }
size_t VxMemoryMappedFile::GetFileSize() { return m_cbFile; }
XBOOL VxMemoryMappedFile::IsValid() { return m_errCode == VxMMF_NoError; }
VxMMF_Error VxMemoryMappedFile::GetErrorType() { return m_errCode; }
