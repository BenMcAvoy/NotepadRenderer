#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <atomic>
#include <cstdlib> // For rand()
#include <ctime>   // For time()

#include "notepad.h"

// Global variables
static std::atomic<bool> running = true;
static HANDLE hThread = nullptr;

struct Vector2 {
    int x = 0;
    int y = 0;
};

struct Physics_t {
    float velocityY = 0.0f;
    const float gravity = 0.5f;
    const float jumpForce = -4.0f;
    bool isOnGround = false;
    const int groundLevel = 32;
    const float terminalVelocity = 5.0f;  // Maximum falling speed
};

struct State_t {
    Vector2 position;
    Physics_t physics;
    int blinkTimer = 0;      // Timer for controlling eye blinks
    bool isBlinking = false; // Whether eyes are currently blinking
} state;

// Function to render the player with blinking eyes
void RenderPlayer(IL::Notepad& notepad, State_t& state) {
    // Render player body
    notepad.Rectangle(state.position.x, state.position.y, 5, 5, false);
    
    // Update blinking logic
    state.blinkTimer++;
    
    // Randomly start blinking every ~2 seconds (120 frames)
    if (state.blinkTimer >= 120) {
        state.blinkTimer = 0;
        // 70% chance to blink
        state.isBlinking = (rand() % 100) < 70;
    }
    
    // Stop blinking after 10 frames
    if (state.isBlinking && state.blinkTimer > 10) {
        state.isBlinking = false;
    }
    
    // Draw the eyes
    if (state.isBlinking) {
        notepad.Text(state.position.x + 1, state.position.y + 1, " -  -"); // Closed eyes
    } else {
        notepad.Text(state.position.x + 1, state.position.y + 1, " O  O"); // Open eyes
    }

	// Draw the mouth
	notepad.Text(state.position.x + 1, state.position.y + 3, " ~~~~");
}

// Main thread function
DWORD WINAPI MainThread(LPVOID lpParam) {
    IL::Notepad notepad;
    
    // Seed random number generator
    srand(static_cast<unsigned int>(time(nullptr)));
    
    while (running.load()) {
        // Process keyboard input
        for (const auto key : notepad.GetKeysPressed()) {
            switch (key) {
                case IL::KEY_A:
                    state.position.x--;
                    break;
                case IL::KEY_D:
                    state.position.x++;
                    break;
                case IL::KEY_SPACE:
                    // Only allow jumping when on the ground
                    if (state.physics.isOnGround) {
                        state.physics.velocityY = state.physics.jumpForce;
                        state.physics.isOnGround = false;
                    }
                    break;
                case IL::KEY_ESCAPE:
                    break;
            }
        }
        
        // Apply gravity and update position
        state.physics.velocityY += state.physics.gravity;
        
        // Apply terminal velocity
        if (state.physics.velocityY > state.physics.terminalVelocity) {
            state.physics.velocityY = state.physics.terminalVelocity;
        }
        
        state.position.y += static_cast<int>(state.physics.velocityY);
        
        // Check for ground collision
        if (state.position.y >= state.physics.groundLevel) {
            state.position.y = state.physics.groundLevel;
            state.physics.velocityY = 0;
            state.physics.isOnGround = true;
        }
        
        // Render
        notepad.Begin();
        RenderPlayer(notepad, state); // Call the new render function
        notepad.End();
        
        Sleep(16); // ~60fps
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
