#ifndef METROID_PRIME2_PORT_PORT_CRASH_H
#define METROID_PRIME2_PORT_PORT_CRASH_H

#include <string>

// Writes a crash report into the log: what went wrong, where (module + offset,
// which a symbolizer turns back into a line of code), the build, and the stack.
// The breadcrumbs (PortBreadcrumb) follow. Nothing else changes about the crash,
// so the system still gets it afterwards: a core dump on Linux, Android's
// tombstone, and the exit status still says which signal (a harness greps it).
// From MP1 (MetroidPrimePort), without its Windows report.
//
// MP2_CRASH_TEST=segv|abort crashes right after Install, to check the report.
namespace PortCrash {

// dumpFolder: where Windows writes metroid_prime2_port.dmp (empty: no dump).
void Install(const std::string& dumpFolder = {});

// Runs at the end of a crash report, still in the signal handler (async-signal-safe
// code only): Android uses it to let the stderr pump reach logcat before the process dies.
void SetAfterReport(void (*afterReport)(int signal, const void* address));

// Linux and Android: asks the thread with kernel id `threadId` (gettid; macOS: its pthread_t) to write its
// current stack into the log as "watchdog:" lines (needs Install), then carry on.
// False when the request could not be sent.
bool RequestStack(long threadId);

// Call from a catch block: logs the exception's type and what(), then aborts, so the
// crash report follows.
[[noreturn]] void AbortOnException(const char* where);

} // namespace PortCrash

#endif // METROID_PRIME2_PORT_PORT_CRASH_H
