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
		// Heap-free unwind first: when the heap is corrupted, dbghelp (which allocates)
		// may crash before printing anything. Offsets can be symbolized offline with the PDB.
		void PrintRawStack(const CONTEXT& start) {
			static bool s_Printing = false;
			if (s_Printing) return;
			s_Printing = true;
			CONTEXT ctx = start;
			fprintf(stderr, "---- raw stack (module+offset) ----\n");
			for (int i = 0; i < 64 && ctx.Rip; i++) {
				HMODULE mod = nullptr;
				char name[MAX_PATH] = "?";
				if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
					reinterpret_cast<LPCSTR>(ctx.Rip), &mod) && mod) {
					GetModuleFileNameA(mod, name, MAX_PATH);
				}
				const char* base = strrchr(name, '\\');
				fprintf(stderr, "  raw #%02d %s+0x%llx\n", i, base ? base + 1 : name,
					static_cast<unsigned long long>(ctx.Rip - reinterpret_cast<DWORD64>(mod)));

				DWORD64 imageBase = 0;
				PRUNTIME_FUNCTION fe = RtlLookupFunctionEntry(ctx.Rip, &imageBase, nullptr);
				if (!fe) {
					ctx.Rip = *reinterpret_cast<DWORD64*>(ctx.Rsp);
					ctx.Rsp += 8;
				}
				else {
					void* handlerData = nullptr;
					DWORD64 establisher = 0;
					RtlVirtualUnwind(UNW_FLAG_NHANDLER, imageBase, ctx.Rip, fe, &ctx, &handlerData, &establisher, nullptr);
				}
			}
			fflush(stderr);
		}

		LONG WINAPI OnUnhandledException(EXCEPTION_POINTERS* info) {
			fprintf(stderr, "\n==== LUNEX CRASH (raw): exception 0x%08lX ====\n", info->ExceptionRecord->ExceptionCode);
			PrintRawStack(*info->ContextRecord);
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

	void CrashHandler::CheckHeap(const char* where) {
		static bool reported = false;
		if (reported) return;
		if (!HeapValidate(GetProcessHeap(), 0, nullptr)) {
			reported = true;
			fprintf(stderr, "==== HEAP CORRUPTED before: %s ====\n", where);
			fflush(stderr);
		}
	}
#else
	void CrashHandler::Install() {}
	void CrashHandler::CheckHeap(const char*) {}
#endif

}
