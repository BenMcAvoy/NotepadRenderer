#include "pch.h"

#include "notepad.h"

using namespace IL; // InbetweenLines implementation file, this is fine

// Keyboard hook procedure implementation
LRESULT CALLBACK Notepad::KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode < 0) {
        return CallNextHookEx(s_keyboardHook, nCode, wParam, lParam);
    }

    // For WH_KEYBOARD, wParam contains the virtual key code
    UINT vkCode = static_cast<UINT>(wParam);

    bool press = (lParam & (1 << 31)) == 0;
    if (press) {
        keysPressed.insert(vkCode);
    }
    else {
        keysPressed.erase(vkCode);
    }

    return 1; // Block the key
}

LRESULT CALLBACK Notepad::EditWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    // Block selection-related messages and text updates
    if (message == WM_PAINT || 
        message == WM_SETTEXT || 
        message == WM_LBUTTONDOWN || 
        message == WM_LBUTTONUP || 
        message == WM_MOUSEMOVE || 
        message == WM_SETCURSOR ||
        message == WM_CHAR ||     // Block character input
        message == WM_KEYDOWN ||  // Block key events
        message == WM_KEYUP) {    // Block key events
        
        if (message == WM_PAINT) {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            
            // Get client area dimensions
            RECT clientRect;
            GetClientRect(hWnd, &clientRect);
            int width = clientRect.right - clientRect.left;
            int height = clientRect.bottom - clientRect.top;
            
            // Create memory DC and bitmap for double buffering
            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP memBitmap = CreateCompatibleBitmap(hdc, width, height);
            HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);
            
            // Fill background once
            HBRUSH whiteBrush = CreateSolidBrush(RGB(255, 255, 255));
            FillRect(memDC, &clientRect, whiteBrush);
            DeleteObject(whiteBrush);
            
            // Get the current Notepad instance
            Notepad* pThis = reinterpret_cast<Notepad*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
            
            if (pThis && pThis->IsValid()) {
                wchar_t* buffer = pThis->GetBuffer();
                
                // Calculate font size
                int fontHeight = height / NOTEPAD_HEIGHT;
                int fontWidth = width / (NOTEPAD_WIDTH + 1);
                
                // Create font with simpler settings
                HFONT hFont = CreateFont(
                    fontHeight, fontWidth > 0 ? fontWidth : 0, 
                    0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                    ANSI_CHARSET,
                    OUT_DEFAULT_PRECIS,
                    CLIP_DEFAULT_PRECIS,
                    DEFAULT_QUALITY,
                    FIXED_PITCH | FF_MODERN,
                    L"Consolas"
                );
                
                HFONT oldFont = (HFONT)SelectObject(memDC, hFont);
                SetTextColor(memDC, RGB(0, 0, 0));
                SetBkMode(memDC, TRANSPARENT);  // Try transparent background
                
                // Draw text line by line
                for (int y = 0; y < NOTEPAD_HEIGHT; y++) {
                    TextOutW(
                        memDC,
                        0, y * fontHeight,
                        &buffer[y * NOTEPAD_WIDTH],
                        NOTEPAD_WIDTH
                    );
                }
                
                SelectObject(memDC, oldFont);
                DeleteObject(hFont);
            }
            
            // Copy to screen
            BitBlt(hdc, 0, 0, width, height, memDC, 0, 0, SRCCOPY);
            
            // Cleanup
            SelectObject(memDC, oldBitmap);
            DeleteObject(memBitmap);
            DeleteDC(memDC);
            
            EndPaint(hWnd, &ps);
            return 0;
        }
        return 0;  // Block other messages
    }
    
    if (message == WM_ERASEBKGND) {
        return 1;  // Prevent background erasing
    }

    return CallWindowProc(oEditWndProc, hWnd, message, wParam, lParam);
}

