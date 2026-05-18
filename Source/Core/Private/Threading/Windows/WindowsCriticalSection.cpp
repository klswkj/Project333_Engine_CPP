#include "Threading/CriticalSection.h"

#ifndef NOMINMAX
	#define NOMINMAX
#endif

#ifndef WIN32_LEAN_AND_MEAN
	#define WIN32_LEAN_AND_MEAN
#endif

#include <Windows.h>

#include "../../../Public/Diagnostics/DebugCheck.h"
#include "../../../Public/Logging/Logger.h"

namespace Core::Threading
{
#pragma region CS

	CriticalSection::CriticalSection()
		: m_NativeCS(nullptr)
	{
		CRITICAL_SECTION* pCS = static_cast<CRITICAL_SECTION*>(
			::HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(CRITICAL_SECTION))
			);

		CHECKF(nullptr != pCS, L"HeapAlloc() for CRITICAL_SECTION failed.", GetLastError());

		::InitializeCriticalSection(pCS);

		m_NativeCS = pCS;
	}

	CriticalSection::~CriticalSection()
	{
		CRITICAL_SECTION* NativeCS =
			static_cast<CRITICAL_SECTION*>(m_NativeCS);

		if (nullptr != m_NativeCS)
		{
			::DeleteCriticalSection(NativeCS);
			HeapFree(GetProcessHeap(), 0, NativeCS);

			m_NativeCS = nullptr;
		}
	}

	void CriticalSection::Lock()
	{
		CRITICAL_SECTION* NativeCS =
			static_cast<CRITICAL_SECTION*>(m_NativeCS);

		CHECKF(nullptr != NativeCS, L"CriticalSection native object is nullptr.", 1);

		::EnterCriticalSection(NativeCS);
	}

	void CriticalSection::Unlock()
	{
		
		CRITICAL_SECTION* NativeCS =
			static_cast<CRITICAL_SECTION*>(m_NativeCS);
		
		CHECKF(nullptr != NativeCS, L"CriticalSection native object is nullptr.", 1);

		::LeaveCriticalSection(NativeCS);
	}
	
#pragma endregion CS

#pragma region SRWLock
	SRWLock::SRWLock()
		: m_NativeSRWLock(nullptr)
	{
		SRWLOCK* NativeSRWLock = static_cast<SRWLOCK*>(
			HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(SRWLOCK))
		);

		CHECKF(nullptr != NativeSRWLock, L"HeapAlloc() for SRWLOCK failed.", GetLastError());

		if (nullptr == NativeSRWLock)
		{
			return;
		}

		InitializeSRWLock(NativeSRWLock);

		m_NativeSRWLock = NativeSRWLock;
	}

	SRWLock::~SRWLock()
	{
		SRWLOCK* NativeSRWLock = static_cast<SRWLOCK*>(m_NativeSRWLock);

		if (nullptr != NativeSRWLock)
		{
			// SRWLOCK has no DeleteSRWLock().
			// It only needs memory cleanup after no thread is using it.
			HeapFree(GetProcessHeap(), 0, NativeSRWLock);

			m_NativeSRWLock = nullptr;
		}
	}

	void SRWLock::LockShared()
	{
		SRWLOCK* NativeSRWLock = static_cast<SRWLOCK*>(m_NativeSRWLock);

		CHECKF(nullptr != NativeSRWLock, L"SRWLOCK native object is nullptr.", 1);

		if (nullptr == NativeSRWLock)
		{
			return;
		}

		AcquireSRWLockShared(NativeSRWLock);
	}

	bool SRWLock::TryLockShared()
	{
		SRWLOCK* NativeSRWLock = static_cast<SRWLOCK*>(m_NativeSRWLock);

		CHECKF(nullptr != NativeSRWLock, L"SRWLOCK native object is nullptr.", 1);

		if (nullptr == NativeSRWLock)
		{
			return false;
		}

		return TRUE == TryAcquireSRWLockShared(NativeSRWLock);
	}

	void SRWLock::UnlockShared()
	{
		SRWLOCK* NativeSRWLock = static_cast<SRWLOCK*>(m_NativeSRWLock);

		CHECKF(nullptr != NativeSRWLock, L"SRWLOCK native object is nullptr.", 1);

		if (nullptr == NativeSRWLock)
		{
			return;
		}
		
#pragma warning(suppress: 26110)
		::ReleaseSRWLockShared(NativeSRWLock);
	}

	void SRWLock::LockExclusive()
	{
		SRWLOCK* NativeSRWLock = static_cast<SRWLOCK*>(m_NativeSRWLock);

		CHECKF(nullptr != NativeSRWLock, L"SRWLOCK native object is nullptr.", 1);

		if (nullptr == NativeSRWLock)
		{
			return;
		}

		AcquireSRWLockExclusive(NativeSRWLock);
	}

	bool SRWLock::TryLockExclusive()
	{
		SRWLOCK* NativeSRWLock = static_cast<SRWLOCK*>(m_NativeSRWLock);

		CHECKF(nullptr != NativeSRWLock, L"SRWLOCK native object is nullptr.", 1);

		if (nullptr == NativeSRWLock)
		{
			return false;
		}

		return TRUE == TryAcquireSRWLockExclusive(NativeSRWLock);
	}

	void SRWLock::UnlockExclusive()
	{
		SRWLOCK* NativeSRWLock = static_cast<SRWLOCK*>(m_NativeSRWLock);

		CHECKF(nullptr != NativeSRWLock, L"SRWLOCK native object is nullptr.", 1);

		if (nullptr == NativeSRWLock)
		{
			return;
		}
		
#pragma warning(suppress: 26110)
		::ReleaseSRWLockExclusive(NativeSRWLock);
	}
#pragma endregion SRWLock
}