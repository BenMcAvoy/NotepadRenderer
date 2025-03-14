#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <string>

namespace IL {
    class Notepad {
    public:
        Notepad();

        /// @brief Writes text to the notepad window
        /// @param text The text to write
        /// @param length The length of the text
        /// @param x The x position to write the text
        /// @param y The y position to write the text
        /// @param widthEqualsHeight Whether the width of x index should be the same as the height of y index (default: true)
        /// @param flush Whether to flush the text buffer to the notepad window automatically (default: true)
        void Write(const wchar_t* text, size_t length, int x, int y, bool widthEqualsHeight = true, bool flush = true);

        /// @brief Writes text to the notepad window
        /// @param text The text to write
        /// @param x The x position to write the text
        /// @param y The y position to write the text
        /// @param widthEqualsHeight Whether the width of x index should be the same as the height of y index (default: true)
        /// @param flush Whether to flush the text buffer to the notepad window automatically (default: true)
        void Write(const std::wstring_view& text, int x, int y, bool widthEqualsHeight = true, bool flush = true);

        /// @brief Writes text to the notepad window
        /// @param text The text to write
        /// @param x The x position to write the text
        /// @param y The y position to write the text
        /// @param widthEqualsHeight Whether the width of x index should be the same as the height of y index (default: true)
        /// @param flush Whether to flush the text buffer to the notepad window automatically (default: true)
        void Write(const std::string_view& text, int x, int y, bool widthEqualsHeight = true, bool flush = true);

        /// @brief Writes text to the notepad window
        /// @param text The text to write
        /// @param length The length of the text
        /// @param x The x position to write the text
        /// @param y The y position to write the text
        /// @param baseDelay The base delay between each character (default: 1)
        /// @param randomDelay The random delay to add to the base delay (default: 10)
        /// @param widthEqualsHeight Whether the width of x index should be the same as the height of y index (default: true)
        /// @param flush Whether to flush the text buffer to the notepad window automatically (default: true)
        void WriteAnimated(const wchar_t* text, size_t length, int x, int y, int baseDelay = 1, int randomDelay = 10, bool widthEqualsHeight = true, bool flush = true);

        /// @brief Writes text to the notepad window
        /// @param text The text to write
        /// @param x The x position to write the text
        /// @param y The y position to write the text
        /// @param baseDelay The base delay between each character (default: 1)
        /// @param randomDelay The random delay to add to the base delay (default: 10)
        /// @param widthEqualsHeight Whether the width of x index should be the same as the height of y index (default: true)
        /// @param flush Whether to flush the text buffer to the notepad window automatically (default: true)
        void WriteAnimated(const std::wstring_view& text, int x, int y, int baseDelay = 1, int randomDelay = 10, bool widthEqualsHeight = true, bool flush = true);

        /// @brief Writes text to the notepad window
        /// @param text The text to write
        /// @param x The x position to write the text
        /// @param y The y position to write the text
        /// @param baseDelay The base delay between each character (default: 1)
        /// @param randomDelay The random delay to add to the base delay (default: 10)
        /// @param widthEqualsHeight Whether the width of x index should be the same as the height of y index (default: true)
        /// @param flush Whether to flush the text buffer to the notepad window automatically (default: true)
        void WriteAnimated(const std::string_view& text, int x, int y, int baseDelay = 1, int randomDelay = 10, bool widthEqualsHeight = true, bool flush = true);

        /// @brief Flushes the text buffer to the notepad window
        void Flush();

        /// @brief Gets the text buffer address
        wchar_t* GetBuffer();

    private:
        HWND mainhWnd = nullptr;
        HWND editWnd = nullptr;
    };
}