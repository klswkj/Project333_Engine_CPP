#pragma once

#include <cassert>
#include <cstdint>

// ----------------------------------------
// Build Switch
// -----------------------------------------

#ifndef DO_CHECK
	#if defined(_DEBUG)
		#define DO_CHECK 1
	#else
		#define DO_CHECK 0
	#endif
#endif

#ifndef DO_ENSURE
	#if defined(_DEBUG)
		#define DO_ENSURE 1
	#else
		#define DO_ENSURE 1
	#endif
#endif

// ----------------------------------------
// Unique Name Helpers
// -----------------------------------------
#define CONCAT_IMPL(a, b) a##b
#define CONCAT(a, b) CONCAT_IMPL(a, b)

// ----------------------------------------
// Platform Debug Break
// -----------------------------------------

#if defined(_MSC_VER)
	#define DEBUG_BREAK() __debugbreak()

#elif defined(__GNUC__) || defined(__clang__)
	#if defined(__i386__) || defined(__x86_64__)
		#define DEBUG_BREAK() __builtin_trap()
	#elif defined(__arm__) || defined(__aarch64__)
		#define DEBUG_BREAK() __builtin_trap()
	#else
		#define DEBUG_BREAK() ((void)0)
	#endif

#else
	#define DEBUG_BREAK() ((void)0)
#endif

// ----------------------------------------
// Debug Break
// -----------------------------------------

#if defined(_MSC_VER)
	#define DEBUG_BREAK() __debugbreak()
#else
	#define DEBUG_BREAK() ((void)0)
#endif

// ----------------------------------------
// Internal Check Implementations
// -----------------------------------------
#pragma region Internal Check Implementations

