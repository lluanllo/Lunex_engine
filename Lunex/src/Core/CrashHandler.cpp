#include "stpch.h"
#include "CrashHandler.h"

#ifdef _WIN32
#include <Windows.h>
#include <DbgHelp.h>
#pragma comment(lib, "dbghelp.lib")

#include <cstdio>
#include <ctime>
#include <filesystem>
#endif

namespace Lunex {

#ifdef _WIN32
	namespace {
		LONG WINAPI OnUnhandledException(EXCEPTION_POINTERS* info) {
			HANDLE process = GetCurrentProcess();
			HANDLE thread = GetCurrentThread();
			SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
			SymInitialize(process, nullptr, TRUE);

			fprintf(stderr, "\n==== LUNEX CRASH: exception 0x%08lX at %p ====\n",
				info->ExceptionRecord->ExceptionCode, info->ExceptionRecord->ExceptionAddress);

			CONTEXT ctx = *info->ContextRecord;
			STACKFRAME64 frame = {};
			frame.AddrPC.Offset = ctx.Rip;    frame.AddrPC.Mode = AddrModeFlat;
			frame.AddrFrame.Offset = ctx.Rbp; frame.AddrFrame.Mode = AddrModeFlat;
			frame.AddrStack.Offset = ctx.Rsp; frame.AddrStack.Mode = AddrModeFlat;

			alignas(SYMBOL_INFO) char symbolBuffer[sizeof(SYMBOL_INFO) + 512];
			for (int i = 0; i < 48; i++) {
				if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, process, thread, &frame, &ctx, nullptr,
					SymFunctionTableAccess64, SymGetModuleBase64, nullptr) || frame.AddrPC.Offset == 0)
					break;

				auto* symbol = reinterpret_cast<SYMBOL_INFO*>(symbolBuffer);
				symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
				symbol->MaxNameLen = 511;
				DWORD64 displacement = 0;
				const char* name = SymFromAddr(process, frame.AddrPC.Offset, &displacement, symbol) ? symbol->Name : "???";

				IMAGEHLP_LINE64 line = {};
				line.SizeOfStruct = sizeof(line);
				DWORD lineDisp = 0;
				if (SymGetLineFromAddr64(process, frame.AddrPC.Offset, &lineDisp, &line))
					fprintf(stderr, "  #%02d %s  (%s:%lu)\n", i, name, line.FileName, line.LineNumber);
				else
					fprintf(stderr, "  #%02d %s\n", i, name);
			}
			fflush(stderr);

			// Minidump next to the executable's working directory
			std::error_code ec;
			std::filesystem::create_directories("Crashes", ec);
			char path[128];
			snprintf(path, sizeof(path), "Crashes/Lunex_%lld.dmp", static_cast<long long>(time(nullptr)));
			HANDLE file = CreateFileA(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
			if (file != INVALID_HANDLE_VALUE) {
				MINIDUMP_EXCEPTION_INFORMATION mei{ GetCurrentThreadId(), info, FALSE };
				MiniDumpWriteDump(process, GetCurrentProcessId(), file, MiniDumpNormal, &mei, nullptr, nullptr);
				CloseHandle(file);
				fprintf(stderr, "Minidump written to %s\n", path);
			}

			SymCleanup(process);
			return EXCEPTION_EXECUTE_HANDLER;
		}
	}

	void CrashHandler::Install() {
		SetUnhandledExceptionFilter(OnUnhandledException);
	}
#else
	void CrashHandler::Install() {}
#endif

}
