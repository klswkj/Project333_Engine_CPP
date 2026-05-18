#pragma once

/*
CRITICAL_SECTION
	exclusive lock
	recursive 가능
	같은 thread가 여러 번 Lock 가능
	Logger 같은 단순 lock에 무난

SRWLOCK ( ResourceManager, ShaderCache, DescriptorCache, AssetDatabase)
	shared / exclusive lock
	reader-writer lock
	recursive lock 용도로 쓰면 위험
	read-heavy 구조에 좋음
	=> Reader 여러 명 동시진입 가능
	=> Writer만 Exclusive할때 좋음
*/

namespace Core::Threading
{
	class CriticalSection
	{
	private:
		void* m_NativeCS; // Critical Section

	public:
		CriticalSection();
		~CriticalSection();

		CriticalSection(const CriticalSection&) = delete;
		CriticalSection& operator=(const CriticalSection&) = delete;

		void Lock();
		void Unlock();
	};


	class ScopeLock
	{
	private:
		CriticalSection& m_CS;

	public:
		explicit ScopeLock(CriticalSection& InCriticalSection)
			: m_CS(InCriticalSection)
		{
			m_CS.Lock();
		}

		~ScopeLock()
		{
			m_CS.Unlock();
		}

		ScopeLock           (const ScopeLock&) = delete;
		ScopeLock& operator=(const ScopeLock&) = delete;
	};

	class SRWLock
	{
	private:
		void* m_NativeSRWLock;

    public:
        SRWLock();
        ~SRWLock();

        SRWLock(const SRWLock&) = delete;
        SRWLock& operator=(const SRWLock&) = delete;

        void LockShared();
        bool TryLockShared();
        void UnlockShared();

        void LockExclusive();
        bool TryLockExclusive();
        void UnlockExclusive();
	};

	class ScopeSRWReadLock
    {
    private:
        SRWLock& m_Lock;

    public:
        explicit ScopeSRWReadLock(SRWLock& InLock)
            : m_Lock(InLock)
        {
            m_Lock.LockShared();
        }

        ~ScopeSRWReadLock()
        {
            m_Lock.UnlockShared();
        }

        ScopeSRWReadLock           (const ScopeSRWReadLock&) = delete;
        ScopeSRWReadLock& operator=(const ScopeSRWReadLock&) = delete;
    };

	class ScopeSRWWriteLock
    {
    private:
        SRWLock& m_Lock;

    public:
        explicit ScopeSRWWriteLock(SRWLock& InLock)
            : m_Lock(InLock)
        {
            m_Lock.LockExclusive();
        }

        ~ScopeSRWWriteLock()
        {
            m_Lock.UnlockExclusive();
        }

        ScopeSRWWriteLock           (const ScopeSRWWriteLock&) = delete;
        ScopeSRWWriteLock& operator=(const ScopeSRWWriteLock&) = delete;
    };
}