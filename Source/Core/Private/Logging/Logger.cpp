#include "../../Public/Logging/Logger.h"
#include "../../Public/Threading/CriticalSection.h"

#include <Windows.h>
#include <string>
#include <sstream>

static std::wstring ConvertToWString(const char* InputText)
{
	if (InputText == nullptr)
	{
		return L"";
	}

	int NeededSize = MultiByteToWideChar
	(
		CP_UTF8, 
		0, 
		InputText, 
		-1, 
		nullptr, 
		0
	);

	if (NeededSize <= 0)
	{
		return L"";
	}

	std::wstring Result;
	Result.resize(static_cast<size_t>(NeededSize) - 1); // Exclude null terminator

	MultiByteToWideChar
	(
		CP_UTF8,
		0,
		InputText,
		-1,
		Result.data(),
		NeededSize
	);

	return Result;
}

void Logger::Log
(
	const char*    InputFile,
	int            InputLine,
	const char*    InputFunction,
	const wchar_t* InputContext,
	unsigned long  InputErrorCode
)
{
	LPWSTR pSystemMessage = nullptr;

	DWORD FormatMessageResult = ::FormatMessageW
	(
		FORMAT_MESSAGE_ALLOCATE_BUFFER
		| FORMAT_MESSAGE_FROM_SYSTEM
		| FORMAT_MESSAGE_IGNORE_INSERTS,
		nullptr, // lpSource
		InputErrorCode,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // dwLanguageId
		reinterpret_cast<LPWSTR>(&pSystemMessage), // lpBuffer
		0, // nSize
		nullptr // Arguments
	);

	std::wstringstream LogStream;
	LogStream << L"[ERROR]\n";
	LogStream << L"  File      : " << ConvertToWString(InputFile) << L"\n";
	LogStream << L"  Line      : " << InputLine << L"\n";
	LogStream << L"  Function  : " << ConvertToWString(InputFunction) << L"\n";
	LogStream << L"  Context   : " << (nullptr != InputContext ? InputContext : L"") << L"\n";
	LogStream << L"  Win32Code : " << InputErrorCode << L"\n";

	if (nullptr != pSystemMessage)
	{
		LogStream << L"  Message   : " << pSystemMessage;
		::LocalFree(pSystemMessage);
	}

	LogStream << L"\n";

	::OutputDebugStringW(LogStream.str().c_str());
}

namespace
{
	Core::Threading::CriticalSection GLoggerCriticalSection;
}

void Logger::LogWithCS
(
	const char*    InputFile,
	int            InputLine,
	const char*    InputFunction,
	const wchar_t* InputContext,
	unsigned long  InputErrorCode
)
{
	Core::Threading::ScopeLock Lock(GLoggerCriticalSection);

	LPWSTR pSystemMessage = nullptr;

	DWORD FormatMessageResult = ::FormatMessageW
	(
		FORMAT_MESSAGE_ALLOCATE_BUFFER
		| FORMAT_MESSAGE_FROM_SYSTEM
		| FORMAT_MESSAGE_IGNORE_INSERTS,
		nullptr, // lpSource
		InputErrorCode,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // dwLanguageId
		reinterpret_cast<LPWSTR>(&pSystemMessage), // lpBuffer
		0, // nSize
		nullptr // Arguments
	);

	std::wstringstream LogStream;
	LogStream << L"[ERROR]\n";
	LogStream << L"  File      : " << ConvertToWString(InputFile) << L"\n";
	LogStream << L"  Line      : " << InputLine << L"\n";
	LogStream << L"  Function  : " << ConvertToWString(InputFunction) << L"\n";
	LogStream << L"  Context   : " << (nullptr != InputContext ? InputContext : L"") << L"\n";
	LogStream << L"  Win32Code : " << InputErrorCode << L"\n";

	if (nullptr != pSystemMessage)
	{
		LogStream << L"  Message   : " << pSystemMessage;
		::LocalFree(pSystemMessage);
	}

	LogStream << L"\n";

	::OutputDebugStringW(LogStream.str().c_str());
}