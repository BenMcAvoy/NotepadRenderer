#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <atomic>
#include <cstdlib> // For rand()
#include <ctime>   // For time()
#include <vector>  // For storing platforms
#include <string>  // For std::to_string
#include <algorithm> // For std::remove_if

#include "notepad.h"

// Global variables
static std::atomic<bool> running = true;
static HANDLE hThread = nullptr;

// Screen boundaries
constexpr int SCREEN_WIDTH = 80;  // Typical Notepad width in characters
constexpr int SCREEN_HEIGHT = 35; // Typical Notepad height in lines
constexpr int PLAYER_WIDTH = 5;   // Width of the player
constexpr int PLAYER_HEIGHT = 5;  // Height of the player

void SpawnCoin();

struct Vector2 {
    int x = 0;
    int y = 0;
};

struct Platform {
    int x, y, width, height;
};

// Define a structure for coins with lifetime tracking
struct Coin {
    int x, y;         // Position
    bool active;      // Whether the coin is currently visible
    int lifetime;     // How long the coin has existed (in frames)
    bool exploding;   // Whether the coin is currently exploding
    int explosionFrame; // Current frame of explosion animation
};

// Define a structure for explosion animation
struct Explosion {
    int x, y;           // Position
    int currentFrame;   // Current animation frame
    static const int totalFrames = 5; // Total frames in the explosion animation
    bool active;        // Whether the explosion is still active
};

struct Physics_t {
    float velocityY = 0.0f;
    const float gravity = 0.5f;
    const float jumpForce = -4.0f;
    bool isOnGround = false;
    const int groundLevel = 33;
    const float terminalVelocity = 5.0f;  // Maximum falling speed
};

struct State_t {
    Vector2 position = {SCREEN_WIDTH / 2 - PLAYER_WIDTH / 2, 0};
    Physics_t physics;
    int blinkTimer = 0;      // Timer for controlling eye blinks
    bool isBlinking = false; // Whether eyes are currently blinking
    std::vector<Platform> platforms; // Platforms to jump between
    std::vector<Coin> coins; // Collectable coins
    int score = 0;           // Player's score
    int coinSpawnTimer = 0;  // Timer for spawning new coins
    const int coinSpawnInterval = 20; // Spawn check every ~2 seconds at 60fps
    const int maxCoinsOnScreen = 10;  // Maximum number of coins allowed at once
    std::vector<Explosion> explosions; // Active explosions
    const int coinLifetime = 200;      // Coin lifetime in frames (10 seconds at 60fps)
    
    // Animation state for squash and stretch
    bool isMovingHorizontal = false;   // Is player currently moving horizontally
    int lastMoveDirection = 0;         // Last movement direction (-1 left, 1 right, 0 none)
    int moveFrames = 0;                // Counter for tracking movement duration
    
    // Current animation dimensions (for collision detection)
    int currentWidth = PLAYER_WIDTH;
    int currentHeight = PLAYER_HEIGHT;
    int xOffset = 0;
    int yOffset = 0;
} state;

