/*  date = March 12th 2026 03:41 PM  */
#pragma once


#include <stdint.h>
#include <windows.h>
#include <shellapi.h>
#include <timeapi.h>
#include <debugapi.h>

#include <wchar.h>
#include <iostream>
#include <atomic>
#include <thread>
#include <string>
#include <memory>
#include <vector>

/*
1. Either attach to or spawn the process you want to process 
2. Pause the program
3. Retrieve and save the value of the rip register
4. Resume program
5. Repeat 2-4 until program terminates
6. Aggregrate rip values

Resolve rip to symbol names.
Display rip count of functions in ascending order.
Unwind call stack to display heriarchical relationship.
*/

/*
TODOs(sb):

1. Implement minimal debugger loop
proces()
WaitForDebugEvent
ContinueDebugEvent

while (running)
{
    WaitForDebugEvent()
    print event type
    ContinueDebugEvent()
}



sample function:
for each thread in mThreads
    SuspendThread()
    GetThreadContext()
    StackWalk64()
    Resolve symbols
    Store stack
    ResumeThread()

sample()
  ↓
for (thread in mThreads)
  ↓
SuspendThread(thread)
  ↓
GetThreadContext(thread)
  ↓
StackWalk64() loop
       ↓
lookupSymbol()
       ↓
lookupLine()
       ↓
append CallStackEntry
  ↓
ResumeThread(thread)
*/

////////////////////////////////
//- Symbols
struct Symbol
{
    std::string name;
    std::string file;
    uint64_t address;
    uint32_t size;
    uint32_t line;
    uint32_t lineLast;

    std::string module;
};

typedef std::shared_ptr<Symbol> SymbolPtr;


namespace Profiler
{



////////////////////////////////
//- Constants

inline bool running = true;

enum class Command
{
    None = 0,
    Attach,
    Launch,
};

struct Options
{
    Command command;
    uint32_t sampling_period_in_ms; // >= 1ms (<= 1000Hz) and 1ms granularity 
    DWORD pid;
    // bool captureDebugOutputString;
    // bool downloadSymbols;
};

// loaded dll or exe
struct Module
{
    HANDLE handle;
    std::string name;
    uint64_t address;
    uint32_t size;
};

struct CallStackEntry
{
    SymbolPtr symbol;   // resolved function name
    uint32_t line{};    // source line
    uint32_t offset{};  // PC offset from symbol start
};

// TODO(sb): this is for a single threaded program for now, 
// look at how to make it work for multithreaded programs
typedef std::vector<CallStackEntry> ThreadCallStack;    // all stack frames collected for a thread across samples
typedef std::vector<ThreadCallStack> CallStack;         // std::vector<std::vector<CallStackEntry>> CallStack

enum class Error : uint32_t
{
    None,
    AttachFailed,
};


	
int Attach(char const* file_name, DWORD pid, Options& options);
int Launch(char const* file_name, Options& options); // check if file_name or cmd need to be char* or char const* based on waht windows functions uses, them. Sometimes the Unicode version of functions require string editing, so const will return access violation
#if 0
this is for Launch(), use this when you're writing Launch()
// Create process (launch)
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    if (!CreateProcessW(nullptr, argv[1], nullptr, nullptr, FALSE, DEBUG_PROCESS | DEBUG_ONLY_THIS_PROCESS, nullptr, nullptr, &si, &pi))  
    {
        std::cerr << "CreateProcess Failed - " << GetLastError() << "\n";
    }
    DebugSetProcessKillOnExit(TRUE);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

#endif

void Run(Options& options);

Options OptionsFromArgV(LPWSTR* argv);
} // namespace Profiler