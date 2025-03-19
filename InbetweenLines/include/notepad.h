#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <string>
#include <memory>
#include <format>
#include <unordered_set>

namespace IL {
    constexpr int NOTEPAD_WIDTH = 165;
    constexpr int NOTEPAD_HEIGHT = 38;

    constexpr UINT KEY_UP = 0x26;
    constexpr UINT KEY_DOWN = 0x28;
    constexpr UINT KEY_LEFT = 0x25;
    constexpr UINT KEY_RIGHT = 0x27;
    constexpr UINT KEY_W = 0x57;
    constexpr UINT KEY_A = 0x41;
    constexpr UINT KEY_S = 0x53;
    constexpr UINT KEY_D = 0x44;
    constexpr UINT KEY_SPACE = 0x20;
    constexpr UINT KEY_ENTER = 0x0D;
    constexpr UINT KEY_ESCAPE = 0x1B;

    class Notepad {
    public:
        Notepad();
        ~Notepad();

        /// @brief Writes text to the notepad window
        /// @param text The text to write
        /// @param x The x position to write the text
        /// @param y The y position to write the text
        /// @param widthEqualsHeight Whether the width of x index should be the same as the height of y index (default: true)
        template<typename... Args>
        void Text(int x, int y, const std::string_view& fmt, Args... args) {
            Text(std::vformat(fmt, std::make_format_args(args...)), x, y);
        }
        template<typename... Args>
        void Text(int x, int y, bool widthEqualsHeight, const std::string_view& fmt, Args... args) {
            Text(std::vformat(fmt, std::make_format_args(args...)), x, y, widthEqualsHeight);
        }
        void Text(const std::string_view& text, int x, int y, bool widthEqualsHeight = true);

        /// @brief Draws a rectangle to the notepad window
        /// @param x The x position to draw the rectangle
        /// @param y The y position to draw the rectangle
        /// @param width The width of the rectangle
        /// @param height The height of the rectangle
        /// @param fill Whether to fill the rectangle (default: false)
        /// @param widthEqualsHeight Whether the width of x index should be the same as the height of y index (default: true)
        void Rectangle(int x, int y, int width, int height, bool fill = false, bool widthEqualsHeight = true, wchar_t fillChar = L'\u2588');

        /// @brief Begins writing to the notepad window
        void Begin();

        /// @brief Ends writing to the notepad window and flushes the text buffer
        /// @param targetFPS The target frames per second to wait for before flushing the text buffer (default: 60)
        void End(int targetFPS = 60);

        /// @brief Flushes the text buffer to the notepad window
        void Flush();

        /// @brief Gets the text buffer address
        static wchar_t* GetBuffer();

        /// @brief Installs a keyboard hook to prevent typing in the notepad
        bool InstallKeyboardHook() const;
        
        /// @brief Uninstalls the keyboard hook
        bool UninstallKeyboardHook() const;

        /// @brief Returns a reference to the keys currently pressed
        static std::unordered_set<UINT>& GetKeysPressed() { return keysPressed; }

        /// @brief Checks if the notepad is valid
        bool IsValid() const;
    private:
        HWND mainhWnd = nullptr;
        HWND editWnd = nullptr;

        std::shared_ptr<wchar_t> backBuffer = std::shared_ptr<wchar_t>(new wchar_t[NOTEPAD_WIDTH * NOTEPAD_HEIGHT * 2], std::default_delete<wchar_t[]>());
        
        // Static hook handle and procedure
        static LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
        static inline HHOOK s_keyboardHook = nullptr;

        static inline std::unordered_set<UINT> keysPressed = {};

        static inline WNDPROC oEditWndProc = nullptr;

        static LRESULT CALLBACK EditWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
    };
}