// Function to render the player with blinking eyes
void RenderPlayer(IL::Notepad& notepad, State_t& state) {
    // Calculate animation parameters based on movement
    int widthModifier = 0;
    int heightModifier = 0;
    int eyeSpacing = 3;  // Default spacing between eyes
    
    // Squash when moving horizontally
    if (state.isMovingHorizontal) {
        widthModifier = -1;  // Make character wider
        heightModifier = 1;  // Make character shorter
        eyeSpacing = 4;      // Eyes further apart when squashed
    }
    
    // Stretch when jumping or falling
    if (!state.physics.isOnGround) {
        if (state.physics.velocityY < 0) {
            // Stretching upward during jump
            widthModifier = 1;  // Make character narrower
            heightModifier = -1; // Make character taller
            eyeSpacing = 2;     // Eyes closer together when stretched
        } else if (state.physics.velocityY > 2.0f) {
            // Stretching downward during fall (only when falling fast)
            widthModifier = 1;   // Make character narrower
            heightModifier = -2; // Make character even taller during fall
            eyeSpacing = 2;      // Eyes closer together when stretched
        }
    }
    
    // Limit the animation effect
    if (widthModifier < -1) widthModifier = -1;
    if (heightModifier < -2) heightModifier = -2;
    if (widthModifier > 1) widthModifier = 1;
    if (heightModifier > 1) heightModifier = 1;
    
    // Calculate adjusted dimensions
    int adjustedWidth = PLAYER_WIDTH - widthModifier;
    int adjustedHeight = PLAYER_HEIGHT - heightModifier;
    
    // Calculate the horizontal offset to center the character
    int xOffset = (PLAYER_WIDTH - adjustedWidth) / 2;
    
    // Calculate the vertical offset - key change here:
    // When squashing (heightModifier > 0), keep the bottom aligned
    // When stretching (heightModifier < 0), center the stretch
    int yOffset = 0;
    if (heightModifier > 0) {
        // For squash: maintain bottom position
        yOffset = heightModifier; // Push down from the top
    } else if (heightModifier < 0) {
        // For stretch: center the stretch effect
        yOffset = (PLAYER_HEIGHT - adjustedHeight) / 2;
    }
    
    // Store current dimensions and offsets for collision detection
    state.currentWidth = adjustedWidth;
    state.currentHeight = adjustedHeight;
    state.xOffset = xOffset;
    state.yOffset = yOffset;
    
    // Render player body with adjusted dimensions
    notepad.Rectangle(state.position.x + xOffset, state.position.y + yOffset, 
                     adjustedWidth, adjustedHeight, false, "#");
    
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
    
    // Adjust eye position based on squash/stretch
    int eyeYPosition = state.position.y + yOffset + 1;
    
    // Draw the eyes with adjusted spacing
    if (state.isBlinking) {
        // For blinking eyes, create a string with proper spacing
        std::string eyeText = " -" + std::string(eyeSpacing - 2, ' ') + "-";
        notepad.Text(state.position.x + xOffset + 1, eyeYPosition, eyeText);
    } else {
        // For open eyes, create a string with proper spacing
        std::string eyeText = " O" + std::string(eyeSpacing - 2, ' ') + "O";
        notepad.Text(state.position.x + xOffset + 1, eyeYPosition, eyeText);
    }

    // Draw the mouth (adjusted for squash/stretch)
    int mouthYPosition = state.position.y + yOffset + adjustedHeight - 2;
    int mouthWidth = adjustedWidth - 2;
    std::string mouthText = " " + std::string(mouthWidth - 1, '~');
    notepad.Text(state.position.x + xOffset + 1, mouthYPosition, mouthText);
}

// Function to render platforms
void RenderPlatforms(IL::Notepad& notepad, const std::vector<Platform>& platforms) {
    for (const auto& platform : platforms) {
        // draw with block character
        notepad.Rectangle(platform.x, platform.y, platform.width, platform.height, true);
    }
}

// Function to render coins with degradation based on lifetime
void RenderCoins(IL::Notepad& notepad, std::vector<Coin>& coins, const int maxLifetime) {
    for (auto& coin : coins) {
        if (coin.active) {
            // Calculate the degradation stage based on lifetime
            float lifePercentage = static_cast<float>(coin.lifetime) / maxLifetime;
            
            // Choose symbol based on degradation stage
            std::string coinSymbol;
            if (lifePercentage < 0.25f) {
                coinSymbol = "O"; // Fresh coin
            } else if (lifePercentage < 0.5f) {
                coinSymbol = "0"; // Slightly degraded
            } else if (lifePercentage < 0.75f) {
                coinSymbol = "o"; // More degraded
            } else {
                coinSymbol = "."; // Almost gone
            }
            
            notepad.Text(coin.x, coin.y, coinSymbol);
        }
    }
}

// Function to render explosions
void RenderExplosions(IL::Notepad& notepad, std::vector<Explosion>& explosions) {
    static const std::string explosionFrames[Explosion::totalFrames] = {
        "*",    // Frame 1
        "+",    // Frame 2
        "#",    // Frame 3
        "+",    // Frame 4
        "."     // Frame 5
    };
    
    for (auto& explosion : explosions) {
        if (explosion.active && explosion.currentFrame < Explosion::totalFrames) {
            // Render current explosion frame
            notepad.Text(explosion.x - 1, explosion.y - 1, explosionFrames[explosion.currentFrame]);
            notepad.Text(explosion.x, explosion.y - 1, explosionFrames[explosion.currentFrame]);
            notepad.Text(explosion.x + 1, explosion.y - 1, explosionFrames[explosion.currentFrame]);
            notepad.Text(explosion.x - 1, explosion.y, explosionFrames[explosion.currentFrame]);
            notepad.Text(explosion.x, explosion.y, explosionFrames[explosion.currentFrame]);
            notepad.Text(explosion.x + 1, explosion.y, explosionFrames[explosion.currentFrame]);
            notepad.Text(explosion.x - 1, explosion.y + 1, explosionFrames[explosion.currentFrame]);
            notepad.Text(explosion.x, explosion.y + 1, explosionFrames[explosion.currentFrame]);
            notepad.Text(explosion.x + 1, explosion.y + 1, explosionFrames[explosion.currentFrame]);
        }
    }
}

// Start an explosion at the given coordinates
void StartExplosion(int x, int y) {
    Explosion explosion;
    explosion.x = x;
    explosion.y = y;
    explosion.currentFrame = 0;
    explosion.active = true;
    state.explosions.push_back(explosion);
}

