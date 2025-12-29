#pragma once

#include <SFML/Graphics.hpp>
#include <vector>
#include <memory>
#include <string>
#include <functional>
#include "Tank.hpp"
#include "Bullet.hpp"
#include "Enemy.hpp"
#include "Maze.hpp"
#include "NetworkManager.hpp"

// 多人模式状态
struct MultiplayerState
{
  bool isMultiplayer = false;
  bool isHost = false;
  bool localPlayerReachedExit = false;
  bool otherPlayerReachedExit = false;
  bool multiplayerWin = false;
  std::string roomCode;
  std::string connectionStatus = "Enter server IP:";
  int npcSyncCounter = 0;
  int nearbyNpcIndex = -1;
  bool rKeyJustPressed = false;
  std::vector<std::string> generatedMazeData;

  // Escape 模式相关
  bool isEscapeMode = false;    // 是否是 Escape 模式（否则是 Battle 模式）
  bool localPlayerDead = false; // 本地玩家是否已死亡（可被救援）
  bool otherPlayerDead = false; // 对方玩家是否已死亡
  bool isRescuing = false;      // 是否正在救援对方
  bool beingRescued = false;    // 是否正在被对方救援
  float rescueProgress = 0.f;   // 救援进度 (0-3s)
  float rescueSyncTimer = 0.f;  // 救援进度同步计时器
  bool fKeyHeld = false;        // F键是否按下
  bool canRescue = false;       // 是否在救援范围内

  // 终点交互相关（按住E键3秒才能判定到达）
  bool isAtExitZone = false;    // 是否在终点区域内
  bool isHoldingExit = false;   // 是否正在按住E键
  float exitHoldProgress = 0.f; // 按住E键的进度 (0-3s)
  bool eKeyHeld = false;        // E键是否按下

  // 房间大厅相关
  bool localPlayerReady = false;  // 本地玩家是否准备就绪
  bool otherPlayerReady = false;  // 对方玩家是否准备就绪
  bool otherPlayerInRoom = false; // 对方是否在房间中
  std::string localPlayerIP;      // 本地玩家IP
  std::string otherPlayerIP;      // 对方玩家IP
  int npcCount = 10;              // NPC数量
  int mazeWidth = 41;             // 迷宫宽度
  int mazeHeight = 31;            // 迷宫高度
  bool isDarkMode = false;        // 是否是暗黑模式
};

// 多人游戏渲染和更新所需的上下文
struct MultiplayerContext
{
  sf::RenderWindow &window;
  sf::View &gameView;
  sf::View &uiView;
  sf::Font &font;
  Tank *player;
  Tank *otherPlayer;
  std::vector<std::unique_ptr<Enemy>> &enemies;
  std::vector<std::unique_ptr<Bullet>> &bullets;
  Maze &maze;
  unsigned int screenWidth;
  unsigned int screenHeight;
  float tankScale;
  bool placementMode; // 墙壁放置模式
  bool isEscapeMode;  // 是否是 Escape 模式
  bool isDarkMode;    // 是否是暗黑模式
};

// 多人模式处理器
class MultiplayerHandler
{
public:
  // 更新多人游戏逻辑
  static void update(
      MultiplayerContext &ctx,
      MultiplayerState &state,
      float dt,
      std::function<void()> onVictory,
      std::function<void()> onDefeat);

  // 渲染连接界面
  static void renderConnecting(
      sf::RenderWindow &window,
      sf::View &uiView,
      sf::Font &font,
      unsigned int screenWidth,
      unsigned int screenHeight,
      const std::string &connectionStatus,
      const std::string &inputText,
      bool isServerIPMode);

  // 渲染等待玩家界面
  static void renderWaitingForPlayer(
      sf::RenderWindow &window,
      sf::View &uiView,
      sf::Font &font,
      unsigned int screenWidth,
      unsigned int screenHeight,
      const std::string &roomCode);

  // 渲染多人游戏界面
  static void renderMultiplayer(
      MultiplayerContext &ctx,
      MultiplayerState &state);

  // 清理静态资源（在程序退出前调用）
  static void cleanup();

private:
  // 更新NPC AI逻辑（仅房主执行）
  static void updateNpcAI(
      MultiplayerContext &ctx,
      MultiplayerState &state,
      float dt);

  // 检查玩家接近的NPC（用于激活提示）
  static void checkNearbyNpc(
      MultiplayerContext &ctx,
      MultiplayerState &state);

  // 处理NPC激活
  static void handleNpcActivation(
      MultiplayerContext &ctx,
      MultiplayerState &state);

  // 渲染NPC及其标记
  static void renderNpcs(
      MultiplayerContext &ctx,
      MultiplayerState &state);

  // 渲染UI血条和金币
  static void renderUI(
      MultiplayerContext &ctx,
      MultiplayerState &state);

  // 渲染小地图
  static void renderMinimap(
      MultiplayerContext &ctx,
      MultiplayerState &state);

  // 渲染暗黑模式遮罩
  static void renderDarkModeOverlay(
      MultiplayerContext &ctx);

  // 暗黑模式遮罩纹理（静态成员）
  static std::unique_ptr<sf::Texture> s_darkModeTexture;
  static std::unique_ptr<sf::Sprite> s_darkModeSprite;
  static bool s_darkModeTextureInitialized;
  static unsigned int s_lastTextureWidth;
  static unsigned int s_lastTextureHeight;

  // 初始化/更新暗黑模式遮罩纹理
  static void initDarkModeTexture(unsigned int width, unsigned int height);
};
