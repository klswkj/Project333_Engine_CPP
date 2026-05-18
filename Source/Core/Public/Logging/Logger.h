#pragma once

// Note : Don't include <Windows.h> here, 
// as it may cause conflicts with other platforms. 
// Include it in the .cpp file instead.

#include <cassert>

class Logger
{
public:
	static void Log
	(
		const char*    InputFile,
		int			   InputLine,
		const char*    InputFunction,
		const wchar_t* InputContext,
		unsigned long  InputErrorCode
	);

	static void LogWithCS
	(
		const char*    InputFile,
		int			   InputLine,
		const char*    InputFunction,
		const wchar_t* InputContext,
		unsigned long  InputErrorCode
	);
};

#ifndef LOG_ERROR
	#define LOG_ERROR(Context, ErrorCode) \
		do \
		{ \
		Logger::Log(__FILE__, __LINE__, __FUNCTION__, Context, ErrorCode); \
		__debugbreak(); \
		assert(false); \
		} while (false)
#endif

#ifndef LOG_WARNING
	#define LOG_WARNING(Context, ErrorCode) \
			Logger::Log(__FILE__, __LINE__, __FUNCTION__, Context, ErrorCode)
#endif

#ifndef LOG_MESSAGE
	#define LOG_MESSAGE(Context, ErrorCode) \
				Logger::Log(__FILE__, __LINE__, __FUNCTION__, Context, ErrorCode)
#endif