// Update explosions (advance animation frames)
void UpdateExplosions() {
    for (auto& explosion : state.explosions) {
        if (explosion.active) {
            explosion.currentFrame++;
            if (explosion.currentFrame >= Explosion::totalFrames) {
                explosion.active = false;
            }
        }
    }
    
    // Remove completed explosions
    state.explosions.erase(
        std::remove_if(state.explosions.begin(), state.explosions.end(),
            [](const Explosion& e) { return !e.active; }),
        state.explosions.end()
    );
}

// Initialize platforms with a more balanced layout
void InitializePlatforms() {
    // Clear existing platforms
    state.platforms.clear();
    
    // Ground level platforms (y=25)
    state.platforms.push_back({12, 25, 15, 1});
    state.platforms.push_back({45, 25, 15, 1});
    
    // Mid-level platforms (y=18)
    state.platforms.push_back({5, 18, 10, 1});
    state.platforms.push_back({30, 18, 15, 1});
    state.platforms.push_back({60, 18, 12, 1});
    
    // Higher level platforms (y=12)
    state.platforms.push_back({20, 12, 10, 1});
    state.platforms.push_back({45, 12, 14, 1});
    
    // Top level platforms (y=6)
    state.platforms.push_back({35, 6, 15, 1});
}

// Initialize coins
void InitializeCoins() {
    // Clear existing coins
    state.coins.clear();
    // Start with a few coins
    SpawnCoin();
    SpawnCoin();
}

// Improved function to check if player collides with any platform
bool CheckPlatformCollision() {
    bool wasOnGround = state.physics.isOnGround;
    
    // First, assume we're not on the ground unless we detect a collision
    if (state.position.y < state.physics.groundLevel) {
        state.physics.isOnGround = false;
    }
    
    // Calculate the actual player bounds based on current animation state
    int playerLeft = state.position.x + state.xOffset;
    int playerRight = playerLeft + state.currentWidth;
    int playerBottom = state.position.y + state.yOffset + state.currentHeight;
    
    for (const auto& platform : state.platforms) {
        // Check if player's bottom edge is near the platform's top edge
        // AND player is within the horizontal bounds of the platform
        if (state.physics.velocityY > 0 && 
            playerBottom >= platform.y - 1 &&  // More forgiving collision (-1)
            playerBottom <= platform.y + 2 &&  // More forgiving collision (+2)
            playerRight > platform.x && 
            playerLeft < platform.x + platform.width) {
            
            // Player landed on this platform
            state.position.y = platform.y - state.currentHeight - state.yOffset;  // Position player on top of platform
            state.physics.velocityY = 0;
            state.physics.isOnGround = true;
            return true;
        }
        
        // Check if we're no longer on this platform
        if (wasOnGround && 
            (playerRight <= platform.x || 
             playerLeft >= platform.x + platform.width)) {
            // We might have walked off the platform
            // This will be handled by gravity in the next frame
        }
    }
    
    return state.physics.isOnGround;
}

// Check if player collects any coins
void CheckCoinCollection() {
    // Use animated dimensions for coin collection detection
    int playerLeft = state.position.x + state.xOffset;
    int playerRight = playerLeft + state.currentWidth;
    int playerTop = state.position.y + state.yOffset;
    int playerBottom = playerTop + state.currentHeight;
    
    for (auto& coin : state.coins) {
        if (coin.active && 
            playerLeft < coin.x + 1 && playerRight > coin.x &&
            playerTop < coin.y + 1 && playerBottom > coin.y) {
            // Coin collected
            coin.active = false;
            state.score += 10;
        }
    }
    
    // Remove inactive coins
    state.coins.erase(
        std::remove_if(state.coins.begin(), state.coins.end(), 
            [](const Coin& coin) { return !coin.active; }),
        state.coins.end()
    );
}

// Spawn a new coin at a random position
void SpawnCoin() {
    // Don't spawn more coins if we've hit the maximum
    if (state.coins.size() >= state.maxCoinsOnScreen) {
        return;
    }

    Coin coin;
    coin.x = rand() % (SCREEN_WIDTH - 3); // Avoid spawning right at the edge
    
    // 50% chance to spawn on a platform, 50% chance to spawn in air
    if (rand() % 2 == 0 && !state.platforms.empty()) {
        // Choose a random platform
        const auto& platform = state.platforms[rand() % state.platforms.size()];
        // Place the coin right above the platform
        coin.x = platform.x + (rand() % (platform.width - 1));
        coin.y = platform.y - 2;
    } else {
        // Random position in air
        coin.y = (rand() % (state.physics.groundLevel - 5)) + 2;  // Avoid spawning too high or too low
    }
    
    coin.active = true;
    coin.lifetime = 0;
    coin.exploding = false;
    coin.explosionFrame = 0;
    state.coins.push_back(coin);
}