Notepad::Notepad() {
    //HANDLE hProcess = GetCurrentProcess();
    DWORD pid = GetCurrentProcessId();

     for (HWND curWnd = GetTopWindow(nullptr); curWnd != nullptr; curWnd = GetNextWindow(curWnd, GW_HWNDNEXT)) {
        // Skip windows not in our process
        DWORD wndPid = 0;
        GetWindowThreadProcessId(curWnd, &wndPid);
        if (wndPid != pid) continue;
            
        char wndClass[256] = {0}; // NOSONAR
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
    memset(this->backBuffer.get(), 0, utf16CharCount);
    Flush();
    
    // Install the keyboard hook to prevent user typing
    InstallKeyboardHook();

    // Set the window procedure for the edit control
    oEditWndProc = (WNDPROC)SetWindowLongPtr(editWnd, GWLP_WNDPROC, (LONG_PTR)EditWndProc);
    
    // Store the this pointer so we can access our buffer in the WndProc
    SetWindowLongPtr(editWnd, GWLP_USERDATA, (LONG_PTR)this);
}

Notepad::~Notepad() {
    // Uninstall the keyboard hook before destroying the notepad
    UninstallKeyboardHook();

    // Restore the wndproc for the edit control
    SetWindowLongPtr(editWnd, GWLP_WNDPROC, (LONG_PTR)oEditWndProc);
    
    // Clear the user data pointer
    SetWindowLongPtr(editWnd, GWLP_USERDATA, 0);
    
    SetWindowLong(mainhWnd, GWL_STYLE, GetWindowLong(mainhWnd, GWL_STYLE) | WS_MAXIMIZEBOX | WS_SIZEBOX);
    SetWindowPos(mainhWnd, nullptr, 0, 0, 1365, 768, SWP_NOMOVE | SWP_NOZORDER);
}

bool Notepad::InstallKeyboardHook() const {
    if (s_keyboardHook != NULL) {
        ERROR("Keyboard hook already installed");
        return true;
    }
    
    if (editWnd == nullptr) {
        ERROR("Cannot install hook: Edit window not found");
        return false;
    }
    
    DWORD threadId = GetWindowThreadProcessId(editWnd, NULL);
    if (threadId == 0) {
        ERROR("Failed to get window thread ID");
        return false;
    }
    
    s_keyboardHook = SetWindowsHookEx(WH_KEYBOARD, KeyboardProc, NULL, threadId);
    
    if (s_keyboardHook == NULL) {
        ERROR("SetWindowsHookEx failed with error code: {}", GetLastError());
        return false;
    }

    return true;
}

bool Notepad::UninstallKeyboardHook() const {
    if (s_keyboardHook == NULL) {
        // No hook to uninstall
        return true;
    }
    
    bool result = UnhookWindowsHookEx(s_keyboardHook);
    if (!result) {
        ERROR("UnhookWindowsHookEx failed with error code: {}", GetLastError());
    }
    
    s_keyboardHook = NULL;
    return result;
}

void Notepad::Text(const std::string_view& text, int x, int y, bool widthEqualsHeight) {
    wchar_t* buffer = GetBuffer();
    if (buffer == nullptr) {
        ERROR("Failed to get text buffer address");
        return;
    }

    // Calculate the index to write to
    size_t index = (y * NOTEPAD_WIDTH) + (widthEqualsHeight ? x * 2 : x);
    
    // Convert UTF-8 string to UTF-16 (Windows Unicode)
    std::wstring wtext;
    int requiredSize = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (requiredSize > 0) {
        wtext.resize(requiredSize);
        MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), &wtext[0], requiredSize);
    }
    
    // Check if we're going out of bounds
    if (index + wtext.length() > NOTEPAD_WIDTH * NOTEPAD_HEIGHT) {
        ERROR("Write out of bounds");
        return;
    }

    // Copy the text to the buffer
    memcpy(&this->backBuffer.get()[index], wtext.c_str(), wtext.length() * sizeof(wchar_t));
}

void Notepad::Rectangle(int x, int y, int width, int height, bool fill, bool widthEqualsHeight, wchar_t fillChar) {
    if (widthEqualsHeight) {
        x *= 2;
        width *= 2;
    }

    wchar_t* buffer = GetBuffer();
    if (buffer == nullptr) {
        ERROR("Failed to get text buffer address");
        return;
    }

    // Ensure we don't draw outside the buffer boundaries
    int endX = min(x + width, NOTEPAD_WIDTH);
    int endY = min(y + height, NOTEPAD_HEIGHT);

    // Draw the rectangle ensuring we always have borders
    for (int j = y; j < endY; j++) {
        for (int i = x; i < endX; i++) {
            int index = (j * NOTEPAD_WIDTH) + i;
            if (index < 0 || index >= NOTEPAD_WIDTH * NOTEPAD_HEIGHT) {
                continue;  // Skip if outside buffer
            }
            bool isBorder = (i == x || i == endX - 1 || j == y || j == endY - 1);
            if (fill || isBorder) {
                this->backBuffer.get()[index] = fillChar;
            }
        }
    }
}

void Notepad::Flush() {
    // No-op now, we handle invalidation in End()
}

wchar_t* Notepad::GetBuffer() {
    return (wchar_t*)**(uintptr_t**)((uintptr_t)GetModuleHandle(nullptr) + 0x356C0);
}

void Notepad::Begin() {
    // Clear the back buffer completely - note that Begin() is no longer const
    if (backBuffer) {
        size_t bufferSize = NOTEPAD_WIDTH * NOTEPAD_HEIGHT * sizeof(wchar_t);
        ZeroMemory(backBuffer.get(), bufferSize);
    }
}

void Notepad::End(int targetFPS) {
    // Ensure we have valid buffers
    wchar_t* frontBuffer = GetBuffer();
    if (!frontBuffer || !backBuffer) {
        ERROR("Invalid buffer pointers");
        return;
    }
    
    // Target frame time in microseconds
    static const long long targetFrameTime = 1000000 / targetFPS;
    static auto lastFrameTime = std::chrono::high_resolution_clock::now();
    
    // Swap buffers in one atomic operation
    size_t bufferSize = NOTEPAD_WIDTH * NOTEPAD_HEIGHT * sizeof(wchar_t);
    memcpy(frontBuffer, backBuffer.get(), bufferSize);
    
    // Only invalidate once, don't call UpdateWindow
    if (editWnd) {
        InvalidateRect(editWnd, nullptr, FALSE);
    }
    
    // Frame timing code...
    auto now = std::chrono::high_resolution_clock::now();
    auto elapsedTime = std::chrono::duration_cast<std::chrono::microseconds>(
        now - lastFrameTime).count();
    
    long long sleepTime = targetFrameTime - elapsedTime;
    if (sleepTime > 0) {
        auto sleepUntil = now + std::chrono::microseconds(sleepTime);
        while (std::chrono::high_resolution_clock::now() < sleepUntil) {
            std::this_thread::yield();
        }
    }
    
    lastFrameTime = std::chrono::high_resolution_clock::now();
}

bool Notepad::IsValid() const {
    return (editWnd != nullptr && backBuffer != nullptr && GetBuffer() != nullptr);
}