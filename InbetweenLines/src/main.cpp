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
    int value;        // Value of the coin (added for multiplayer)
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

// New Player struct for multiplayer
struct Player {
    Vector2 position = {0, 0};
    Physics_t physics;
    int blinkTimer = 0;         // Timer for controlling eye blinks
    bool isBlinking = false;    // Whether eyes are currently blinking
    bool isMovingHorizontal = false;  // Is player currently moving horizontally
    int lastMoveDirection = 0;  // Last movement direction (-1 left, 1 right, 0 none)
    int moveFrames = 0;         // Counter for tracking movement duration
    int currentWidth = PLAYER_WIDTH;  // Current animation dimensions
    int currentHeight = PLAYER_HEIGHT;
    int xOffset = 0;
    int yOffset = 0;
    int score = 0;              // Player's score
    
    // Player color/theme
    std::string eyeColor = "O";
    std::string mouthChar = "~";
};

struct State_t {
    Player players[2];   // Two players: 0=left (WASD), 1=right (arrows)
    std::vector<Platform> platforms; // Platforms to jump between
    std::vector<Coin> coins; // Collectable coins
    int coinSpawnTimer = 0;  // Timer for spawning new coins
    const int coinSpawnInterval = 20; // Spawn check every ~2 seconds at 60fps
    const int maxCoinsOnScreen = 10;  // Maximum number of coins allowed at once
    std::vector<Explosion> explosions; // Active explosions
    const int coinLifetime = 200;      // Coin lifetime in frames (10 seconds at 60fps)
} state;

