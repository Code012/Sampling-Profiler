/*  date = March 12th 2026 03:41 PM  */

/*
TODOs:

Now:
- Sample instruction pointer count and display

Later:
- Symbol resolution
- Fix exception debug event, so profiler works with exception code.
*/

#include "profiler.h"


namespace Profiler
{


// --[command] : signify toggleable values
// -[flag] [value]
Options OptionsFromArgV(LPWSTR* argv, int argc)
{
    Options result{};
    
    if (wcscmp(argv[1], L"attach") == 0)
        result.command = Command::Attach;

    else if (wcscmp(argv[1], L"launch") == 0)
        result.command = Command::Launch;

    for (int i{2}; i < argc; ++i)
    {
        // std::wcout << L"i:" << i << ", " << argv[i] << L"\n";

        // FLAGS (-flag value)
        if (wcscmp(argv[i], L"-period") == 0 && i+1 < argc)
        {
            result.sampling_period_in_ms = _wtoi(argv[++i]);

        }

        // TOGGLE FLAGS (--flag)


        // pid
        else if (result.command == Command::Attach && argv[i][0] != L'-')
        {
            result.pid = _wtoi(argv[i]);
        }
    }


    return result;
}

int Attach(char const* file_name, DWORD pid, Options& options)
{

    State state{};
    state.running = true;
    state.process_id = pid;

    if (!DebugActiveProcess(pid))
    {
        std::cerr << "DebugActiveProcess failed - " << GetLastError();
        return 1;
    }
    DebugSetProcessKillOnExit(FALSE);

    Profiler::Run(state, options);
    return 0;
}

void CreateProcessDebugEvent(State& state, DWORD pid, DWORD thread_id, CREATE_PROCESS_DEBUG_INFO const* info)
{
    std::cout << "Process attached, pid=" << pid << ", tid=" << thread_id  << "\n";

    state.threads.insert({thread_id, info->hThread});
    state.call_stack_index.insert({thread_id, static_cast<uint32_t>(state.call_stack.size())});
    state.call_stack.push_back(ThreadCallStack());
    state.thread_count = 1;             // main thread init

    state.process_id = pid;
    state.process_handle = info->hProcess;
    state.process_base = (DWORD64)info->lpBaseOfImage;
    IsWow64Process(state.process_handle, (PBOOL)&state.is_wow64);

    // symbol server .. 
}
void ExitProcessDebugEvent(State& state, DWORD thread_id, EXIT_PROCESS_DEBUG_INFO const* info)
{
    std::cout << "Process finished, exit code " << info->dwExitCode << "\n";
    // symbol clean up..

    state.threads.erase(thread_id);
    state.call_stack_index.erase(thread_id);
    state.process_handle = nullptr;
    --state.thread_count;
}


void CreateThreadDebugEvent(State& state, DWORD thread_id, CREATE_THREAD_DEBUG_INFO const* info)
{
    std::cout << "Thread started, tid=" << thread_id << "\n";
    state.threads.insert({thread_id, info->hThread});
    state.call_stack_index.insert({thread_id, static_cast<uint32_t>(state.call_stack.size())});
    state.call_stack.push_back(ThreadCallStack());
    ++state.thread_count;
}
void ExitThreadDebugEvent(State& state, DWORD thread_id, EXIT_THREAD_DEBUG_INFO const* info)
{
    std::cout << "Thread finished, tid=" << thread_id << ", exit code " << info->dwExitCode << "\n";
    state.threads.erase(thread_id);
    state.call_stack_index.erase(thread_id);
    --state.thread_count;
}

void Run(State& state, Options& options)
{
    timeBeginPeriod(1);

    while (state.running)
    {
        DEBUG_EVENT ev;

        if (WaitForDebugEvent(&ev, options.sampling_period_in_ms))
        {
            std::cout << "WaitForDebugEvent: ";
            LONG status = DBG_CONTINUE;

            switch (ev.dwDebugEventCode)
            {
            // Info(sb): for loading symbols, and registering the main thread for the attached process 
            // this will only be called once because of the DEBUG_ONLY_THIS_PROCESS process creation flag
            // DebugActiveProcess mimics DEBUG_ONLY_THIS_PROCESS
            // not called on child processes
            case CREATE_PROCESS_DEBUG_EVENT:
                // TODO(sb): do symbol resolution
                state.symbols_initialised = true;   
                CreateProcessDebugEvent(state, ev.dwProcessId, ev.dwThreadId, &ev.u.CreateProcessInfo);
                std::cout << "create process, proocess_id: " << ev.dwProcessId << ", thread_id: " << ev.dwThreadId  << "\n";
                break;
            case EXIT_PROCESS_DEBUG_EVENT:
                // ExitProcessDebugEvent(state, ev.dwThreadId, &ev.u.ExitProcess);
                std::cout << "exit process\n";
                break;
            case CREATE_THREAD_DEBUG_EVENT:
                if (ev.u.CreateThread.hThread != nullptr)
                {
                    CreateThreadDebugEvent(state, ev.dwThreadId, &ev.u.CreateThread);
                }
                break;
            case EXIT_THREAD_DEBUG_EVENT:
                ExitThreadDebugEvent(state, ev.dwThreadId, &ev.u.ExitThread);
                break;
            case LOAD_DLL_DEBUG_EVENT:
                std::cout << "load dll\n";
                break;
            case UNLOAD_DLL_DEBUG_EVENT:
                std::cout << "unload dll\n";
                break;
            // TODO(sb): Martins code doesn't seem like it works properly with exceptions,
            // see what you can do, you mentioned it in the discord
            case EXCEPTION_DEBUG_EVENT:
                std::cout << "exception\n";
                if (!ev.u.Exception.dwFirstChance)
                {
                    status = DBG_EXCEPTION_NOT_HANDLED;
                }
                break;
            case OUTPUT_DEBUG_STRING_EVENT:
                std::cout << "debug string\n";
                break;
            }

            if (!ContinueDebugEvent(ev.dwProcessId, ev.dwThreadId, status))
            {
                std::cerr << "ContinueDebugEvent failed - " << GetLastError();
            }
        }
        else
        {
            // std::cout << state.process_handle;
            DWORD err = GetLastError();
            if (err == ERROR_SEM_TIMEOUT)
            {
                // if (state.process_handle && state.symbols_initialised)  // Note(sb): symbol resolution not done yet, but its been set to true
                {
                    Sample(state);
                }
            }
            else
            {
                std::cerr << "WaitForDebugEvent failed - " << err;
            }
        }
    }

    /*
    Clean up for when you do symbol resolution:
    if (state.process_handle)
    {
        SysCleanup(state.process_handle)
        state.process_handle = nullptr;
    }
    */

    timeEndPeriod(1);
}

void Sample(State& state)
{
    /*
    for each thread:
        SuspendThread()
        GetThreadContext()
        StackWalk64()
        Resolve Symbols (todo)
        Store stack
        ResumeThread()
    */

    // YOU WERE HERE:
    // look up STACKFRAME64 struct
    // WOW64 is compatibillity layer for 32 bit processes to run on 64 bit OS
    // this is just initialisation for stack walk
    // ip tells you which function thread is executing
    // bp is current stack frame's base or start, local variables are relative to this, bp is stable when inside a function
    // sp is top of stack, changes
     //https://claude.ai/chat/155a6995-4be0-4817-9267-a625ec2043d7

    for (auto& [thread_id, thread_handle] : state.threads) // structured bindings, C++17
    {
        if (SuspendThread(thread_handle) == -1)
        {
            std::cout << "Thread suspend failed\n"; 
            continue;
        }

        union
        {
            WOW64_CONTEXT ctx32;
            CONTEXT ctx64;
        };
        PVOID ctx{nullptr};
        DWORD machine;

        STACKFRAME64 frame = {};
        frame.AddrPC.Mode = AddrModeFlat;   // default
        frame.AddrFrame.Mode = AddrModeFlat;
        frame.AddrStack.Mode = AddrModeFlat;

        if (state.is_wow64)
        {
            machine = IMAGE_FILE_MACHINE_I386;
            // WOW64_CONTEXT_CONTROL flag gives: Eip, Esp, Ebp, EFlags, SegCs, SegSs
            // WOW64_CONTEXT_INTEGER flag gives: Eax, Ebx, Ecx, Edx, Esi, Edi (I don't think these are needed for stack walking) 
            // important ones are Ebp to follow frame pointer chain (linked list), Eip is current address, Esp for frameless functions using unwind info
            ctx32.ContextFlags = WOW64_CONTEXT_CONTROL | WOW64_CONTEXT_INTEGER; 
            if (Wow64GetThreadContext(thread_handle, &ctx32))
            {
                // Note(sb): Offset if misleading here, they are absolute virtual addresses. Offset comes from 16-bit legacy where in segemnted memory, address = segment + offset. 
                frame.AddrPC.Offset = ctx32.Eip;
                frame.AddrFrame.Offset = ctx32.Ebp;
                frame.AddrStack.Offset = ctx32.Esp;
                ctx = &ctx32;
            }
        }
        else
        {
            machine = IMAGE_FILE_MACHINE_AMD64;
            // CONTEXT_CONTROL flag gives: Rip, Rsp, Rbp, EFlags, SegCs, SegSs
            // CONTEXT_INTEGER flag gives: Rax, Rbx, Rcx, Rdx, Rsi, Rdi, R8-R15 (again, don't think these are needed for stack walking)
            // important ones are Rbp to follow frame pointer chain (linked list), Rip is current address, Rsp for frameless functions using unwind info
            ctx64.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
            if (GetThreadContext(thread_handle, &ctx64))
            {
                // Note(sb): Offset if misleading here, they are absolute virtual addresses. Offset comes from 16-bit legacy where in segemnted memory, address = segment + offset. 
                frame.AddrPC.Offset = ctx64.Rip;
                frame.AddrFrame.Offset = ctx64.Rbp;
                frame.AddrStack.Offset = ctx64.Rsp;
                ctx = &ctx64;
            }
        }

        if (!ctx)
        {
            std::cerr << "GetThreadContext failed - " + GetLastError() << "\n";
        }
        else
        {
            // stackwalk
            ThreadCallStack& callstack = state.call_stack[state.call_stack_index.at(thread_id)];
            DWORD64 last_stack{};

            while (StackWalk64(machine, state.process_handle, thread_handle, &frame, ctx, nullptr,  // updates frame in place each iteration, filling in next frame's AddrPC, AddrFrame, AddrStack etc.
                SymFunctionTableAccess64, SymGetModuleBase64, nullptr)) // defaults provided by Dbghelp.dll: SymFunctionTableAccess64 gets unwind info for a given address (for frameless funcitons) and SymGetModuleBase64 gets the base address of the module containing an address (to find the right unwind table)
            {
                if (frame.AddrPC.Offset == 0 
                    || (frame.AddrStack.Offset <= last_stack) // stack grows downwards, so each frame's stack pointer must be hiehger than the last. <= catched going backwards and duplicate frames (equal) 
                    || (frame.AddrStack.Offset % (state.is_wow64 ? sizeof(uint32_t) : sizeof(uint64_t))) != 0)  // granularity check, stack must be aligned
                {
                    break;
                }
                last_stack = frame.AddrStack.Offset;

                uint64_t address = frame.AddrPC.Offset;

                CallStackEntry entry;
                // TODO(sb): symbol lookup
                entry.symbol = std::make_shared<Symbol>();
                entry.symbol->address = address;
                callstack.push_back(entry);

            }
            callstack.push_back(CallStackEntry());
            ++state.sampled_count;
        }

        ResumeThread(thread_handle);

    }
}

// [thread count]  [[thread call stack size][thread call stack addresses][thread call stack size][thread call stack addresses]]..
void SerialiseCallStacks(State& state)
{
    std::ofstream f("sample.sp", std::ios::binary); // hardcoded file name for now
    uint32_t thread_count = state.call_stack.size();
    f.write((char*)&thread_count, sizeof(thread_count)); // thread count

    for (ThreadCallStack& callstack : state.call_stack)
    {
        uint32_t entry_count = callstack.size();
        f.write((char*)&entry_count, sizeof(entry_count)); // entry count
        for (CallStackEntry& entry : callstack)
        {
            uint64_t address = entry.symbol->address;
            f.write((char*)address, sizeof(address));   // instructio pointer
        }
    }
}


void LoadFlattenedData()
{
    uint32_t total_count = CreateProfile(....);

    // display
}

} // namespace Profiler



