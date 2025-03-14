#include "notepad.h"

#include <cstdlib>
#include <format>
#include <thread>

#undef ERROR
#define INFO(x) MessageBoxA(nullptr, x, "InbetweenLines - Info", MB_OK)
#define ERROR(x) MessageBoxA(nullptr, std::format("[!] {} ({}, {}:{})", x, __FUNCTION__, __FILE__, __LINE__).c_str(), "InbetweenLines - Error", MB_OK)

using namespace IL; // InbetweenLines implementation file, this is fine

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

	size_t charCount = 165 * 37;
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

	std::this_thread::sleep_for(std::chrono::milliseconds(2000));

	// Wipe out the text buffer
	memset(GetBuffer(), 0, utf16CharCount);
	Flush();

}

void Notepad::Write(const wchar_t* text, size_t length, int x, int y, bool widthEqualsHeight, bool flush) {
    wchar_t* buffer = GetBuffer();
    if (buffer == nullptr) {
        ERROR("Failed to get text buffer address");
        return;
    }

    // Calculate the index to write to
    size_t index = (y * 165) + (widthEqualsHeight ? x * 2 : x);
    if (index + length > 165 * 37) {
        ERROR("Write out of bounds");
        return;
    }

    // Copy the text to the buffer
    wcsncpy_s(&buffer[index], length + 1, text, length);
    if (flush) Flush();
}

void Notepad::Write(const std::wstring_view& text, int x, int y, bool widthEqualsHeight, bool flush) {
    Write(text.data(), text.length(), x, y, widthEqualsHeight, flush);
}

void Notepad::Write(const std::string_view& text, int x, int y, bool widthEqualsHeight, bool flush) {
    Write(std::wstring(text.begin(), text.end()), x, y, widthEqualsHeight, flush);
}

void Notepad::WriteAnimated(const wchar_t* text, size_t length, int x, int y, int baseDelay, int randomDelay, bool widthEqualsHeight, bool flush) {
    wchar_t* buffer = GetBuffer();
    if (buffer == nullptr) {
        ERROR("Failed to get text buffer address");
        return;
    }

    // Calculate the index to write to
    size_t index = (y * 165) + (widthEqualsHeight ? x * 2 : x);
    if (index + length > 165 * 37) {
        ERROR("Write out of bounds");
        return;
    }

    // Copy the text to the buffer
    for (size_t i = 0; i < length; i++) {
        buffer[index + i] = text[i];
        if (flush) Flush();
        std::this_thread::sleep_for(std::chrono::milliseconds(baseDelay + (rand() % randomDelay)));
    }

    if (flush) Flush();
}

void Notepad::WriteAnimated(const std::wstring_view& text, int x, int y, int baseDelay, int randomDelay, bool widthEqualsHeight, bool flush) {
    WriteAnimated(text.data(), text.length(), x, y, baseDelay, randomDelay, widthEqualsHeight, flush);
}

void Notepad::WriteAnimated(const std::string_view& text, int x, int y, int baseDelay, int randomDelay, bool widthEqualsHeight, bool flush) {
    WriteAnimated(std::wstring(text.begin(), text.end()), x, y, baseDelay, randomDelay, widthEqualsHeight, flush);
}

void Notepad::Flush() {
    InvalidateRect(editWnd, nullptr, TRUE);
    UpdateWindow(editWnd);
}

wchar_t* Notepad::GetBuffer() {
	return (wchar_t*)**(uintptr_t**)((uintptr_t)GetModuleHandle(nullptr) + 0x356C0);
}