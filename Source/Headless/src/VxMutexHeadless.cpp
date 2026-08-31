#include "VxMutex.h"

#include <mutex>

VxMutex::VxMutex() : m_Mutex(new std::recursive_mutex()) {}

VxMutex::~VxMutex() {
    delete static_cast<std::recursive_mutex *>(m_Mutex);
    m_Mutex = NULL;
}

XBOOL VxMutex::EnterMutex() {
    if (!m_Mutex) {
        return FALSE;
    }
    static_cast<std::recursive_mutex *>(m_Mutex)->lock();
    return TRUE;
}

XBOOL VxMutex::LeaveMutex() {
    if (!m_Mutex) {
        return FALSE;
    }
    static_cast<std::recursive_mutex *>(m_Mutex)->unlock();
    return TRUE;
}

XBOOL VxMutex::operator++(int) { return EnterMutex(); }
XBOOL VxMutex::operator--(int) { return LeaveMutex(); }
