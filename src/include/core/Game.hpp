#pragma once

#include <SFML/Graphics.hpp>
#include <vector>
#include <memory>
#include "Tank.hpp"
#include "Bullet.hpp"
#include "Enemy.hpp"
#include "Maze.hpp"
#include "MazeGenerator.hpp"
#include "NetworkManager.hpp"
#include "MultiplayerHandler.hpp"
#include "AudioManager.hpp"

// 游戏状态枚举
enum class GameState
{
  MainMenu,   // 主菜单（选择 Single/Multi Player 和设置）
  ModeSelect, // 模式选择（选择 Escape/Battle）
  Playing,
  Paused,
  Connecting,
  CreatingRoom, // 创建房间时选择游戏模式
  WaitingForPlayer,
  RoomLobby, // 房间大厅（显示房间信息，等待开始）
  Multiplayer,
  GameOver,
  Victory
};

// 主菜单选项
enum class MainMenuOption
{
  SinglePlayer,
  MultiPlayer,
  MapSize, // 地图大小预设
  MapWidth,
  MapHeight,
  EnemyCount,
  Exit,
  Count
};

// 地图大小预设
enum class MapSizePreset
{
  Small,  // 31x21, 10 NPCs
  Medium, // 41x31, 20 NPCs
  Large,  // 61x51, 30 NPCs
  Ultra,  // 121x101, 80 NPCs
  Custom, // 自定义
  Count
};

// 模式选择选项
enum class GameModeOption
{
  EscapeMode, // 逃脱模式（到达终点获胜）
  BattleMode, // 战斗模式（击败对手获胜）
  Back,
  Count
};

// 输入模式
enum class InputMode
{
  None,
  ServerIP,
  RoomCode
};

class Game
{
public:
  Game();

  bool init();
  void run();

private:
  void processEvents();
  void processMainMenuEvents(const sf::Event &event);
  void processModeSelectEvents(const sf::Event &event);
  void processConnectingEvents(const sf::Event &event);
  void processCreatingRoomEvents(const sf::Event &event); // 创建房间时选择模式
  void processRoomLobbyEvents(const sf::Event &event);    // 房间大厅事件处理
  void update(float dt);
  void updateMultiplayer(float dt);
  void render();
  void renderMainMenu();
  void renderModeSelect();
  void renderGame();
  void renderPaused();
  void renderGameOver();
  void renderConnecting();
  void renderCreatingRoom(); // 创建房间选择模式渲染
  void renderWaitingForPlayer();
  void renderRoomLobby(); // 房间大厅渲染
  void renderMultiplayer();
  void checkCollisions();
  void checkMultiplayerCollisions();
  void spawnEnemies();
  void resetGame();
  void startGame();
  void updateCamera();
  void generateRandomMaze();
  void handleWindowResize(); // 处理窗口大小变化，保持宽高比

  // 网络回调
  void setupNetworkCallbacks();

  // 配置（放在前面以便初始化列表使用）
  static constexpr float ASPECT_RATIO = 16.f / 9.f; // 固定宽高比
  // 逻辑分辨率（所有屏幕看到的游戏范围相同）
  static constexpr unsigned int LOGICAL_WIDTH = 1920;
  static constexpr unsigned int LOGICAL_HEIGHT = 1080;
  static constexpr float VIEW_ZOOM = 0.75f; // 视图缩放（小于1表示放大，看到更小范围）
  unsigned int m_screenWidth = 1280;        // 实际窗口宽度
  unsigned int m_screenHeight = 720;        // 实际窗口高度
  const float m_shootCooldown = 0.3f;
  const float m_bulletSpeed = 500.f;
  const float m_cameraLookAhead = 100.f; // 视角向瞄准方向偏移量
  const float m_cameraSmoothSpeed = 3.f; // 相机平滑跟随速度（越小越平滑）
  const float m_tankScale = 0.4f;

  sf::Vector2f m_currentCameraPos = {0.f, 0.f}; // 当前相机位置（用于平滑插值）

  sf::RenderWindow m_window;
  sf::View m_gameView; // 游戏视图（跟随玩家）
  sf::View m_uiView;   // UI 视图（固定）

  std::unique_ptr<Tank> m_player;
  std::unique_ptr<Tank> m_otherPlayer; // 另一个玩家（多人模式）
  std::vector<std::unique_ptr<Enemy>> m_enemies;
  std::vector<std::unique_ptr<Bullet>> m_bullets;
  Maze m_maze;
  MazeGenerator m_mazeGenerator;

  sf::Font m_font;

  sf::Clock m_clock;
  sf::Clock m_shootClock;

  // 游戏状态
  GameState m_gameState = GameState::MainMenu;
  MainMenuOption m_mainMenuOption = MainMenuOption::SinglePlayer;
  GameModeOption m_gameModeOption = GameModeOption::EscapeMode;

  // 当前选择的玩家模式和游戏模式
  bool m_isMultiplayer = false; // true = Multi Player, false = Single Player

  // 多人游戏状态（使用MultiplayerState结构体）
  MultiplayerState m_mpState;

  // 输入
  InputMode m_inputMode = InputMode::None;
  std::string m_inputText;
  std::string m_serverIP = "183.131.51.191";

  // 地图尺寸选项
  MapSizePreset m_mapSizePreset = MapSizePreset::Medium; // 默认 Medium
  std::vector<int> m_widthOptions = {21, 31, 41, 51, 61, 71, 81, 101, 121, 151};
  std::vector<int> m_heightOptions = {15, 21, 31, 41, 51, 61, 71, 81, 101};
  int m_widthIndex = 2;  // 默认 41
  int m_heightIndex = 2; // 默认 31
  int m_mazeWidth = 41;
  int m_mazeHeight = 31;

  // 敌人数量选项
  std::vector<int> m_enemyOptions = {3, 5, 8, 10, 15, 20, 30, 50, 80, 100};
  int m_enemyIndex = 5; // 默认 20

  // 多人模式：R键状态跟踪（事件驱动）
  bool m_rKeyWasPressed = false;

  // 墙壁放置模式
  bool m_placementMode = false; // 是否处于放置模式

  // 暗黑模式（创建房间时选择）
  bool m_darkModeOption = false;

  bool m_gameOver = false;
  bool m_gameWon = false;

  // 单人模式终点交互相关（按住E键3秒才能判定到达）
  bool m_isAtExitZone = false;    // 是否在终点区域内
  bool m_isHoldingExit = false;   // 是否正在按住E键
  float m_exitHoldProgress = 0.f; // 按住E键的进度 (0-3s)
  bool m_eKeyHeld = false;        // E键是否按下

  // 音频相关
  bool m_exitVisible = false; // 终点是否在视野内

  // 获取多人模式上下文
  MultiplayerContext getMultiplayerContext();

  // 检查终点是否在玩家视野内
  bool isExitInView() const;

  // 渲染暗黑模式遮罩
  void renderDarkModeOverlay();

  // 渲染小地图（单人模式）
  void renderMinimap();

  // 暗黑模式遮罩纹理（非静态，确保在窗口销毁前释放）
  std::unique_ptr<sf::Texture> m_darkModeTexture;
  std::unique_ptr<sf::Sprite> m_darkModeSprite;
  unsigned int m_darkModeTexWidth = 0;
  unsigned int m_darkModeTexHeight = 0;
};
