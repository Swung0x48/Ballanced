#include "VxThread.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

VxThread *VxThread::m_MainThread = NULL;

VxThread::VxThread()
    : m_Name(), m_Thread(NULL), m_ThreadID(0), m_State(VXTS_JOINABLE),
      m_Priority(VXTP_NORMAL), m_Func(NULL), m_Args(NULL) {}

VxThread::~VxThread() {
    if (m_Thread) {
        VxMutexLock lock(GetMutex());
        GetHashThread().Remove(m_Thread);
        CloseHandle(static_cast<HANDLE>(m_Thread));
    }
}

XBOOL VxThread::CreateThread(VxThreadFunction *func, void *args) {
    if (IsCreated()) {
        return TRUE;
    }

    m_Func = func;
    m_Args = args;
    DWORD threadId = 0;
    m_Thread = ::CreateThread(NULL, 0, ThreadFunc, this, 0, &threadId);
    if (!m_Thread) {
        return FALSE;
    }
    m_ThreadID = static_cast<XUINTPTR>(threadId);
    m_State |= VXTS_CREATED;

    if (m_Name.Length() == 0) {
        m_Name = "THREAD_";
        m_Name << static_cast<unsigned int>(m_ThreadID);
    }

    {
        VxMutexLock lock(GetMutex());
        GetHashThread().Insert(m_Thread, this);
    }
    SetPriority();
    return TRUE;
}

void VxThread::SetPriority(unsigned int priority) {
    m_Priority = priority;
    if (IsCreated()) {
        SetPriority();
    }
}

void VxThread::SetName(const char *name) { m_Name = name ? name : ""; }

void VxThread::Close() {
    if (m_Thread) {
        VxMutexLock lock(GetMutex());
        GetHashThread().Remove(m_Thread);
        CloseHandle(static_cast<HANDLE>(m_Thread));
    }
    m_ThreadID = 0;
    m_Thread = NULL;
    m_State = VXTS_INITIALE;
    m_Priority = VXTP_NORMAL;
    m_Func = NULL;
    m_Args = NULL;
}

const XString &VxThread::GetName() const { return m_Name; }
unsigned int VxThread::GetPriority() const { return m_Priority; }
XBOOL VxThread::IsCreated() const { return (m_State & VXTS_CREATED) != 0; }
XBOOL VxThread::IsJoinable() const { return (m_State & VXTS_JOINABLE) != 0; }
XBOOL VxThread::IsMainThread() const { return (m_State & VXTS_MAIN) != 0; }
XBOOL VxThread::IsStarted() const { return (m_State & VXTS_STARTED) != 0; }

VxThread *VxThread::GetCurrentVxThread() {
    const XUINTPTR currentThreadId = GetCurrentVxThreadId();
    VxMutexLock lock(GetMutex());
    for (XHashTable<VxThread *, GENERIC_HANDLE>::Iterator it = GetHashThread().Begin();
         it != GetHashThread().End(); ++it) {
        VxThread *thread = *it;
        if (thread && thread->m_ThreadID == currentThreadId) {
            return thread;
        }
    }
    return NULL;
}

int VxThread::Wait(unsigned int *status, unsigned int timeout) {
    if (!m_Thread) {
        return VXTERROR_NULLTHREAD;
    }
    const DWORD waitResult = WaitForSingleObject(static_cast<HANDLE>(m_Thread),
                                                 timeout ? timeout : INFINITE);
    if (waitResult == WAIT_TIMEOUT) {
        return VXTERROR_TIMEOUT;
    }
    if (waitResult != WAIT_OBJECT_0) {
        return VXTERROR_WAIT;
    }

    unsigned int localStatus = 0;
    if (!GetExitCode(localStatus)) {
        return VXTERROR_EXITCODE;
    }
    if (status) {
        *status = localStatus;
    }
    return localStatus == VXT_STILLACTIVE ? VXTERROR_WAIT : VXT_OK;
}

const GENERIC_HANDLE VxThread::GetHandle() const { return m_Thread; }
XUINTPTR VxThread::GetID() const { return m_ThreadID; }

XBOOL VxThread::GetExitCode(unsigned int &status) {
    if (!m_Thread) {
        return FALSE;
    }
    DWORD nativeStatus = 0;
    if (!GetExitCodeThread(static_cast<HANDLE>(m_Thread), &nativeStatus)) {
        return FALSE;
    }
    status = nativeStatus == STILL_ACTIVE ? VXT_STILLACTIVE : nativeStatus;
    return TRUE;
}

XBOOL VxThread::Terminate(unsigned int *status) {
    if (!m_Thread) {
        return FALSE;
    }
    return TerminateThread(static_cast<HANDLE>(m_Thread), status ? *status : 0) ? TRUE : FALSE;
}

XUINTPTR VxThread::GetCurrentVxThreadId() {
    return static_cast<XUINTPTR>(GetCurrentThreadId());
}

void VxThread::SetPriority() {
    if (!m_Thread) {
        return;
    }
    static const int priorities[] = {
        THREAD_PRIORITY_NORMAL,
        THREAD_PRIORITY_ABOVE_NORMAL,
        THREAD_PRIORITY_BELOW_NORMAL,
        THREAD_PRIORITY_HIGHEST,
        THREAD_PRIORITY_LOWEST,
        THREAD_PRIORITY_IDLE,
        THREAD_PRIORITY_TIME_CRITICAL,
    };
    const unsigned int index = m_Priority <= VXTP_TIMECRITICAL ? m_Priority : VXTP_NORMAL;
    SetThreadPriority(static_cast<HANDLE>(m_Thread), priorities[index]);
}

VxMutex &VxThread::GetMutex() {
    static VxMutex threadMutex;
    return threadMutex;
}

XHashTable<VxThread *, GENERIC_HANDLE> &VxThread::GetHashThread() {
    static XHashTable<VxThread *, GENERIC_HANDLE> threads;
    return threads;
}

unsigned long VX_STDCALL VxThread::ThreadFunc(void *args) {
    if (!args) {
        return VXTERROR_NULLTHREAD;
    }
    VxThread *thread = static_cast<VxThread *>(args);
    thread->m_State |= VXTS_STARTED;
    const unsigned long result = thread->m_Func
            ? thread->m_Func(thread->m_Args)
            : thread->Run();
    thread->m_State &= ~VXTS_STARTED;
    return result;
}
