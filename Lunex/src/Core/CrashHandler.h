#pragma once

/**
 * @file CrashHandler.h
 * @brief Writes a symbolized call stack (and a minidump on Windows) when the process
 * crashes, so silent access violations leave a trace in stderr and Crashes/.
 */

namespace Lunex {

	class CrashHandler {
	public:
		static void Install();
	};

}