// Function to render a player with blinking eyes
void RenderPlayer(IL::Notepad& notepad, Player& player, int playerIndex) {
    // Calculate animation parameters based on movement
    int widthModifier = 0;
    int heightModifier = 0;
    int eyeSpacing = 3;  // Default spacing between eyes
    
    // Squash when moving horizontally
    if (player.isMovingHorizontal) {
        widthModifier = -1;  // Make character wider
        heightModifier = 1;  // Make character shorter
        eyeSpacing = 4;      // Eyes further apart when squashed
    }
    
    // Stretch when jumping or falling
    if (!player.physics.isOnGround) {
        if (player.physics.velocityY < 0) {
            // Stretching upward during jump
            widthModifier = 1;  // Make character narrower
            heightModifier = -1; // Make character taller
            eyeSpacing = 2;     // Eyes closer together when stretched
        } else if (player.physics.velocityY > 2.0f) {
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
    
    // Calculate the vertical offset
    int yOffset = 0;
    if (heightModifier > 0) {
        // For squash: maintain bottom position
        yOffset = heightModifier; // Push down from the top
    } else if (heightModifier < 0) {
        // For stretch: center the stretch effect
        yOffset = (PLAYER_HEIGHT - adjustedHeight) / 2;
    }
    
    // Store current dimensions and offsets for collision detection
    player.currentWidth = adjustedWidth;
    player.currentHeight = adjustedHeight;
    player.xOffset = xOffset;
    player.yOffset = yOffset;
    
    // Different character shape or color for each player
    std::string borderChar = (playerIndex == 0) ? "#" : "@";
    
    // Render player body with adjusted dimensions
    notepad.Rectangle(player.position.x + xOffset, player.position.y + yOffset, 
                     adjustedWidth, adjustedHeight, false, true, borderChar[0]);
    
    // Update blinking logic
    player.blinkTimer++;
    
    // Randomly start blinking every ~2 seconds (120 frames)
    if (player.blinkTimer >= 120) {
        player.blinkTimer = 0;
        // 70% chance to blink
        player.isBlinking = (rand() % 100) < 70;
    }
    
    // Stop blinking after 10 frames
    if (player.isBlinking && player.blinkTimer > 10) {
        player.isBlinking = false;
    }
    
    // Adjust eye position based on squash/stretch
    int eyeYPosition = player.position.y + yOffset + 1;
    
    // Draw the eyes with adjusted spacing
    if (player.isBlinking) {
        // For blinking eyes, create a string with proper spacing
        std::string eyeText = " -" + std::string(eyeSpacing - 2, ' ') + "-";
        notepad.Text(player.position.x + xOffset + 1, eyeYPosition, eyeText);
    } else {
        // For open eyes, create a string with proper spacing
        std::string eyeText = " " + player.eyeColor + std::string(eyeSpacing - 2, ' ') + player.eyeColor;
        notepad.Text(player.position.x + xOffset + 1, eyeYPosition, eyeText);
    }

    // Draw the mouth (adjusted for squash/stretch)
    int mouthYPosition = player.position.y + yOffset + adjustedHeight - 2;
    int mouthWidth = adjustedWidth - 2;
    std::string mouthText = " " + player.mouthChar + std::string(mouthWidth - 2, ' ') + player.mouthChar;
    notepad.Text(player.position.x + xOffset + 1, mouthYPosition, mouthText);
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

// Initialize players
void InitializePlayers() {
    // Left player (WASD)
    state.players[0].position = {SCREEN_WIDTH / 4 - PLAYER_WIDTH / 2, 0};
    state.players[0].eyeColor = "O";
    state.players[0].mouthChar = '~';
    state.players[0].score = 0;
    
    // Right player (Arrow keys)
    state.players[1].position = {(SCREEN_WIDTH * 3) / 4 - PLAYER_WIDTH / 2, 0};
    state.players[1].eyeColor = "X";
    state.players[1].mouthChar = '-';
    state.players[1].score = 0;
}

// Improved function to check if player collides with any platform
bool CheckPlatformCollision(Player& player) {
    bool wasOnGround = player.physics.isOnGround;
    
    // First, assume we're not on the ground unless we detect a collision
    if (player.position.y < player.physics.groundLevel) {
        player.physics.isOnGround = false;
    }
    
    // Calculate the actual player bounds based on current animation state
    int playerLeft = player.position.x + player.xOffset;
    int playerRight = playerLeft + player.currentWidth;
    int playerBottom = player.position.y + player.yOffset + player.currentHeight;
    
    for (const auto& platform : state.platforms) {
        // Check if player's bottom edge is near the platform's top edge
        // AND player is within the horizontal bounds of the platform
        if (player.physics.velocityY > 0 && 
            playerBottom >= platform.y - 1 &&  // More forgiving collision (-1)
            playerBottom <= platform.y + 2 &&  // More forgiving collision (+2)
            playerRight > platform.x && 
            playerLeft < platform.x + platform.width) {
            
            // Player landed on this platform
            player.position.y = platform.y - player.currentHeight - player.yOffset;  // Position player on top of platform
            player.physics.velocityY = 0;
            player.physics.isOnGround = true;
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
    
    return player.physics.isOnGround;
}

// Check if player collects any coins
void CheckCoinCollection(Player& player, int playerIndex) {
    // Use animated dimensions for coin collection detection
    int playerLeft = player.position.x + player.xOffset;
    int playerRight = playerLeft + player.currentWidth;
    int playerTop = player.position.y + player.yOffset;
    int playerBottom = playerTop + player.currentHeight;
    
    for (auto& coin : state.coins) {
        if (coin.active && 
            playerLeft < coin.x + 1 && playerRight > coin.x &&
            playerTop < coin.y + 1 && playerBottom > coin.y) {
            // Coin collected
            coin.active = false;
            player.score += 10;
            
            // Create explosion on coin collection
            StartExplosion(coin.x, coin.y);
        }
    }
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
        coin.y = (rand() % (state.players[0].physics.groundLevel - 5)) + 2;  // Avoid spawning too high or too low
    }
    
    coin.active = true;
    coin.lifetime = 0;
    coin.exploding = false;
    coin.explosionFrame = 0;
    coin.value = (rand() % 3 == 0) ? 20 : 10;  // 33% chance for a high-value coin
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
    
    // Initialize platforms, coins, and players
    InitializePlatforms();
    InitializeCoins();
    InitializePlayers();
    
    while (running.load()) {
        // Get keyboard state once per frame
        auto& keys = notepad.GetKeysPressed();
        
        // Process keyboard input for Player 1 (WASD)
        if (keys.find(IL::KEY_A) != keys.end()) {
            if (state.players[0].position.x > 0) {
                state.players[0].position.x--;
                state.players[0].isMovingHorizontal = true;
                state.players[0].lastMoveDirection = -1;
                state.players[0].moveFrames = 10;
            }
        }
        
        if (keys.find(IL::KEY_D) != keys.end()) {
            if (state.players[0].position.x < SCREEN_WIDTH - PLAYER_WIDTH) {
                state.players[0].position.x++;
                state.players[0].isMovingHorizontal = true;
                state.players[0].lastMoveDirection = 1;
                state.players[0].moveFrames = 10;
            }
        }
        
        if (keys.find(IL::KEY_W) != keys.end()) {
            // Only allow jumping when on the ground
            if (state.players[0].physics.isOnGround) {
                state.players[0].physics.velocityY = state.players[0].physics.jumpForce;
                state.players[0].physics.isOnGround = false;
            }
        }
        
        // Process keyboard input for Player 2 (Arrow Keys)
        if (keys.find(IL::KEY_LEFT) != keys.end()) {
            if (state.players[1].position.x > 0) {
                state.players[1].position.x--;
                state.players[1].isMovingHorizontal = true;
                state.players[1].lastMoveDirection = -1;
                state.players[1].moveFrames = 10;
            }
        }
        
        if (keys.find(IL::KEY_RIGHT) != keys.end()) {
            if (state.players[1].position.x < SCREEN_WIDTH - PLAYER_WIDTH) {
                state.players[1].position.x++;
                state.players[1].isMovingHorizontal = true;
                state.players[1].lastMoveDirection = 1;
                state.players[1].moveFrames = 10;
            }
        }
        
        if (keys.find(IL::KEY_UP) != keys.end()) {
            // Only allow jumping when on the ground
            if (state.players[1].physics.isOnGround) {
                state.players[1].physics.velocityY = state.players[1].physics.jumpForce;
                state.players[1].physics.isOnGround = false;
            }
        }
        
        // Process escape key for both players
        if (keys.find(IL::KEY_ESCAPE) != keys.end()) {
            // Handle escape key (could add pause menu)
        }
        
        // Update physics for both players
        for (int i = 0; i < 2; i++) {
            Player& player = state.players[i];
            
            // Apply gravity and update position
            player.physics.velocityY += player.physics.gravity;
            
            // Apply terminal velocity
            if (player.physics.velocityY > player.physics.terminalVelocity) {
                player.physics.velocityY = player.physics.terminalVelocity;
            }
            
            // Update Y position
            player.position.y += static_cast<int>(player.physics.velocityY);
            
            // Enforce screen top boundary
            if (player.position.y < 0) {
                player.position.y = 0;
                player.physics.velocityY = 0; // Stop upward movement if hitting the ceiling
            }
            
            // Check for platform collision first
            bool onPlatform = CheckPlatformCollision(player);
            
            // Check for ground collision only if not on platform
            if (!onPlatform) {
                // Calculate the actual bottom of the player based on animation
                int playerBottom = player.position.y + player.yOffset + player.currentHeight;
                
                if (playerBottom >= player.physics.groundLevel) {
                    // Adjust position based on current height and offset
                    player.position.y = player.physics.groundLevel - player.currentHeight - player.yOffset;
                    player.physics.velocityY = 0;
                    player.physics.isOnGround = true;
                }
            }
            
            // Make sure player can't go below ground level and stays within screen boundaries
            if (player.position.y > player.physics.groundLevel) {
                player.position.y = player.physics.groundLevel;
            }
            
            // Enforce side boundaries (in case other code moves the player)
            if (player.position.x < 0) {
                player.position.x = 0;
            }
            else if (player.position.x > SCREEN_WIDTH - PLAYER_WIDTH) {
                player.position.x = SCREEN_WIDTH - PLAYER_WIDTH;
            }
            
            // Check for coin collection
            CheckCoinCollection(player, i);
            
            // Update animation state
            if (player.moveFrames > 0) {
                player.moveFrames--;
                if (player.moveFrames == 0) {
                    player.isMovingHorizontal = false;
                }
            }
        }
        
        // Spawn new coins
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
        
        // Remove inactive coins
        state.coins.erase(
            std::remove_if(state.coins.begin(), state.coins.end(), 
                [](const Coin& coin) { return !coin.active; }),
            state.coins.end()
        );
        
        // Render
        notepad.Begin();
        
        // Display scores for both players
        notepad.Text(1, 1, "P1 Score: " + std::to_string(state.players[0].score));
        notepad.Text(SCREEN_WIDTH - 15, 1, "P2 Score: " + std::to_string(state.players[1].score));
        
        // Display coin info in center
        std::string coinInfo = "Coins: " + std::to_string(state.coins.size()) + 
                             " Next: " + std::to_string((state.coinSpawnInterval - state.coinSpawnTimer) / 10);
        notepad.Text((SCREEN_WIDTH - coinInfo.length()) / 2, 1, coinInfo);
        
        RenderPlatforms(notepad, state.platforms);  // Render platforms
        RenderCoins(notepad, state.coins, state.coinLifetime);  // Render coins with degradation
        RenderExplosions(notepad, state.explosions);  // Render explosions
        
        // Render both players
        RenderPlayer(notepad, state.players[0], 0);  // Left player
        RenderPlayer(notepad, state.players[1], 1);  // Right player

        notepad.Text(1, IL::NOTEPAD_HEIGHT - 2, "By Ben McAvoy (https://github.com/BenMcAvoy)");
        notepad.Text(1, IL::NOTEPAD_HEIGHT - 1, "P1: WASD to move/jump. P2: Arrows to move/jump. Collect coins before they explode!");
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
