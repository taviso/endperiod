#define WIN32_LEAN_AND_MEAN
#define WIN32_NO_STATUS
#include <stdbool.h>
#include <windows.h>
#include <winternl.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#undef WIN32_NO_STATUS
#include <ntstatus.h>

#include "winutil.h"
#include "getopt.h"
#include "logging.h"

//
// This utility attempts to force a call to timeEndPeriod() in another process.
//
// The reason you would want to do that is if a process is keeping the timer
// resolution high and interfering with power management.
//
// - You can use the utility clockres from sysinternals to see the current
//   timer resolution.
// - You can use the utility pwrtest /timer from the WDK to figure out which
//   process is messing with it.
// - You can use this utility to force a process to end a long-running period
//   without killing it. This *might* break the process, or it might work fine
//   and help your battery. YMMV.
//
// Tavis Ormandy, Jul 2025 <taviso@gmail.com>

static void print_usage(char *name)
{
    LogMsg("%s [-p <pid> | -n <image>] [-t period]", name);
    LogMsg("Attempt to end a timerPeriod in a process interfering with power management");
    LogMsg("    -p pid      : Attach to the specified process id");
    LogMsg("    -n image    : Attach to first matching process (e.g. notepad.exe)");
    LogMsg("    -t period   : Period to end (optiona, I can guess if not specified)");
    LogMsg("Note: You must use the 64bit version if the target process is 64bit");
}

int main(int argc, char **argv)
{
    DWORD ProcessId;
    FARPROC Address;
    DWORD Period;
    DWORD Result;
    int opt;

    _setmode(0, _O_BINARY);
    _setmode(1, _O_BINARY);
    SetConsoleOutputCP(CP_UTF8);

    // Reset Options
    Result = ProcessId = Period = 0;

    // Parse Commandline
    while ((opt = getopt(argc, argv, "p:n:t:h")) != -1) {
        switch (opt) {
            case 'p': ProcessId = atoi(optarg);
                      break;
            case 'n': FindProcessImage(optarg, &ProcessId, 1);
                      break;
            case 't': Period = atoi(optarg);
                      break;
            case 'h':
                print_usage(*argv);
                return 0;
            default:
                print_usage(*argv);
                return 1;
        }
    }

    if (!ProcessId) {
        LogMessage("No process specified, or process not found");
        return 1;
    }

    if (!Period) {
        ULONG MinimumResolution;
        ULONG MaximumResolution;
        ULONG CurrentResolution;

        NtQueryTimerResolution(&MinimumResolution,
                               &MaximumResolution,
                               &CurrentResolution);

        if (CurrentResolution == MinimumResolution) {
            LogMessage("The period is already optimal, not doing anything");
            return 0;
        }

        // Timer uses miliseconds
        Period = CurrentResolution / 10000;

        LogMessage("No period specified, current period is %lu, I'll use that", Period);
    }

    // Look up timeEndPeriod()
    if ((Address = GetProcAddress(LoadLibrary("WINMM"), "timeEndPeriod")) == NULL) {
        LogErr("failed to find timeEndPeriod Address");
        return 1;
    }

    // Call it in the specified PID
    if (RemoteCallOneLongParam(ProcessId, Address, Period, &Result) == false) {
        LogErr("could not call remote procedure, maybe permission or error or 64/32 mismatch");
        return 1;
    }

    fprintf(stderr, "Success (result=%d), run clockres to see if it worked\n", Result);
    return 0;
}
