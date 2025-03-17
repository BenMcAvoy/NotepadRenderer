#include "notepad.h"

#include <cstdlib>
#include <format>
#include <thread>

#undef ERROR
#define INFO(x) MessageBoxA(nullptr, x, "InbetweenLines - Info", MB_OK)
#define ERROR(x) MessageBoxA(nullptr, std::format("[!] {} ({}, {}:{})", x, __FUNCTION__, __FILE__, __LINE__).c_str(), "InbetweenLines - Error", MB_OK)

using namespace IL; // InbetweenLines implementation file, this is fine

LRESULT CALLBACK KeyboardProc(int code, WPARAM wParam, LPARAM lParam) {
    return 1;
}

Notepad::Notepad() {
    HANDLE hProcess = GetCurrentProcess();
    DWORD pid = GetCurrentProcessId();

     for (HWND curWnd = GetTopWindow(nullptr); curWnd != nullptr; curWnd = GetNextWindow(curWnd, GW_HWNDNEXT)) {
        // Skip windows not in our process
        DWORD wndPid = 0;
        GetWindowThreadProcessId(curWnd, &wndPid);
        if (wndPid != pid) continue;
            
        char wndClass[256] = {0};
        GetClassNameA(curWnd, wndClass, 256);
        if (strcmp(wndClass, "Notepad") != 0) continue;

        mainhWnd = curWnd;
        editWnd = FindWindowEx(mainhWnd, nullptr, L"Edit", nullptr);
        if (editWnd == nullptr) {
            ERROR("Failed to get notepad edit control handle");
            return;
        }

        break;
    }

    if (mainhWnd == nullptr) {
        ERROR("Failed to get notepad window handle");
        return;
    }

    SetWindowLong(mainhWnd, GWL_STYLE, GetWindowLong(mainhWnd, GWL_STYLE) & ~WS_MAXIMIZEBOX & ~WS_SIZEBOX);
    SetWindowPos(mainhWnd, nullptr, 0, 0, 1365, 768, SWP_NOMOVE | SWP_NOZORDER);

	size_t charCount = NOTEPAD_WIDTH * NOTEPAD_HEIGHT;
	size_t utf16CharCount = charCount * 2;

	size_t currentBufferSize = SendMessage(editWnd, WM_GETTEXTLENGTH, 0, 0);
	if (currentBufferSize >= charCount) {
		memset(GetBuffer(), 0, utf16CharCount);
		Flush();
		return;
	}

	for (int i = 0; i < charCount; i++) {
        const wchar_t* msg = L"THE_END_IS_NEVER_THE_";
        wchar_t v = msg[i % wcslen(msg)];
		PostMessage(editWnd, WM_CHAR, v, 0);
	}

	// Query whether we're done with the typing
	size_t textLength = SendMessage(editWnd, WM_GETTEXTLENGTH, 0, 0);
	while (textLength < charCount) {
		textLength = SendMessage(editWnd, WM_GETTEXTLENGTH, 0, 0);
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	std::this_thread::sleep_for(std::chrono::milliseconds(200));

	// Wipe out the text buffer
	memset(GetBuffer(), 0, utf16CharCount);
    memset(backBuffer, 0, utf16CharCount);
	Flush();
}

Notepad::~Notepad() {
    SetWindowLong(mainhWnd, GWL_STYLE, GetWindowLong(mainhWnd, GWL_STYLE) | WS_MAXIMIZEBOX | WS_SIZEBOX);
    SetWindowPos(mainhWnd, nullptr, 0, 0, 1365, 768, SWP_NOMOVE | SWP_NOZORDER);
}

void Notepad::Text(const std::string_view& text, int x, int y, bool widthEqualsHeight) {
    const size_t length = text.length();

    wchar_t* buffer = GetBuffer();
    if (buffer == nullptr) {
        ERROR("Failed to get text buffer address");
        return;
    }

    // Calculate the index to write to
    size_t index = (y * NOTEPAD_WIDTH) + (widthEqualsHeight ? x * 2 : x);
    if (index + length > NOTEPAD_WIDTH * NOTEPAD_HEIGHT) {
        ERROR("Write out of bounds");
        return;
    }

    // Copy the text to the buffer
    std::wstring wtext(text.begin(), text.end());
    wcsncpy_s(&this->backBuffer[index], length + 1, wtext.c_str(), length);
}

void Notepad::Flush() {
    InvalidateRect(editWnd, nullptr, TRUE);
    UpdateWindow(editWnd);
}

wchar_t* Notepad::GetBuffer() {
	return (wchar_t*)**(uintptr_t**)((uintptr_t)GetModuleHandle(nullptr) + 0x356C0);
}

void Notepad::Begin() {
    memset(backBuffer, 0, sizeof(backBuffer));
}

void Notepad::End() {
    wchar_t* buffer = GetBuffer();
    if (buffer == nullptr) {
        ERROR("Failed to get text buffer address");
        return;
    }

    static std::chrono::steady_clock::time_point lastTime = std::chrono::steady_clock::now();
    static const int fps = 30;
    auto currentTime = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastTime).count();
    int delay = 1000 / fps - elapsed;
    if (delay > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(delay));
    }
    
    lastTime = std::chrono::steady_clock::now();

    memcpy(buffer, backBuffer, sizeof(backBuffer));
    Flush();
}