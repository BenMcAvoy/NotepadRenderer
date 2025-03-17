#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <string>

extern "C" {
    // Block the user from typing in the notepad window
	__declspec(dllexport) LRESULT CALLBACK KeyboardProc(int code, WPARAM wParam, LPARAM lParam);
}

namespace IL {
    constexpr int NOTEPAD_WIDTH = 165;
    constexpr int NOTEPAD_HEIGHT = 37;

    class Notepad {
    public:
        Notepad();
        ~Notepad();

        /// @brief Writes text to the notepad window
        /// @param text The text to write
        /// @param x The x position to write the text
        /// @param y The y position to write the text
        /// @param widthEqualsHeight Whether the width of x index should be the same as the height of y index (default: true)
        void Text(const std::string_view& text, int x, int y, bool widthEqualsHeight = true);

        /// @brief Begins writing to the notepad window
        void Begin();

        /// @brief Ends writing to the notepad window and flushes the text buffer
        void End();

        /// @brief Flushes the text buffer to the notepad window
        void Flush();

        /// @brief Gets the text buffer address
        wchar_t* GetBuffer();

    private:
        HWND mainhWnd = nullptr;
        HWND editWnd = nullptr;

        wchar_t backBuffer[NOTEPAD_WIDTH * NOTEPAD_HEIGHT * 2];
    };
}