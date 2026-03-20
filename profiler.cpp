/*  date = March 12th 2026 03:41 PM  */
#include "profiler.h"

#if 0
Profiler::Profiler(const ProfilerOptions& options)
    : mOptions(options)
{
    // worker = std::thread([this](){
    //     process();  // blocking function
    // });
}

Profiler::~Profiler()
{
    mRunning = false;
    if (worker.joinable())
        worker.join();
}

// attach to a running process and mark current process as debugger
void Profiler::attach(DWORD pid)
{
    std::cout << "Attatching to pid " << pid << std::endl;
    if (!DebugActiveProcess(pid))   // windows - enables a debugger to attach to an active process and debug it
    {
        std::cerr << "DebugActiveProcess failed - " << GetLastError() <<  std::endl;
        return;
    }
    DebugSetProcessKillOnExit(FALSE);   // so current thread doesn't terminate all attached process on exit

    mIsAttached = true;
    // process();
}
#endif

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

void Run(Options& options)
{
    timeBeginPeriod(1);

    while (running)
    {
        DEBUG_EVENT ev;

        if (WaitForDebugEvent(&ev, options.sampling_period_in_ms))
        {
            std::cout << "WaitForDebugEvent: ";
            LONG status = DBG_CONTINUE;

            switch (ev.dwDebugEventCode)
            {
            case CREATE_PROCESS_DEBUG_EVENT:
                std::cout << "create process\n";
                break;
            case EXIT_PROCESS_DEBUG_EVENT:
                std::cout << "exit process\n";
                break;
            case CREATE_THREAD_DEBUG_EVENT:
                std::cout << "create thread\n";
                break;
            case EXIT_THREAD_DEBUG_EVENT:
                std::cout << "exit thread\n";
                break;
            case LOAD_DLL_DEBUG_EVENT:
                std::cout << "load dll\n";
                break;
            case UNLOAD_DLL_DEBUG_EVENT:
                std::cout << "unload dll\n";
                break;
            case EXCEPTION_DEBUG_EVENT:
                std::cout << "exception\n";
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
            DWORD err = GetLastError();
            if (err == ERROR_SEM_TIMEOUT)
            {
                
            }
            else
            {
                std::cerr << "WaitForDebugEvent failed - " << err;
            }
        }
    }

    timeEndPeriod(1);
}

int Attach(char const* file_name, DWORD pid, Options& options)
{
    if (!DebugActiveProcess(pid))
    {
        std::cerr << "DebugActiveProcess failed - " << GetLastError();
        return 1;
    }
    DebugSetProcessKillOnExit(FALSE);

    Profiler::Run(options);
    return 0;
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
    

    // int error = Profiler::Attach("sample.sp", opts.pid, opts);
    // if (error)
    //     std::cerr << "could not attach\n";

    // std::cout << "attached!\nrunning..";


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