#define CHECK_IMPL(expr)                           \
	do                                             \
	{                                              \
		if (!(expr))                               \
		{                                          \
			LOG_ERROR(L"Check failed: " #expr, 1); \
			DEBUG_BREAK();                         \
			assert(false);                         \
		}                                          \
	} while (false)

#define CHECKF_IMPL(expr, msg, ErrCode) \
	do                                  \
	{                                   \
		if (!(expr))                    \
		{                               \
			LOG_ERROR(msg, ErrCode);    \
			DEBUG_BREAK();              \
			assert(false);              \
		}                               \
	} while (false)

#define RELEASE_CHECK_IMPL(expr)                   \
	do                                             \
	{                                              \
		if (!(expr))                               \
		{                                          \
			LOG_ERROR(L"Check failed: " #expr, 1); \
			DEBUG_BREAK();                         \
			assert(false);                         \
		}                                          \
	} while (false)

#define RELEASE_CHECKF_IMPL(expr, msg, ErrCode) \
	do                                  \
	{                                   \
		if (!(expr))                    \
		{                               \
			LOG_ERROR(msg, ErrCode);    \
			DEBUG_BREAK();              \
			assert(false);              \
		}                               \
	} while (false)

#define ENSURE_IMPL(expr)                           \
	([&]() -> bool                                  \
	{                                               \
		const bool bResult = !!(expr);              \
		if (!bResult)                               \
		{                                           \
			LOG_ERROR(L"Ensure failed: " #expr, 1); \
			DEBUG_BREAK();                          \
		}                                           \
		return bResult;                             \
	}())

#define ENSUREF_IMPL(expr, msg, ErrCode) \
	([&]() -> bool                                  \
	{                                               \
		const bool bResult = !!(expr);              \
		if (!bResult)                               \
		{                                           \
			LOG_ERROR(msg, ErrCode);                \
			DEBUG_BREAK();                          \
		}                                           \
		return bResult;                             \
	}())

#pragma endregion Internal Check Implementations

// ----------------------------------------
// Public Macros
// -----------------------------------------

#if DO_CHECK
	#ifndef CHECK_CODE
		#define CHECK_CODE(Code) \
			do                   \
			{                    \
				Code             \
			} while (false)
	#endif

	#ifndef CHECK
		#define CHECK(expr) CHECK_IMPL(expr)
	#endif

	#ifndef CHECKF
		#define CHECKF(expr, msg, ErrCode) CHECKF_IMPL(expr, msg, ErrCode)
	#endif

	#ifndef VERIFY
		#define VERIFY(expr) CHECK_IMPL(expr)
	#endif

	#ifndef VERIFYF
		#define VERIFYF(expr, msg, ErrCode) CHECKF_IMPL(expr, msg, ErrCode)
	#endif

#else

	#ifndef CHECK_CODE
		#define CHECK_CODE(Code) ((void)0)
	#endif
	#ifndef CHECK
		#define CHECK(expr) ((void)0)
	#endif
	#ifndef CHECKF
		#define CHECKF(expr, msg, ErrCode) ((void)0)
	#endif
	#ifndef VERIFY
		#define VERIFY(expr) ((void)(expr))
	#endif
	#ifndef VERIFYF
		#define VERIFYF(expr, msg, ErrCode) ((void)(expr))
	#endif
#endif

#if DO_ENSURE
	#ifndef ENSURE
		#define ENSURE(expr) ENSURE_IMPL(expr)
	#endif
	#ifndef ENSUREF
		#define ENSUREF(expr, msg, ErrCode) ENSUREF_IMPL(expr, msg, ErrCode)
	#endif
#else

	#ifndef ENSURE
		#define ENSURE(expr) (!!(expr))
	#endif

	#ifndef ENSUREF
		#define ENSUREF(expr, msg, ErrCode) (!!(expr))
	#endif
#endif


#ifndef UNREACHABLE
	#if DO_CHECK
		#define UNREACHABLE()                          \
		do                                             \
		{                                              \
			LOG_ERROR(L"Reached unreachable code", 1); \
			DEBUG_BREAK();                             \
			assert(false);                             \
		} while (false)
	#else
		#define UNREACHABLE() ((void)0)
	#endif
#endif

#ifndef UNREACHABLEF
#define UNREACHABLEF(msg, ErrCode)                 \
	do                                             \
	{                                              \
		LOG_ERROR(msg, ErrCode);                   \
		DEBUG_BREAK();                             \
		assert(false);                             \
	} while (false)
#endif

// ----------------------------------------
// RELEASE_CHECK
// -----------------------------------------

#ifndef RELEASE_CHECK
	#define RELEASE_CHECK(expr) RELEASE_CHECK_IMPL(expr)
#endif

#ifndef RELEASE_CHECKF
	#define RELEASE_CHECKF(expr, msg, ErrCode) RELEASE_CHECKF_IMPL(expr, msg, ErrCode)
#endif

// ----------------------------------------
// Scope Guards
// -----------------------------------------

struct FNoReentryGuard
{
    bool& bEnteredRef;

    explicit FNoReentryGuard(bool& InEnteredRef)
        : bEnteredRef(InEnteredRef)
    {
        bEnteredRef = true;
    }

    ~FNoReentryGuard()
    {
        bEnteredRef = false;
    }
};

struct FNoRecursionGuard
{
    int32_t& DepthRef;

    explicit FNoRecursionGuard(int32_t& InDepthRef)
        : DepthRef(InDepthRef)
    {
        ++DepthRef;
    }

    ~FNoRecursionGuard()
    {
        --DepthRef;
    }
};

#define CHECK_NOENTRY()                                                     \
    static bool CONCAT(GCheckNoReentry_, __LINE__) = false;                  \
    checkf(!CONCAT(GCheckNoReentry_, __LINE__), L"Reentry detected", 1);     \
    FNoReentryGuard CONCAT(GCheckNoReentryGuard_, __LINE__)                  \
    (CONCAT(GCheckNoReentry_, __LINE__))

#define CHECK_NORECURSION()                                                   \
    static thread_local int32_t CONCAT(GCheckNoRecDepth_, __LINE__) = 0;     \
    checkf(0 == CONCAT(GCheckNoRecDepth_, __LINE__), L"Recursion detected", 1); \
    FNoRecursionGuard CONCAT(GCheckNoRecGuard_, __LINE__)                    \
    (CONCAT(GCheckNoRecDepth_, __LINE__))


/*
#if DO_CHECK
	#if defined(_MSC_VER)
		#define DEBUG_BREAK() __debugbreak()
	#elif defined(__GNUC__) || defined(__clang__)
		#define DEBUG_BREAK() __builtin_trap()
	#else
		#include <signal.h>
		#define DEBUG_BREAK() raise(SIGTRAP)
	#endif
#endif
*/