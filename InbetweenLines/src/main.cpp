#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <iostream>
#include <thread>
#include <format>

// Terminal color definitions
#define GREEN_TEXT "\033[32m"
#define RED_TEXT "\033[91m"
#define RESET_TEXT "\033[0m"

// Log helpers
#undef ERROR
/*#define INFO(x) std::cout << GREEN_TEXT << "[+] " << x << RESET_TEXT << std::endl
#define ERROR(x) std::cerr << RED_TEXT << "[!] " << x << " (" << __FUNCTION__ << " | " __FILE__ << ":" << __LINE__ << ")"  << RESET_TEXT << std::endl*/

#define INFO(x) MessageBoxA(nullptr, x, "InbetweenLines - Info", MB_OK)
#define ERROR(x) MessageBoxA(nullptr, std::format("[!] {} ({}, {}:{})", x, __FUNCTION__, __FILE__, __LINE__).c_str(), "InbetweenLines - Error", MB_OK)

[[nodiscard]] __forceinline uintptr_t GetTextBuffer() noexcept {
	return **(uintptr_t**)((uintptr_t)GetModuleHandle(nullptr) + 0x356C0);
}

HWND getNotepadHWND() {
    DWORD currentPid = GetCurrentProcessId();
    for (HWND curWnd = GetTopWindow(nullptr); curWnd != nullptr; curWnd = GetNextWindow(curWnd, GW_HWNDNEXT)) {
        // Skip windows not in our process
        DWORD wndPid = 0;
        GetWindowThreadProcessId(curWnd, &wndPid);
        if (wndPid != currentPid) continue;
            
        char wndClass[256] = {0};
        GetClassNameA(curWnd, wndClass, 256);
        if (strcmp(wndClass, "Notepad") != 0) continue;

		return curWnd;
    }

	return nullptr;
}

void RefreshNotepadEditControls() {
    auto notepadWnd = getNotepadHWND();

    if (HWND editWnd = FindWindowEx(notepadWnd, nullptr, L"Edit", nullptr)) {
        InvalidateRect(editWnd, nullptr, TRUE);
        UpdateWindow(editWnd);
    }
}

void SetupWindow() {
	auto notepadWnd = getNotepadHWND();
	if (notepadWnd == nullptr) {
		ERROR("Failed to get notepad window handle");
		return;
	}

	// Firstly we need to resize the notepad window, disable resizing and maximizing and stuff
	SetWindowLong(notepadWnd, GWL_STYLE, GetWindowLong(notepadWnd, GWL_STYLE) & ~WS_MAXIMIZEBOX & ~WS_SIZEBOX);
	SetWindowPos(notepadWnd, nullptr, 0, 0, 1365, 768, SWP_NOMOVE | SWP_NOZORDER);

	// Next, we need to get the handle of the edit control
	HWND editWnd = FindWindowEx(notepadWnd, nullptr, L"Edit", nullptr);
	if (editWnd == nullptr) {
		ERROR("Failed to get notepad edit control handle");
		return;
	}

	// Then we need to generate some garbage text to fill the edit control with
	size_t charCount = 165 * 37; // 1365x768
	size_t utf16CharCount = charCount * 2;

	size_t currentBufferSize = SendMessage(editWnd, WM_GETTEXTLENGTH, 0, 0);
	if (currentBufferSize >= charCount) {
		// Clear out the text buffer of its junk
		memset((wchar_t*)GetTextBuffer(), 0, utf16CharCount);
		RefreshNotepadEditControls();
		return;
	}

	char* frameBuffer = (char*)malloc(utf16CharCount);
	for (int i = 0; i < charCount; i++) {
		char v = 0x41 + (rand() % 26);
		PostMessage(editWnd, WM_CHAR, v, 0);
		frameBuffer[i * 2] = v;
		frameBuffer[i * 2 + 1] = 0x00;
	}

	// Query whether we're done with the typing
	size_t textLength = SendMessage(editWnd, WM_GETTEXTLENGTH, 0, 0);
	while (textLength < charCount) {
		textLength = SendMessage(editWnd, WM_GETTEXTLENGTH, 0, 0);
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	// Wipe out the text buffer
	memset((wchar_t*)GetTextBuffer(), 0, utf16CharCount);
	RefreshNotepadEditControls();
}

void Write(const std::wstring& msg) {
	uintptr_t addr = GetTextBuffer();
	if (addr == 0) {
		ERROR("Failed to get text buffer address");
		return;
	}

	wchar_t* buffer = (wchar_t*)addr;
	wcscpy_s(buffer, msg.size() + 1, msg.c_str());

	RefreshNotepadEditControls();

	if (addr == 0) {
		ERROR("Failed to get text buffer address");
		return;
	}
}

void AnimatedWrite(const std::wstring& msg, int baseDelayMs = 1, int randomDelayMs = 1) {
	uintptr_t addr = GetTextBuffer();
	if (addr == 0) {
		ERROR("Failed to get text buffer address");
		return;
	}

	wchar_t* buffer = (wchar_t*)addr;
	for (size_t i = 0; i < msg.size(); i++) {
		buffer[i] = msg[i];
		std::this_thread::sleep_for(std::chrono::milliseconds(baseDelayMs + (rand() % randomDelayMs)));
		RefreshNotepadEditControls();
	}

	if (addr == 0) {
		ERROR("Failed to get text buffer address");
		return;
	}
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);

		SetupWindow();
		AnimatedWrite(L"Do we really need game engines?", 0, 100);
		AnimatedWrite(L"                               ", 0, 10);
		AnimatedWrite(L"Yeah... I didn't think so either.", 0, 100);
		AnimatedWrite(L"                                   ", 0, 10);
		AnimatedWrite(L"Welcome to the one and only Notepad", 0, 100);
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		AnimatedWrite(L"                                   ", 0, 10);
		AnimatedWrite(L"GAME ENGINE EDITION", 0, 100);
    }

    return TRUE;
}
