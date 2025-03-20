#include "pch.h"

#include "notepad.h"

static HANDLE hThread = nullptr;
static std::atomic<bool> running = true;

// Main thread function
DWORD WINAPI MainThread(LPVOID lpParam) {
    IL::Notepad notepad;

    // Map the `badapple.txt` file to memory (fast read-only access)
    //HANDLE hFile = CreateFileA("C:\\Users\\Public\\badapple.txt", GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    HANDLE hFile = CreateFileA("C:\\Users\\Public\\Mickey.txt", GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        ERROR("Failed to open file");
        return 1;
    }

    HANDLE hMap = CreateFileMapping(hFile, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (hMap == nullptr) {
        ERROR("Failed to create file mapping");
        CloseHandle(hFile);
        return 1;
    }

    char* fileData = (char*)MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0);
    if (fileData == nullptr) {
        ERROR("Failed to map view of file");
        CloseHandle(hMap);
        CloseHandle(hFile);
        return 1;
    }

    // Get the file size
    DWORD fileSize = GetFileSize(hFile, nullptr);
    if (fileSize == INVALID_FILE_SIZE) {
        ERROR("Failed to get file size");
        UnmapViewOfFile(fileData);
        CloseHandle(hMap);
        CloseHandle(hFile);
        return 1;
    }

    // Read the file into a string
    std::string fileView(fileData, fileSize);

    // Close the file handle
    UnmapViewOfFile(fileData);
    CloseHandle(hMap);
    CloseHandle(hFile);

    // Count the number of lines first, we will resize the vector later
    size_t lineCount = 1;
    for (size_t i = 0; i < fileView.size(); ++i)
        if (fileView[i] == '\n')
            lineCount++;

    // Preallocate the vector of frames, each new line is a new frame
    std::vector<std::string_view> frames;
    frames.reserve(lineCount);

    // Extract each line as a frame
    size_t pos = 0;
    while (pos < fileView.size()) {
        size_t end = fileView.find('\n', pos);
        if (end == std::string::npos)
            end = fileView.size();

        // Handle carriage return if present (for Windows-style line endings \r\n)
        size_t lineLength = end - pos;
        if (end > pos && fileView[end - 1] == '\r') {
            lineLength--;
        }

        frames.emplace_back(fileView.data() + pos, lineLength);
        pos = end + 1;
    }

    // Main loop
    bool autoPlay = true;
    size_t currentFrame = 0;
    
    while (running) {
        auto& keysPressed = IL::Notepad::GetKeysPressed();
        
        // Handle keyboard input
        if (keysPressed.contains(IL::KEY_LEFT)) {
            // Go to previous frame
            if (currentFrame > 0) {
                currentFrame--;
            }
            autoPlay = false;
            keysPressed.erase(IL::KEY_LEFT);
        }
        
        if (keysPressed.contains(IL::KEY_RIGHT)) {
            // Go to next frame
            if (currentFrame < frames.size() - 1) {
                currentFrame++;
            }
            autoPlay = false;
            keysPressed.erase(IL::KEY_RIGHT);
        }
        
        if (keysPressed.contains(IL::KEY_UP)) {
            // Fast forward 5 frames
            if (currentFrame + 5 < frames.size()) {
                currentFrame += 5;
            } else {
                currentFrame = frames.size() - 1;
            }
            autoPlay = false;
            keysPressed.erase(IL::KEY_UP);
        }
        
        if (keysPressed.contains(IL::KEY_DOWN)) {
            // Rewind 5 frames
            if (currentFrame >= 5) {
                currentFrame -= 5;
            } else {
                currentFrame = 0;
            }
            autoPlay = false;
            keysPressed.erase(IL::KEY_DOWN);
        }
        
        if (keysPressed.contains(IL::KEY_SPACE)) {
            // Toggle autoplay
            autoPlay = !autoPlay;
            keysPressed.erase(IL::KEY_SPACE);
        }
        
        // Display the current frame
        notepad.Begin();
        notepad.Text(0, 0, frames[currentFrame]);
        
        // Display frame info and controls
        notepad.Text(1, 1, "Frame {} / {} ({:.2f}%)", currentFrame, frames.size() - 1, (currentFrame / static_cast<float>(frames.size() - 1)) * 100);
        
        // Improved FPS counter using high-resolution clock
        static int frameCount = 0;
        static auto lastFpsUpdate = std::chrono::high_resolution_clock::now();
        frameCount++;
        
        // Calculate FPS every 500ms for smoother display
        static int fps = 0;
        auto now = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFpsUpdate).count();
        
        if (elapsed >= 500) {  // Update twice per second for smoother readings
            fps = static_cast<int>((frameCount * 1000.0) / elapsed);
            frameCount = 0;
            lastFpsUpdate = now;
        }
        
        // Display the fps and controls
        notepad.Text(1, 2, "FPS: {} (Target: 30)", fps);
        notepad.Text(1, 3, std::string("Controls: ← Previous | → Next | ↑ +5 frames | ↓ -5 frames | Space: {}"),
                     autoPlay ? "Pause" : "Play");
        
        // If in autoplay mode, advance to next frame
        if (autoPlay) {
            if (currentFrame < frames.size() - 1) {
                currentFrame++;
            } else {
                currentFrame = 0; // Loop back to the beginning
            }
        }
        
        // You can change this number to adjust the target FPS
        notepad.End(30);
    }
    
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hModule);
            hThread = CreateThread(nullptr, 0, MainThread, nullptr, 0, nullptr);
            break;
        case DLL_PROCESS_DETACH:
            // Signal the thread to exit.
            running = false;
            if (hThread) {
                // Wait briefly (e.g. 100ms) for the thread to exit.
                WaitForSingleObject(hThread, 100);
                CloseHandle(hThread);
                hThread = nullptr;
            }
            break;
    }

    return TRUE;
}