// TODO(sb): error codes, and wrap whatever is in main in another function so it can be used as a library
int main() 
{
    // TODO(sb): command line parsing and integration
    // profiler launch program.exe -freq 100000 -out sample.sp -duration 20000
    // profiler attach 1234 ...

    int argc{};
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);

    Profiler::Options opts = Profiler::OptionsFromArgV(argv, argc);
    
    std::cout << "sampling period (ms): " << opts.sampling_period_in_ms << "\npid: " << opts.pid << std::endl;
    
    int error = Profiler::Attach("sample.sp", opts.pid, opts);
    if (error)
        std::cerr << "could not attach\n";

    SerialiseCallStacks();
    // YOU WERE HERE:
    // you serialised callstacks to a file
    // load them into FlatSymbolCounts and FlatThread, calculating aggregrates
    // then display

    LoadFlattenedData();


    


    // clean up    
    LocalFree(argv);

    /*

    std::thread worker;
    std::atomic<bool> running{true};


    profile main.exe -args
    process cmd args, and create process using those args
    
    create process main.exe with args
    get process id of main.exe

    Profiler::Options options{100};
    int error = Profiler::Run("trace.sp", pid, &options);
    if (error)
    {
        std::cout  << StringFromError(error);
        return 1;
    }
    */
	
	return 0;
}