# 🎮 Tank Maze Game

A multiplayer tank battle game built with SFML 3.0, featuring procedurally generated mazes, intelligent enemies, and real-time online multiplayer support.

![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)
![SFML](https://img.shields.io/badge/SFML-3.0.2-green.svg)
![Platform](https://img.shields.io/badge/Platform-macOS%20%7C%20Windows-lightgrey.svg)

## 📖 Table of Contents

- [Features](#-features)
- [Project Structure](#-project-structure)
- [Building & Running](#-building--running)
- [Game Controls](#-game-controls)
- [Game Modes](#-game-modes)
- [Technical Highlights](#-technical-highlights)
- [Innovations](#-innovations)

---

## ✨ Features

- **Single Player Mode** - Battle against AI enemies and escape the maze
- **Online Multiplayer** - Real-time cooperative or competitive gameplay via WebSocket
- **Procedural Maze Generation** - Every game features a unique randomly generated maze
- **Smart AI NPCs** - Enemies with pathfinding and strategic behavior
- **Special Wall Types** - Destructible walls with unique effects (repositioning, healing, gold rewards)
- **Cross-Platform Support** - Runs on both macOS and Windows
- **Immersive Audio** - Dynamic background music and spatial sound effects

---

## 📁 Project Structure

```
CS101A_FinalProj/
├── CMakeLists.txt                 # CMake build configuration
├── README.md                      # This file
│
├── src/                           # Source code
│   ├── core/                      # Core game logic
│   │   ├── main.cpp               # Application entry point
│   │   └── Game.cpp               # Main game loop, state machine, rendering
│   │
│   ├── entities/                  # Game entities
│   │   ├── Tank.cpp               # Tank class (player & enemy)
│   │   ├── Bullet.cpp             # Projectile physics and collision
│   │   ├── Enemy.cpp              # AI behavior and pathfinding
│   │   └── HealthBar.cpp          # Health bar UI component
│   │
│   ├── world/                     # World & map generation
│   │   ├── Maze.cpp               # Maze rendering and interaction
│   │   └── MazeGenerator.cpp      # Procedural maze generation algorithm
│   │
│   ├── systems/                   # Game systems
│   │   ├── CollisionSystem.cpp    # Collision detection & response
│   │   └── AudioManager.cpp       # Sound effects & music management
│   │
│   ├── network/                   # Networking module
│   │   ├── NetworkManager.cpp     # WebSocket communication layer
│   │   └── MultiplayerHandler.cpp # Multiplayer game state sync
│   │
│   └── include/                   # Header files (mirrors src/ structure)
│       ├── core/
│       ├── entities/
│       ├── world/
│       ├── systems/
│       ├── network/
│       ├── ui/                    # UI utilities
│       │   ├── UIHelper.hpp       # Menu rendering helpers
│       │   └── RoundedRectangle.hpp
│       └── utils/
│           └── Utils.hpp          # Math utilities, resource path helpers
│
├── tank_assets/                   # Tank sprite assets
│   └── PNG/
│       ├── Hulls_Color_A/         # Player 1 tank body
│       ├── Hulls_Color_B/         # Player 2 tank body
│       ├── Hulls_Color_C/         # Allied NPC tank body
│       ├── Hulls_Color_D/         # Enemy tank body
│       └── Weapon_Color_*/        # Corresponding turret sprites
│
├── music_assets/                  # Audio resources
│   ├── menu.mp3                   # Menu background music
│   ├── start.mp3, middle.mp3, climax.mp3  # In-game music
│   ├── shoot.mp3, explode.mp3     # Combat sound effects
│   └── ...                        # Additional sound effects
│
├── server/                        # Multiplayer server
│   └── server.js                  # Node.js WebSocket server
│
└── Build/                         # Build output directory
```

---

## 🔧 Building & Running

### Prerequisites

| Requirement | Version |
|-------------|---------|
| C++ Compiler | C++20 support (GCC 10+, Clang 12+, MSVC 2019+) |
| CMake | 3.16 or higher |
| Git | For automatic SFML download |

### macOS

```bash
# 1. Clone the repository
git clone https://github.com/InfiniteHorizon0524/Tank_Maze_Game
cd CS101A_FinalProj

# 2. Create build directory
mkdir -p Build && cd Build

# 3. Configure (SFML will be downloaded automatically)
cmake .. -DCMAKE_BUILD_TYPE=Release

# 4. Build (using all CPU cores)
cmake --build . --config Release -j

# 5. Run the game
open CS101AFinalProj.app
```

**Debug build:**
```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build . --config Debug -j
```

### Windows (Visual Studio)

```powershell
# 1. Clone the repository
git clone https://github.com/InfiniteHorizon0524/Tank_Maze_Game
cd CS101A_FinalProj

# 2. Create build directory
mkdir Build; cd Build

# 3. Configure
cmake ..

# 4. Build
cmake --build . --config Release -j

# 5. Run
.\Release\CS101AFinalProj.exe
```

### Windows (MinGW)

```bash
mkdir -p Build && cd Build
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build . -j
./CS101AFinalProj.exe
```

### Quick Rebuild

After initial configuration:
```bash
cd Build && cmake --build . -j
```

---

## 🎮 Game Controls

### Movement & Combat

| Key | Action |
|-----|--------|
| `W` | Move upward |
| `A` | Move left |
| `S` | Move downward |
| `D` | Move right |
| **Mouse Move** | Aim turret |
| **Left Click** | Fire bullet |

### Game Management

| Key | Action |
|-----|--------|
| `P` | Pause / Resume game |
| `ESC` | Pause menu / Return to previous screen |
| `Space` | Enter wall placing mode |
| `Enter` | Confirm selection in menus |

### Multiplayer-Specific

| Key | Action |
|-----|--------|
| `F` | Rescue downed teammate (hold near teammate in Escape mode) |

### Menu Navigation

| Key | Action |
|-----|--------|
| `W` / `↑` | Navigate up |
| `S` / `↓` | Navigate down |
| `A` / `←` | Decrease value |
| `D` / `→` | Increase value |
| `Enter` / `Space` | Select option |

---

## 🎯 Game Modes

### Single Player
Navigate through a procedurally generated maze, defeat AI enemies, and reach the exit to win.

### Multiplayer - Escape Mode
**Cooperative**: Two players work together to escape the maze. If one player is downed, the other can rescue them by holding `F` nearby. Both players must reach the exit to win.

### Multiplayer - Battle Mode
**Competitive**: Players fight against each other. Destroy your opponent's tank to claim victory. Collect coins from gold walls to recruit ally NPCs!

---

## 🛠 Technical Highlights

### 1. Logical Resolution System
The game uses a **fixed logical resolution** independent of the actual window size. This ensures:
- **Fair gameplay** across all screen sizes
- **Consistent game mechanics** regardless of display resolution
- Players see the same game area with identical zoom levels

```cpp
// Fixed logical resolution for fairness
static constexpr unsigned int LOGICAL_WIDTH = 1920;
static constexpr unsigned int LOGICAL_HEIGHT = 1080;
static constexpr float VIEW_ZOOM = 0.75f; 
```

### 2. Procedural Maze Generation
Uses a **randomized depth-first search** algorithm with:
- Guaranteed solvable mazes with path from start to exit
- Configurable maze dimensions (Small to Ultra sizes)
- Strategic placement of special walls
- NPC spawn point generation ensuring fair distribution

### 3. Smart AI Pathfinding
AI enemies feature intelligent behavior:
- **A* pathfinding** for optimal route calculation
- **Destructible wall consideration** - NPCs can plan paths through breakable walls
- **Target prioritization** - Enemies track and engage the nearest threat
- **Dynamic re-pathing** when obstacles change

### 4. Real-time Network Synchronization
WebSocket-based multiplayer with:
- **Room-based matchmaking** via room codes
- **State synchronization** for positions, rotations, and actions
- **Maze sharing** ensures identical maps for all players
- **Low-latency event broadcasting** for smooth gameplay

### 5. Spatial Audio System
Audio manager with 3D positioning:
- Sound effects attenuate based on distance from the camera
- Different music tracks for menu, gameplay phases, and climax
- Positional audio cues for off-screen events

---

## 💡 Innovations

### 🌐 Cross-Platform Multiplayer
The game supports **seamless online multiplayer** between macOS and Windows players through a unified WebSocket protocol. The Node.js server handles room management and state relay.

### 🖥 Cross-System Compatibility
Built with CMake and SFML, the project compiles and runs identically on:
- macOS (Intel & Apple Silicon)
- Windows (MSVC & MinGW)

Resource paths are automatically resolved for macOS app bundles via CoreFoundation APIs.

### 🧱 Wall System

#### Basic Walls
| Wall Type | Color | Description |
|-----------|-------|-------------|
| **Solid** | Dark Gray | Indestructible barrier, cannot be destroyed |
| **Destructible** | Brown | Can be destroyed; **collected into inventory** for later placement |

#### Special Walls
| Wall Type | Color | Effect |
|-----------|-------|--------|
| **Gold Wall** | Golden | Awards **2 coins** when destroyed (3 coins can recruit 1 ally NPC) |
| **Healing Wall** | Blue | Restores **25% health** when destroyed |

#### Wall Placement System
- Destroy **brown destructible walls** to collect them into your inventory
- Press `Space` to enter **placement mode**
- Click on any empty tile to place a wall from your inventory
- Use walls strategically to block enemies or create defensive positions

### 🤖 Intelligent NPC System
AI tanks feature multiple behavior modes:
- **Chase** - Pursue detected players using pathfinding
- **Attack** - Engage targets when in line of sight
- **Destroy Walls** - Break through destructible obstacles blocking their path

**Recruit Ally NPCs**: Collect **3 gold coins** (from destroying gold walls) to recruit an NPC to fight alongside you! Allies change their appearance and will attack enemies instead of you.

### ⚖️ Fairness Mechanisms
- **Logical Resolution**: All players see the same game area regardless of screen size
- **Synchronized Random Mazes**: Multiplayer mazes are identical for all participants
- **Balanced Spawn Points**: Players and enemies spawn at predetermined fair locations

### 🎵 Dynamic Audio Experience
- **Adaptive Music**: Background tracks change based on game progress (start → middle → climax when approaching exit)
- **Spatial Sound Effects**: Combat sounds are positioned in 3D space
- **Audio Feedback**: Distinct sounds for different wall types, hits, and actions

### 🗺️ Minimap & Dark Mode: Two Ways to Play

The game offers two distinct visual modes that fundamentally change the gameplay experience:

#### **Normal Mode with Minimap**
In standard mode, players have access to a **real-time minimap** showing the entire maze layout. This transforms the game into a **strategic experience** where you can:
- Plan optimal routes to the exit
- Track enemy positions and movements  
- Coordinate with teammates in multiplayer
- Make informed decisions about which paths to take

The minimap empowers players to think tactically, weighing risk vs. reward when choosing between shorter dangerous routes or longer safe paths.

#### **Dark Mode: Pure Tension**
Toggle Dark Mode for an entirely different experience. With **limited visibility** and **no minimap**, every corner becomes a potential ambush. The darkness creates:
- **Heart-pounding exploration** - You never know what's around the next turn
- **Heightened audio awareness** - Sound cues become your primary navigation tool
- **True survival horror atmosphere** - Every enemy encounter is a genuine surprise

#### **🎵 The Climax Mystery**
Here's where it gets *really* intense: when **any player** approaches the maze exit, the background music dynamically shifts to an **adrenaline-pumping climax track**. 

In normal mode, you can check the minimap to see who's close to winning. But in **Dark Mode Battle**? You hear the music change... but you have **no idea** if it's you or your opponent about to claim victory! 

This creates an incredible psychological tension:
- *"Is that me triggering the climax music, or are they about to win?"*
- *"Should I rush to the exit or hunt down my opponent first?"*
- *"They might be one step away from victory and I can't even see them!"*

The combination of audio-driven suspense and visual uncertainty makes Dark Mode Battle the ultimate test of nerves and skill.

---

## 🚀 Running the Multiplayer Server

```bash
cd server
node server.js
```

The server runs on `localhost:9999` by default. Players connect by entering the server IP in the game's multiplayer menu.

---

## 📦 Dependencies

- **[SFML 3.0.2](https://www.sfml-dev.org/)** - Graphics, audio, and networking (auto-downloaded via CMake)
- **Node.js** - For the multiplayer server (optional)

---

## 📄 License

This project was developed as a final project in CS101A by Tian Yang and Jiang Xinyu.

---

*Built with ❤️ using C++20 and SFML 3.0*