// Update coins (lifetime and degradation)
void UpdateCoins() {
    for (auto& coin : state.coins) {
        if (coin.active) {
            coin.lifetime++;
            
            // Check if coin should expire
            if (coin.lifetime >= state.coinLifetime) {
                // Start an explosion at this coin's position
                StartExplosion(coin.x, coin.y);
                coin.active = false;
            }
        }
    }
    
    // Remove inactive coins
    state.coins.erase(
        std::remove_if(state.coins.begin(), state.coins.end(), 
            [](const Coin& coin) { return !coin.active; }),
        state.coins.end()
    );
}

// Main thread function
DWORD WINAPI MainThread(LPVOID lpParam) {
    IL::Notepad notepad;
    
    // Seed random number generator
    srand(static_cast<unsigned int>(time(nullptr)));
    
    // Initialize platforms and coins
    InitializePlatforms();
    InitializeCoins();
    
    while (running.load()) {
        // Process keyboard input
        for (auto key : notepad.GetKeysPressed()) {
            switch (key) {
                case IL::KEY_A:
                    if (state.position.x > 0) { // Prevent moving past left boundary
                        state.position.x--;
                        state.isMovingHorizontal = true;
                        state.lastMoveDirection = -1;
                        state.moveFrames = 10; // Animation will last for 10 frames
                    }
                    break;
                case IL::KEY_D:
                    if (state.position.x < SCREEN_WIDTH - PLAYER_WIDTH) { // Prevent moving past right boundary
                        state.position.x++;
                        state.isMovingHorizontal = true;
                        state.lastMoveDirection = 1;
                        state.moveFrames = 10; // Animation will last for 10 frames
                    }
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
        
        // Update Y position
        state.position.y += static_cast<int>(state.physics.velocityY);
        
        // Enforce screen top boundary
        if (state.position.y < 0) {
            state.position.y = 0;
            state.physics.velocityY = 0; // Stop upward movement if hitting the ceiling
        }
        
        // Check for platform collision first
        bool onPlatform = CheckPlatformCollision();
        
        // Check for ground collision only if not on platform
        if (!onPlatform) {
            // Calculate the actual bottom of the player based on animation
            int playerBottom = state.position.y + state.yOffset + state.currentHeight;
            
            if (playerBottom >= state.physics.groundLevel) {
                // Adjust position based on current height and offset
                state.position.y = state.physics.groundLevel - state.currentHeight - state.yOffset;
                state.physics.velocityY = 0;
                state.physics.isOnGround = true;
            }
        }
        
        // Make sure player can't go below ground level and stays within screen boundaries
        if (state.position.y > state.physics.groundLevel) {
            state.position.y = state.physics.groundLevel;
        }
        
        // Enforce side boundaries (in case other code moves the player)
        if (state.position.x < 0) {
            state.position.x = 0;
		}
		else if (state.position.x > SCREEN_WIDTH) {
			state.position.x = SCREEN_WIDTH;
        }
        
        // Check for coin collection
        CheckCoinCollection();
        
        // Spawn new coins more reliably
        state.coinSpawnTimer++;
        if (state.coinSpawnTimer >= state.coinSpawnInterval) { 
            state.coinSpawnTimer = 0;
            // Increased chance to spawn a coin (75%)
            if (rand() % 4 < 3) {
                SpawnCoin();
            }
        }
        
        // Update coins (lifetime and degradation)
        UpdateCoins();
        
        // Update explosions
        UpdateExplosions();
        
        // Update animation state
        if (state.moveFrames > 0) {
            state.moveFrames--;
            if (state.moveFrames == 0) {
                state.isMovingHorizontal = false;
            }
        }
        
        // Render
        notepad.Begin();
        
        // Display score and active coin count in top-left
        notepad.Text(1, 1, "Score: " + std::to_string(state.score) + 
                          " Coins: " + std::to_string(state.coins.size()) + 
                          " Time till next coin (s): " + std::to_string((state.coinSpawnInterval - state.coinSpawnTimer) / 60));
        
        RenderPlatforms(notepad, state.platforms);  // Render platforms
        RenderCoins(notepad, state.coins, state.coinLifetime);  // Render coins with degradation
        RenderExplosions(notepad, state.explosions);  // Render explosions
        RenderPlayer(notepad, state);               // Render player

		notepad.Text(1, IL::NOTEPAD_HEIGHT - 2, "By Ben McAvoy (https://github.com/BenMcAvoy)");
        notepad.Text(1, IL::NOTEPAD_HEIGHT - 1, "Press A and D to move, SPACE to jump. Collect coins before they explode!");
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
