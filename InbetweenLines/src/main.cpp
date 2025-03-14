#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <thread>

#include "notepad.h"

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
		IL::Notepad notepad;

		notepad.WriteAnimated("Welcome to...", 1, 1, 10, 200);
		std::this_thread::sleep_for(std::chrono::milliseconds(200));
		notepad.WriteAnimated("InbetweenLines", 16, 1, 10, 200, false);
		notepad.WriteAnimated("...", 30, 1, 100, 200, false);
    }

    return TRUE;
}
