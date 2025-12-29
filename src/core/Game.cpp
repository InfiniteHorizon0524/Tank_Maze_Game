#include "Game.hpp"
#include "CollisionSystem.hpp"
#include "UIHelper.hpp"
#include "MultiplayerHandler.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>

Game::Game()
    : m_mazeGenerator(31, 21) // 默认中等尺寸
{
  // 获取桌面尺寸，计算最大窗口大小（保持16:9比例）
  auto desktop = sf::VideoMode::getDesktopMode();
  unsigned int maxWidth = desktop.size.x * 9 / 10; // 留10%边距
  unsigned int maxHeight = desktop.size.y * 9 / 10;

  // 根据宽高比计算实际窗口尺寸
  if (static_cast<float>(maxWidth) / maxHeight > ASPECT_RATIO)
  {
    // 屏幕太宽，以高度为准
    m_screenHeight = maxHeight;
    m_screenWidth = static_cast<unsigned int>(m_screenHeight * ASPECT_RATIO);
  }
  else
  {
    // 屏幕太高，以宽度为准
    m_screenWidth = maxWidth;
    m_screenHeight = static_cast<unsigned int>(m_screenWidth / ASPECT_RATIO);
  }

  // 创建窗口（使用实际尺寸）
  m_window.create(sf::VideoMode({m_screenWidth, m_screenHeight}), "Tank Maze Game");
  m_window.setFramerateLimit(120);

  // 初始化视图 - 使用固定的逻辑分辨率，保证所有屏幕看到的范围相同
  m_gameView = sf::View(sf::FloatRect({0.f, 0.f}, {static_cast<float>(LOGICAL_WIDTH), static_cast<float>(LOGICAL_HEIGHT)}));
  m_uiView = sf::View(sf::FloatRect({0.f, 0.f}, {static_cast<float>(LOGICAL_WIDTH), static_cast<float>(LOGICAL_HEIGHT)}));
}

bool Game::init()
{
  // 加载字体 - 跨平台支持
  bool fontLoaded = false;

#ifdef _WIN32
  // Windows 字体路径
  if (m_font.openFromFile("C:\\Windows\\Fonts\\arial.ttf"))
  {
    fontLoaded = true;
  }
  else if (m_font.openFromFile("C:\\Windows\\Fonts\\times.ttf"))
  {
    fontLoaded = true;
  }
  else if (m_font.openFromFile("C:\\Windows\\Fonts\\segoeui.ttf"))
  {
    fontLoaded = true;
  }
#elif __APPLE__
  // macOS 字体路径
  if (m_font.openFromFile("/System/Library/Fonts/Helvetica.ttc"))
  {
    fontLoaded = true;
  }
  else if (m_font.openFromFile("/System/Library/Fonts/Arial.ttf"))
  {
    fontLoaded = true;
  }
  else if (m_font.openFromFile("/Library/Fonts/Arial.ttf"))
  {
    fontLoaded = true;
  }
#else
  // Linux 字体路径
  if (m_font.openFromFile("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"))
  {
    fontLoaded = true;
  }
  else if (m_font.openFromFile("/usr/share/fonts/TTF/DejaVuSans.ttf"))
  {
    fontLoaded = true;
  }
  else if (m_font.openFromFile("/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf"))
  {
    fontLoaded = true;
  }
#endif

  if (!fontLoaded)
  {
    return false;
  }

  // 初始化音频系统（使用资源路径）
  std::string resourcePath = getResourcePath();
  if (!AudioManager::getInstance().init(resourcePath + "music_assets/"))
  {
    std::cerr << "Warning: Failed to initialize audio system" << std::endl;
    // 音频初始化失败不阻止游戏运行
  }

  // 设置听音范围（基于视野大小）
  AudioManager::getInstance().setListeningRange(LOGICAL_WIDTH * VIEW_ZOOM * 0.6f);

  // 设置网络回调
  setupNetworkCallbacks();

  return true;
}

void Game::generateRandomMaze()
{
  // 使用已设置的迷宫尺寸（来自预设或自定义）
  m_mazeGenerator = MazeGenerator(m_mazeWidth, m_mazeHeight);

  // 使用菜单中选择的敌人数量
  int enemyCount = m_enemyOptions[m_enemyIndex];
  m_mazeGenerator.setEnemyCount(enemyCount);
  m_mazeGenerator.setDestructibleRatio(0.15f);

  // 设置 Escape 模式（单人 Escape 也生成治疗墙）
  bool isEscape = (m_gameModeOption == GameModeOption::EscapeMode);
  m_mazeGenerator.setEscapeMode(isEscape);

  auto mazeMap = m_mazeGenerator.generate();
  m_maze.loadFromString(mazeMap);
}

void Game::startGame()
{
  // 停止所有残留音效
  AudioManager::getInstance().stopAllSFX();

  // 生成随机地图
  generateRandomMaze();

  // 创建玩家
  m_player = std::make_unique<Tank>();

  // 加载玩家坦克纹理
  std::string resPath = getResourcePath();
  m_player->loadTextures(resPath + "tank_assets/PNG/Hulls_Color_A/Hull_01.png",
                         resPath + "tank_assets/PNG/Weapon_Color_A/Gun_01.png");

  // 设置玩家到起点
  sf::Vector2f startPos = m_maze.getStartPosition();
  m_player->setPosition(startPos);

  // 初始化相机位置和缩放，直接居中到玩家位置（无移动效果）
  m_currentCameraPos = startPos;
  m_gameView.setCenter(startPos);
  m_gameView.setSize({LOGICAL_WIDTH * VIEW_ZOOM, LOGICAL_HEIGHT * VIEW_ZOOM});

  // 清空子弹
  m_bullets.clear();

  // 在指定位置生成敌人
  spawnEnemies();

  m_gameState = GameState::Playing;
  m_gameOver = false;
  m_gameWon = false;
  m_exitVisible = false;   // 重置终点可见状态
  m_placementMode = false; // 重置放置模式

  // 播放游戏开始BGM
  AudioManager::getInstance().playBGM(BGMType::Start);
}

void Game::spawnEnemies()
{
  m_enemies.clear();
  const auto &spawnPoints = m_maze.getEnemySpawnPoints();

  std::string resPath = getResourcePath();
  for (const auto &pos : spawnPoints)
  {
    auto enemy = std::make_unique<Enemy>();
    if (enemy->loadTextures(resPath + "tank_assets/PNG/Hulls_Color_D/Hull_01.png",
                            resPath + "tank_assets/PNG/Weapon_Color_D/Gun_01.png"))
    {
      enemy->setPosition(pos);
      enemy->setBounds(m_maze.getSize());
      m_enemies.push_back(std::move(enemy));
    }
  }
}

void Game::resetGame()
{
  m_gameState = GameState::MainMenu;
  m_gameOver = false;
  m_gameWon = false;
  m_placementMode = false; // 重置放置模式
  m_mpState.multiplayerWin = false;
  m_enemies.clear();
  m_bullets.clear();
  m_player.reset();
  m_otherPlayer.reset();
  m_mpState.isMultiplayer = false;
  m_mpState.isHost = false;
  m_mpState.localPlayerReachedExit = false;
  m_mpState.otherPlayerReachedExit = false;
  m_mpState.roomCode.clear();
  m_mpState.connectionStatus = "Enter server IP:";
  m_inputText.clear();
  m_inputMode = InputMode::None;
  m_mpState.generatedMazeData.clear();

  // 重置 Escape 模式相关状态
  m_mpState.isEscapeMode = false;
  m_mpState.localPlayerDead = false;
  m_mpState.otherPlayerDead = false;
  m_mpState.isRescuing = false;
  m_mpState.beingRescued = false;
  m_mpState.rescueProgress = 0.f;
  m_mpState.fKeyHeld = false;
  m_mpState.canRescue = false;

  // 重置终点交互状态（多人模式）
  m_mpState.isAtExitZone = false;
  m_mpState.isHoldingExit = false;
  m_mpState.exitHoldProgress = 0.f;
  m_mpState.eKeyHeld = false;

  // 重置终点交互状态（单人模式）
  m_isAtExitZone = false;
  m_isHoldingExit = false;
  m_exitHoldProgress = 0.f;
  m_eKeyHeld = false;

  // 断开网络连接
  NetworkManager::getInstance().disconnect();
}

void Game::run()
{
  // 开始播放菜单BGM
  AudioManager::getInstance().playBGM(BGMType::Menu);

  while (m_window.isOpen())
  {
    float dt = m_clock.restart().asSeconds();

    // 处理网络消息
    NetworkManager::getInstance().update();

    // 更新音频系统（清理已播放完的音效）
    AudioManager::getInstance().update();

    processEvents();

    switch (m_gameState)
    {
    case GameState::MainMenu:
    case GameState::ModeSelect:
      // 菜单播放菜单BGM
      if (AudioManager::getInstance().getCurrentBGM() != BGMType::Menu)
      {
        AudioManager::getInstance().playBGM(BGMType::Menu);
      }
      break;
    case GameState::Playing:
      update(dt);
      // 检查是否看到终点，切换BGM
      if (!m_exitVisible && isExitInView())
      {
        m_exitVisible = true;
        AudioManager::getInstance().playBGM(BGMType::Climax);
      }
      break;
    case GameState::Paused:
      // 暂停状态不需要 update
      break;
    case GameState::Connecting:
    case GameState::CreatingRoom:
    case GameState::WaitingForPlayer:
    case GameState::RoomLobby:
      // 等待状态播放菜单BGM
      if (AudioManager::getInstance().getCurrentBGM() != BGMType::Menu)
      {
        AudioManager::getInstance().playBGM(BGMType::Menu);
      }
      break;
    case GameState::Multiplayer:
      updateMultiplayer(dt);
      // 检查是否看到终点，切换BGM并同步给对方
      if (!m_exitVisible && isExitInView())
      {
        m_exitVisible = true;
        AudioManager::getInstance().playBGM(BGMType::Climax);
        // 通知对方也开始播放高潮BGM
        NetworkManager::getInstance().sendClimaxStart();
      }
      break;
    case GameState::GameOver:
    case GameState::Victory:
      // 游戏结束/胜利状态不需要 update
      break;
    }

    render();
  }

  // 在窗口关闭后清理静态资源（避免 OpenGL 上下文销毁后释放纹理）
  MultiplayerHandler::cleanup();
  m_darkModeTexture.reset();
  m_darkModeSprite.reset();
}

void Game::processMainMenuEvents(const sf::Event &event)
{
  if (const auto *keyPressed = event.getIf<sf::Event::KeyPressed>())
  {
    int optionCount = static_cast<int>(MainMenuOption::Count);

    // 辅助函数：应用地图预设
    auto applyMapPreset = [this]()
    {
      switch (m_mapSizePreset)
      {
      case MapSizePreset::Small:
        m_mazeWidth = 31;
        m_mazeHeight = 21;
        m_widthIndex = 1;
        m_heightIndex = 1;
        m_enemyIndex = 3; // 10 NPCs
        break;
      case MapSizePreset::Medium:
        m_mazeWidth = 41;
        m_mazeHeight = 31;
        m_widthIndex = 2;
        m_heightIndex = 2;
        m_enemyIndex = 5; // 20 NPCs
        break;
      case MapSizePreset::Large:
        m_mazeWidth = 61;
        m_mazeHeight = 51;
        m_widthIndex = 4;
        m_heightIndex = 4;
        m_enemyIndex = 6; // 30 NPCs
        break;
      case MapSizePreset::Ultra:
        m_mazeWidth = 121;
        m_mazeHeight = 101;
        m_widthIndex = 8;
        m_heightIndex = 8;
        m_enemyIndex = 8; // 80 NPCs
        break;
      case MapSizePreset::Custom:
        // 保持当前自定义值
        m_mazeWidth = m_widthOptions[m_widthIndex];
        m_mazeHeight = m_heightOptions[m_heightIndex];
        break;
      default:
        break;
      }
    };

    switch (keyPressed->code)
    {
    case sf::Keyboard::Key::Up:
    case sf::Keyboard::Key::W:
    {
      // 向上移动，跳过无意义的子选项（MapWidth, MapHeight, EnemyCount）
      {
        int current = static_cast<int>(m_mainMenuOption);
        for (int i = 0; i < optionCount; ++i)
        {
          current = (current - 1 + optionCount) % optionCount;
          MainMenuOption cand = static_cast<MainMenuOption>(current);
          // 允许在任何情况下将光标移到 MapWidth/MapHeight/EnemyCount
          m_mainMenuOption = cand;
          break;
        }
        AudioManager::getInstance().playSFXGlobal(SFXType::MenuSelect);
      }
      break;
    }
    case sf::Keyboard::Key::Down:
    case sf::Keyboard::Key::S:
    {
      int current = static_cast<int>(m_mainMenuOption);
      for (int i = 0; i < optionCount; ++i)
      {
        current = (current + 1) % optionCount;
        MainMenuOption cand = static_cast<MainMenuOption>(current);
        // 允许在任何情况下将光标移到 MapWidth/MapHeight/EnemyCount
        m_mainMenuOption = cand;
        break;
      }
      AudioManager::getInstance().playSFXGlobal(SFXType::MenuSelect);
      break;
    }
    case sf::Keyboard::Key::Enter:
    case sf::Keyboard::Key::Space:
      switch (m_mainMenuOption)
      {
      case MainMenuOption::SinglePlayer:
        AudioManager::getInstance().playSFXGlobal(SFXType::MenuConfirm);
        m_isMultiplayer = false;
        m_gameState = GameState::ModeSelect;
        m_gameModeOption = GameModeOption::EscapeMode;
        break;
      case MainMenuOption::MultiPlayer:
        // 联机模式直接进入连接界面，创建房间时再选择模式
        AudioManager::getInstance().playSFXGlobal(SFXType::MenuConfirm);
        m_isMultiplayer = true;
        m_gameState = GameState::Connecting;
        m_inputText = m_serverIP;
        m_inputMode = InputMode::ServerIP;
        break;
      case MainMenuOption::MapSize:
      case MainMenuOption::MapWidth:
      case MainMenuOption::MapHeight:
      case MainMenuOption::EnemyCount:
        // 这些选项用左右键调整，Enter不做任何事
        break;
      case MainMenuOption::Exit:
        AudioManager::getInstance().playSFXGlobal(SFXType::MenuConfirm);
        m_window.close();
        break;
      default:
        break;
      }
      break;
    case sf::Keyboard::Key::Left:
    case sf::Keyboard::Key::A:
      switch (m_mainMenuOption)
      {
      case MainMenuOption::MapSize:
      {
        int current = static_cast<int>(m_mapSizePreset);
        current = (current - 1 + static_cast<int>(MapSizePreset::Count)) % static_cast<int>(MapSizePreset::Count);
        m_mapSizePreset = static_cast<MapSizePreset>(current);
        applyMapPreset();
        AudioManager::getInstance().playSFXGlobal(SFXType::MenuSelect);
        break;
      }
      case MainMenuOption::MapWidth:
        // 只要用户按左右改变宽度，就将预设切换为 Custom
        if (m_mapSizePreset != MapSizePreset::Custom)
        {
          m_mapSizePreset = MapSizePreset::Custom;
        }
        m_widthIndex = (m_widthIndex - 1 + m_widthOptions.size()) % m_widthOptions.size();
        m_mazeWidth = m_widthOptions[m_widthIndex];
        AudioManager::getInstance().playSFXGlobal(SFXType::MenuSelect);
        break;
      case MainMenuOption::MapHeight:
        if (m_mapSizePreset != MapSizePreset::Custom)
        {
          m_mapSizePreset = MapSizePreset::Custom;
        }
        m_heightIndex = (m_heightIndex - 1 + m_heightOptions.size()) % m_heightOptions.size();
        m_mazeHeight = m_heightOptions[m_heightIndex];
        AudioManager::getInstance().playSFXGlobal(SFXType::MenuSelect);
        break;
      case MainMenuOption::EnemyCount:
        if (m_mapSizePreset != MapSizePreset::Custom)
        {
          m_mapSizePreset = MapSizePreset::Custom;
        }
        m_enemyIndex = (m_enemyIndex - 1 + m_enemyOptions.size()) % m_enemyOptions.size();
        AudioManager::getInstance().playSFXGlobal(SFXType::MenuSelect);
        break;
      default:
        break;
      }
      break;
    case sf::Keyboard::Key::Right:
    case sf::Keyboard::Key::D:
      switch (m_mainMenuOption)
      {
      case MainMenuOption::MapSize:
      {
        int current = static_cast<int>(m_mapSizePreset);
        current = (current + 1) % static_cast<int>(MapSizePreset::Count);
        m_mapSizePreset = static_cast<MapSizePreset>(current);
        applyMapPreset();
        AudioManager::getInstance().playSFXGlobal(SFXType::MenuSelect);
        break;
      }
      case MainMenuOption::MapWidth:
        // 只要用户按左右改变宽度，就将预设切换为 Custom
        if (m_mapSizePreset != MapSizePreset::Custom)
        {
          m_mapSizePreset = MapSizePreset::Custom;
        }
        m_widthIndex = (m_widthIndex + 1) % m_widthOptions.size();
        m_mazeWidth = m_widthOptions[m_widthIndex];
        AudioManager::getInstance().playSFXGlobal(SFXType::MenuSelect);
        break;
      case MainMenuOption::MapHeight:
        if (m_mapSizePreset != MapSizePreset::Custom)
        {
          m_mapSizePreset = MapSizePreset::Custom;
        }
        m_heightIndex = (m_heightIndex + 1) % m_heightOptions.size();
        m_mazeHeight = m_heightOptions[m_heightIndex];
        AudioManager::getInstance().playSFXGlobal(SFXType::MenuSelect);
        break;
      case MainMenuOption::EnemyCount:
        if (m_mapSizePreset != MapSizePreset::Custom)
        {
          m_mapSizePreset = MapSizePreset::Custom;
        }
        m_enemyIndex = (m_enemyIndex + 1) % m_enemyOptions.size();
        AudioManager::getInstance().playSFXGlobal(SFXType::MenuSelect);
        break;
      default:
        break;
      }
      break;
    default:
      break;
    }
  }
}

void Game::processModeSelectEvents(const sf::Event &event)
{
  if (const auto *keyPressed = event.getIf<sf::Event::KeyPressed>())
  {
    int optionCount = static_cast<int>(GameModeOption::Count);

    switch (keyPressed->code)
    {
    case sf::Keyboard::Key::Up:
    case sf::Keyboard::Key::W:
    {
      int current = static_cast<int>(m_gameModeOption);
      current = (current - 1 + optionCount) % optionCount;
      m_gameModeOption = static_cast<GameModeOption>(current);
      AudioManager::getInstance().playSFXGlobal(SFXType::MenuSelect);
      break;
    }
    case sf::Keyboard::Key::Down:
    case sf::Keyboard::Key::S:
    {
      int current = static_cast<int>(m_gameModeOption);
      current = (current + 1) % optionCount;
      m_gameModeOption = static_cast<GameModeOption>(current);
      AudioManager::getInstance().playSFXGlobal(SFXType::MenuSelect);
      break;
    }
    case sf::Keyboard::Key::Escape:
      AudioManager::getInstance().playSFXGlobal(SFXType::MenuSelect);
      m_gameState = GameState::MainMenu;
      break;
    case sf::Keyboard::Key::D:
      // 切换暗黑模式
      m_darkModeOption = !m_darkModeOption;
      AudioManager::getInstance().playSFXGlobal(SFXType::MenuSelect);
      break;
    case sf::Keyboard::Key::Enter:
    case sf::Keyboard::Key::Space:
      switch (m_gameModeOption)
      {
      case GameModeOption::EscapeMode:
        // 单人模式 - 直接开始游戏
        AudioManager::getInstance().playSFXGlobal(SFXType::MenuConfirm);
        startGame();
        break;
      case GameModeOption::BattleMode:
        // Single Player Battle Mode - 还在开发中，不播放确认音效
        break;
      case GameModeOption::Back:
        AudioManager::getInstance().playSFXGlobal(SFXType::MenuSelect);
        m_gameState = GameState::MainMenu;
        break;
      default:
        break;
      }
      break;
    default:
      break;
    }
  }
}

void Game::processEvents()
{
  while (const auto event = m_window.pollEvent())
  {
    if (event->is<sf::Event::Closed>())
    {
      m_window.close();
    }

    // 处理窗口大小变化，保持宽高比
    if (const auto *resized = event->getIf<sf::Event::Resized>())
    {
      handleWindowResize();
    }

    switch (m_gameState)
    {
    case GameState::MainMenu:
      processMainMenuEvents(*event);
      break;

    case GameState::ModeSelect:
      processModeSelectEvents(*event);
      break;

    case GameState::Playing:
      if (m_player)
      {
        // 放置模式下不处理鼠标事件（避免放置墙壁时同时发射子弹）
        bool isMouseEvent = event->getIf<sf::Event::MouseButtonPressed>() ||
                            event->getIf<sf::Event::MouseButtonReleased>();
        if (!m_placementMode || !isMouseEvent)
        {
          m_player->handleInput(*event);
        }
      }
      // 按 ESC 返回菜单，按 P 暂停，按 空格 切换放置模式
      if (const auto *keyPressed = event->getIf<sf::Event::KeyPressed>())
      {
        if (keyPressed->code == sf::Keyboard::Key::Escape)
        {
          if (m_placementMode)
          {
            m_placementMode = false; // 先退出放置模式
          }
          else
          {
            resetGame();
          }
        }
        else if (keyPressed->code == sf::Keyboard::Key::P)
        {
          m_gameState = GameState::Paused;
        }
        else if (keyPressed->code == sf::Keyboard::Key::Space)
        {
          // 切换放置模式
          if (m_player && m_player->getWallsInBag() > 0)
          {
            m_placementMode = !m_placementMode;
          }
          else if (m_placementMode)
          {
            m_placementMode = false;
          }
        }
        else if (keyPressed->code == sf::Keyboard::Key::E)
        {
          // E键按下，确认终点
          m_eKeyHeld = true;
        }
      }
      // E键释放
      if (const auto *keyReleased = event->getIf<sf::Event::KeyReleased>())
      {
        if (keyReleased->code == sf::Keyboard::Key::E)
        {
          m_eKeyHeld = false;
        }
      }
      // 处理鼠标点击放置墙壁
      if (m_placementMode && m_player && m_player->getWallsInBag() > 0)
      {
        if (const auto *mousePressed = event->getIf<sf::Event::MouseButtonPressed>())
        {
          if (mousePressed->button == sf::Mouse::Button::Left)
          {
            // 获取鼠标在世界坐标中的位置
            sf::Vector2f mouseWorldPos = m_window.mapPixelToCoords(mousePressed->position, m_gameView);

            // 检查位置是否有坦克
            bool hasTankAtPos = false;
            GridPos grid = m_maze.worldToGrid(mouseWorldPos);
            sf::Vector2f gridCenter = m_maze.gridToWorld(grid);
            float checkRadius = m_maze.getTileSize() * 1.0f;

            // 检查玩家
            if (m_player)
            {
              float dist = std::hypot(m_player->getPosition().x - gridCenter.x, m_player->getPosition().y - gridCenter.y);
              if (dist < checkRadius)
                hasTankAtPos = true;
            }
            // 检查NPC
            if (!hasTankAtPos)
            {
              for (const auto &enemy : m_enemies)
              {
                if (!enemy->isDead())
                {
                  float dist = std::hypot(enemy->getPosition().x - gridCenter.x, enemy->getPosition().y - gridCenter.y);
                  if (dist < checkRadius)
                  {
                    hasTankAtPos = true;
                    break;
                  }
                }
              }
            }

            // 尝试放置墙壁
            if (!hasTankAtPos && m_maze.placeWall(mouseWorldPos))
            {
              m_player->useWallFromBag();
              // 播放放置音效
              AudioManager::getInstance().playSFX(SFXType::MenuConfirm, mouseWorldPos, m_player->getPosition());

              // 放置成功后自动退出放置模式
              m_placementMode = false;
            }
          }
          else if (mousePressed->button == sf::Mouse::Button::Right)
          {
            // 右键取消放置模式
            m_placementMode = false;
          }
        }
      }
      break;

    case GameState::Paused:
      if (const auto *keyPressed = event->getIf<sf::Event::KeyPressed>())
      {
        if (keyPressed->code == sf::Keyboard::Key::P ||
            keyPressed->code == sf::Keyboard::Key::Escape)
        {
          m_gameState = GameState::Playing;
        }
        else if (keyPressed->code == sf::Keyboard::Key::Q)
        {
          resetGame(); // 返回菜单
        }
      }
      break;

    case GameState::GameOver:
    case GameState::Victory:
      if (const auto *keyPressed = event->getIf<sf::Event::KeyPressed>())
      {
        if (keyPressed->code == sf::Keyboard::Key::R)
        {
          if (m_mpState.isMultiplayer)
          {
            // 多人模式：返回房间大厅
            if (m_mpState.isHost)
            {
              // 房主按 R：通知对方返回房间，并生成新地图
              NetworkManager::getInstance().sendRestartRequest();

              // 重新生成迷宫（使用菜单选择的NPC数量，联机模式，根据游戏模式决定墙体类型）
              int npcCount = m_enemyOptions[m_enemyIndex];
              m_maze.generateRandomMaze(m_mazeWidth, m_mazeHeight, 0, npcCount, true, m_mpState.isEscapeMode);
              m_mpState.generatedMazeData = m_maze.getMazeData();
              NetworkManager::getInstance().sendMazeData(m_mpState.generatedMazeData, m_mpState.isEscapeMode, m_mpState.isDarkMode);

              // 房主默认准备
              m_mpState.localPlayerReady = true;
              // 对方还未返回房间，默认为 NOT READY，等待服务器同步正确状态
              m_mpState.otherPlayerReady = false;
            }
            else
            {
              // 非房主按 R：通知服务器返回房间
              NetworkManager::getInstance().sendRestartRequest();
              // 非房主返回时默认为 NOT READY
              m_mpState.localPlayerReady = false;
              // 房主还未返回房间，默认为 NOT READY，等待服务器同步正确状态
              m_mpState.otherPlayerReady = false;
            }

            // 进入房间大厅
            m_gameState = GameState::RoomLobby;

            // 重置游戏状态（不重置准备状态）
            m_mpState.localPlayerReachedExit = false;
            m_mpState.otherPlayerReachedExit = false;
            m_mpState.multiplayerWin = false;
            m_mpState.localPlayerDead = false;
            m_mpState.otherPlayerDead = false;
            m_gameOver = false;
            m_gameWon = false;
            m_bullets.clear();
          }
          else
          {
            startGame(); // 单机模式重新开始
          }
        }
        else if (keyPressed->code == sf::Keyboard::Key::Escape)
        {
          resetGame(); // 返回菜单
        }
      }
      break;

    case GameState::Connecting:
      processConnectingEvents(*event);
      break;

    case GameState::CreatingRoom:
      processCreatingRoomEvents(*event);
      break;

    case GameState::WaitingForPlayer:
      if (const auto *keyPressed = event->getIf<sf::Event::KeyPressed>())
      {
        if (keyPressed->code == sf::Keyboard::Key::Escape)
        {
          NetworkManager::getInstance().disconnect();
          resetGame();
        }
      }
      break;

    case GameState::RoomLobby:
      processRoomLobbyEvents(*event);
      break;

    case GameState::Multiplayer:
      if (m_player)
      {
        // 放置模式下不处理鼠标事件（避免放置墙壁时同时发射子弹）
        bool isMouseEvent = event->getIf<sf::Event::MouseButtonPressed>() ||
                            event->getIf<sf::Event::MouseButtonReleased>();
        if (!m_placementMode || !isMouseEvent)
        {
          m_player->handleInput(*event);
        }
      }
      if (const auto *keyPressed = event->getIf<sf::Event::KeyPressed>())
      {
        if (keyPressed->code == sf::Keyboard::Key::Escape)
        {
          if (m_placementMode)
          {
            m_placementMode = false; // 先退出放置模式
          }
          else
          {
            NetworkManager::getInstance().disconnect();
            resetGame();
          }
        }
        // R键激活NPC（Battle 模式）
        if (keyPressed->code == sf::Keyboard::Key::R)
        {
          m_mpState.rKeyJustPressed = true;
          std::cout << "[DEBUG] R key event detected!" << std::endl;
        }
        // F键开始救援（Escape 模式）
        if (keyPressed->code == sf::Keyboard::Key::F)
        {
          m_mpState.fKeyHeld = true;
        }
        // E键确认到达终点
        if (keyPressed->code == sf::Keyboard::Key::E)
        {
          m_mpState.eKeyHeld = true;
        }
        // 空格键切换放置模式
        if (keyPressed->code == sf::Keyboard::Key::Space)
        {
          if (m_player && m_player->getWallsInBag() > 0)
          {
            m_placementMode = !m_placementMode;
          }
          else if (m_placementMode)
          {
            m_placementMode = false;
          }
        }
      }
      // F键释放停止救援（Escape 模式）
      // E键释放停止确认终点
      if (const auto *keyReleased = event->getIf<sf::Event::KeyReleased>())
      {
        if (keyReleased->code == sf::Keyboard::Key::F)
        {
          m_mpState.fKeyHeld = false;
        }
        if (keyReleased->code == sf::Keyboard::Key::E)
        {
          m_mpState.eKeyHeld = false;
        }
      }
      // 处理鼠标点击放置墙壁
      if (m_placementMode && m_player && m_player->getWallsInBag() > 0)
      {
        if (const auto *mousePressed = event->getIf<sf::Event::MouseButtonPressed>())
        {
          if (mousePressed->button == sf::Mouse::Button::Left)
          {
            // 获取鼠标在世界坐标中的位置
            sf::Vector2f mouseWorldPos = m_window.mapPixelToCoords(mousePressed->position, m_gameView);

            // 检查位置是否有坦克
            bool hasTankAtPos = false;
            GridPos grid = m_maze.worldToGrid(mouseWorldPos);
            sf::Vector2f gridCenter = m_maze.gridToWorld(grid);
            float checkRadius = m_maze.getTileSize() * 1.0f;

            // 检查本地玩家
            if (m_player)
            {
              float dist = std::hypot(m_player->getPosition().x - gridCenter.x, m_player->getPosition().y - gridCenter.y);
              if (dist < checkRadius)
                hasTankAtPos = true;
            }
            // 检查其他玩家
            if (!hasTankAtPos && m_otherPlayer)
            {
              float dist = std::hypot(m_otherPlayer->getPosition().x - gridCenter.x, m_otherPlayer->getPosition().y - gridCenter.y);
              if (dist < checkRadius)
                hasTankAtPos = true;
            }
            // 检查NPC
            if (!hasTankAtPos)
            {
              for (const auto &enemy : m_enemies)
              {
                if (!enemy->isDead())
                {
                  float dist = std::hypot(enemy->getPosition().x - gridCenter.x, enemy->getPosition().y - gridCenter.y);
                  if (dist < checkRadius)
                  {
                    hasTankAtPos = true;
                    break;
                  }
                }
              }
            }

            // 尝试放置墙壁
            if (!hasTankAtPos && m_maze.placeWall(mouseWorldPos))
            {
              m_player->useWallFromBag();
              // 播放放置音效
              AudioManager::getInstance().playSFX(SFXType::MenuConfirm, mouseWorldPos, m_player->getPosition());
              // 发送到网络同步
              NetworkManager::getInstance().sendWallPlace(mouseWorldPos.x, mouseWorldPos.y);
              // 放置成功后自动退出放置模式
              m_placementMode = false;
            }
          }
          else if (mousePressed->button == sf::Mouse::Button::Right)
          {
            // 右键取消放置模式
            m_placementMode = false;
          }
        }
      }
      break;
    }
  }
}

void Game::update(float dt)
{
  if (!m_player)
    return;

  // 获取鼠标在世界坐标中的位置
  sf::Vector2i mousePixelPos = sf::Mouse::getPosition(m_window);
  sf::Vector2f mouseWorldPos = m_window.mapPixelToCoords(mousePixelPos, m_gameView);

  // 保存旧位置用于碰撞检测
  sf::Vector2f oldPos = m_player->getPosition();

  // 获取移动向量
  sf::Vector2f movement = m_player->getMovement(dt);

  // 更新玩家
  m_player->update(dt, mouseWorldPos);

  // 检查玩家与墙壁的碰撞并实现墙壁滑动
  sf::Vector2f newPos = m_player->getPosition();
  float radius = m_player->getCollisionRadius();

  if (m_maze.checkCollision(newPos, radius))
  {
    // 尝试只在 X 方向移动
    sf::Vector2f posX = {oldPos.x + movement.x, oldPos.y};
    // 尝试只在 Y 方向移动
    sf::Vector2f posY = {oldPos.x, oldPos.y + movement.y};

    bool canMoveX = !m_maze.checkCollision(posX, radius);
    bool canMoveY = !m_maze.checkCollision(posY, radius);

    if (canMoveX && canMoveY)
    {
      // 两个方向都能走，选择移动量大的
      if (std::abs(movement.x) > std::abs(movement.y))
        m_player->setPosition(posX);
      else
        m_player->setPosition(posY);
    }
    else if (canMoveX)
    {
      m_player->setPosition(posX);
    }
    else if (canMoveY)
    {
      m_player->setPosition(posY);
    }
    else
    {
      // 两个方向都不能走，退回原位
      m_player->setPosition(oldPos);
    }
  }

  // E键状态已通过事件驱动在 processEvents 中设置

  // 检查是否到达出口（需要按住E键3秒）
  const float EXIT_HOLD_TIME = 3.0f;
  bool atExit = m_maze.isAtExit(m_player->getPosition(), m_player->getCollisionRadius());

  if (atExit)
  {
    m_isAtExitZone = true;

    if (m_eKeyHeld)
    {
      // 正在按住E键
      if (!m_isHoldingExit)
      {
        m_isHoldingExit = true;
        m_exitHoldProgress = 0.f;
      }

      m_exitHoldProgress += dt;

      // 完成确认
      if (m_exitHoldProgress >= EXIT_HOLD_TIME)
      {
        m_gameWon = true;
        m_gameOver = true;
        m_gameState = GameState::Victory;
        m_isHoldingExit = false;
        m_exitHoldProgress = 0.f;
      }
    }
    else
    {
      // 松开E键，取消进度
      if (m_isHoldingExit)
      {
        m_isHoldingExit = false;
        m_exitHoldProgress = 0.f;
      }
    }
  }
  else
  {
    // 离开终点区域，重置状态
    if (m_isAtExitZone)
    {
      m_isAtExitZone = false;
      m_isHoldingExit = false;
      m_exitHoldProgress = 0.f;
    }
  }

  // 更新视角
  updateCamera();

  // 玩家射击
  if (m_player->hasFiredBullet())
  {
    sf::Vector2f bulletPos = m_player->getBulletSpawnPosition();
    float bulletAngle = m_player->getTurretRotation();
    m_bullets.push_back(std::make_unique<Bullet>(bulletPos.x, bulletPos.y, bulletAngle, true));

    // 播放射击音效
    AudioManager::getInstance().playSFX(SFXType::Shoot, bulletPos, m_player->getPosition());
  }

  // 更新敌人
  for (auto &enemy : m_enemies)
  {
    // 单人模式：自动激活检测
    enemy->checkAutoActivation(m_player->getPosition());

    enemy->setTarget(m_player->getPosition());
    enemy->update(dt, m_maze);

    // 只有激活的敌人才射击
    if (enemy->shouldShoot())
    {
      sf::Vector2f bulletPos = enemy->getGunPosition();
      float bulletAngle = enemy->getTurretAngle();
      auto bullet = std::make_unique<Bullet>(bulletPos.x, bulletPos.y, bulletAngle, false, sf::Color::Red);
      bullet->setDamage(12.5f); // NPC子弹伤害12.5%
      m_bullets.push_back(std::move(bullet));

      // 播放射击音效（基于玩家位置的距离衰减）
      AudioManager::getInstance().playSFX(SFXType::Shoot, bulletPos, m_player->getPosition());
    }
  }

  // 更新迷宫
  m_maze.update(dt);

  // 更新子弹
  for (auto &bullet : m_bullets)
  {
    bullet->update(dt);
  }

  // 删除超出范围的子弹
  sf::Vector2f mazeSize = m_maze.getSize();
  m_bullets.erase(
      std::remove_if(m_bullets.begin(), m_bullets.end(),
                     [&mazeSize](const std::unique_ptr<Bullet> &b)
                     {
                       if (!b->isAlive())
                         return true;
                       sf::Vector2f pos = b->getPosition();
                       return pos.x < -50 || pos.x > mazeSize.x + 50 ||
                              pos.y < -50 || pos.y > mazeSize.y + 50;
                     }),
      m_bullets.end());

  // 检查碰撞
  checkCollisions();

  // 移除死亡的敌人
  m_enemies.erase(
      std::remove_if(m_enemies.begin(), m_enemies.end(),
                     [](const std::unique_ptr<Enemy> &e)
                     { return e->isDead(); }),
      m_enemies.end());

  // 检查玩家是否死亡
  if (m_player->isDead())
  {
    m_gameOver = true;
    m_gameState = GameState::GameOver;
  }
}

void Game::updateCamera()
{
  if (!m_player)
    return;

  sf::Vector2f playerPos = m_player->getPosition();

  // 获取鼠标在世界坐标中的位置
  sf::Vector2i mousePixelPos = sf::Mouse::getPosition(m_window);
  sf::Vector2f mouseWorldPos = m_window.mapPixelToCoords(mousePixelPos, m_gameView);

  // 计算鼠标与玩家的距离
  sf::Vector2f toMouse = mouseWorldPos - playerPos;
  float mouseDist = std::sqrt(toMouse.x * toMouse.x + toMouse.y * toMouse.y);

  // 根据鼠标距离计算视角偏移量（距离越远偏移越大）
  const float minDist = 100.f; // 小于此距离不偏移
  const float maxDist = 400.f; // 大于此距离最大偏移
  float distFactor = 0.f;
  if (mouseDist > minDist)
  {
    distFactor = std::min((mouseDist - minDist) / (maxDist - minDist), 1.f);
  }

  // 获取炮塔瞄准方向，视角向瞄准方向偏移
  float turretAngle = m_player->getTurretAngle();
  float angleRad = (turretAngle - 90.f) * 3.14159f / 180.f;
  sf::Vector2f lookDir = {std::cos(angleRad), std::sin(angleRad)};

  // 视角偏移量随鼠标距离变化
  float actualLookAhead = m_cameraLookAhead * distFactor;
  sf::Vector2f cameraTarget = playerPos + lookDir * actualLookAhead;

  // 缩放后的视野范围
  float zoomedWidth = LOGICAL_WIDTH * VIEW_ZOOM;
  float zoomedHeight = LOGICAL_HEIGHT * VIEW_ZOOM;

  // 不限制相机边界，允许看到迷宫外的区域（与联机模式保持一致）

  // 平滑插值到目标位置（减少晃动感）
  float dt = 1.f / 60.f; // 假设60fps
  float lerpFactor = 1.f - std::exp(-m_cameraSmoothSpeed * dt);

  // 初始化相机位置
  if (m_currentCameraPos.x == 0.f && m_currentCameraPos.y == 0.f)
  {
    m_currentCameraPos = cameraTarget;
  }
  else
  {
    m_currentCameraPos.x += (cameraTarget.x - m_currentCameraPos.x) * lerpFactor;
    m_currentCameraPos.y += (cameraTarget.y - m_currentCameraPos.y) * lerpFactor;
  }

  // 设置视图中心和缩放
  m_gameView.setCenter(m_currentCameraPos);
  m_gameView.setSize({zoomedWidth, zoomedHeight});
}

void Game::checkCollisions()
{
  CollisionSystem::checkSinglePlayerCollisions(m_player.get(), m_enemies, m_bullets, m_maze);
}

void Game::checkMultiplayerCollisions()
{
  CollisionSystem::checkMultiplayerCollisions(
      m_player.get(), m_otherPlayer.get(), m_enemies, m_bullets, m_maze, m_mpState.isHost);
}

void Game::render()
{
  m_window.clear(sf::Color(30, 30, 30));

  switch (m_gameState)
  {
  case GameState::MainMenu:
    renderMainMenu();
    break;
  case GameState::ModeSelect:
    renderModeSelect();
    break;
  case GameState::Playing:
    renderGame();
    break;
  case GameState::Paused:
    renderGame();
    renderPaused();
    break;
  case GameState::Connecting:
    renderConnecting();
    return; // renderConnecting 已经调用了 display
  case GameState::CreatingRoom:
    renderCreatingRoom();
    return; // renderCreatingRoom 已经调用了 display
  case GameState::WaitingForPlayer:
    renderWaitingForPlayer();
    return; // renderWaitingForPlayer 已经调用了 display
  case GameState::RoomLobby:
    renderRoomLobby();
    return; // renderRoomLobby 已经调用了 display
  case GameState::Multiplayer:
    renderMultiplayer();
    return; // renderMultiplayer 已经调用了 display
  case GameState::GameOver:
  case GameState::Victory:
    renderGame();
    renderGameOver();
    break;
  }

  m_window.display();
}

void Game::renderMainMenu()
{
  m_window.setView(m_uiView);

  // 标题
  sf::Text title(m_font);
  title.setString("TANK MAZE");
  title.setCharacterSize(72);
  title.setFillColor(sf::Color::White);
  title.setStyle(sf::Text::Bold);
  sf::FloatRect titleBounds = title.getLocalBounds();
  title.setPosition({(LOGICAL_WIDTH - titleBounds.size.x) / 2.f, 100.f});
  m_window.draw(title);

  // 菜单选项
  float startY = 220.f;
  float spacing = 45.f;

  // 获取地图大小预设字符串
  std::string mapSizeStr;
  switch (m_mapSizePreset)
  {
  case MapSizePreset::Small:
    mapSizeStr = "Small (31x21, 10 NPCs)";
    break;
  case MapSizePreset::Medium:
    mapSizeStr = "Medium (41x31, 20 NPCs)";
    break;
  case MapSizePreset::Large:
    mapSizeStr = "Large (61x51, 30 NPCs)";
    break;
  case MapSizePreset::Ultra:
    mapSizeStr = "Ultra (121x101, 80 NPCs)";
    break;
  case MapSizePreset::Custom:
    mapSizeStr = "Custom";
    break;
  default:
    mapSizeStr = "Medium";
    break;
  }

  std::vector<std::string> options = {
      "Single Player",
      "Multi Player",
      std::string("Map Size: < ") + mapSizeStr + " >",
      std::string("Map Width: < ") + std::to_string(m_widthOptions[m_widthIndex]) + " >",
      std::string("Map Height: < ") + std::to_string(m_heightOptions[m_heightIndex]) + " >",
      std::string("NPCs: < ") + std::to_string(m_enemyOptions[m_enemyIndex]) + " >",
      "Exit"};

  for (size_t i = 0; i < options.size(); ++i)
  {
    sf::Text optionText(m_font);
    optionText.setString(options[i]);
    optionText.setCharacterSize(32);

    // MapWidth/MapHeight/NPCs 在任何预设下都使用相同视觉样式
    bool isCustomOption = (i == 3 || i == 4 || i == 5); // MapWidth, MapHeight, NPCs

    if (static_cast<int>(i) == static_cast<int>(m_mainMenuOption))
    {
      optionText.setFillColor(sf::Color::Yellow);
      optionText.setString("> " + options[i] + " <");
    }
    else
    {
      optionText.setFillColor(sf::Color(180, 180, 180));
    }

    sf::FloatRect bounds = optionText.getLocalBounds();
    optionText.setPosition({(LOGICAL_WIDTH - bounds.size.x) / 2.f, startY + i * spacing});
    m_window.draw(optionText);
  }

  // 地图预览信息
  sf::Text mapInfo(m_font);
  int totalCells = m_mazeWidth * m_mazeHeight;
  mapInfo.setString("Map: " + std::to_string(m_mazeWidth) + " x " +
                    std::to_string(m_mazeHeight) + " = " +
                    std::to_string(totalCells) + " cells");
  mapInfo.setCharacterSize(20);
  mapInfo.setFillColor(sf::Color(100, 180, 100));
  sf::FloatRect mapInfoBounds = mapInfo.getLocalBounds();
  mapInfo.setPosition({(LOGICAL_WIDTH - mapInfoBounds.size.x) / 2.f, LOGICAL_HEIGHT - 120.f});
  m_window.draw(mapInfo);

  // 提示
  sf::Text hint(m_font);
  hint.setString("W/S: Navigate | A/D: Adjust values | Enter: Select");
  hint.setCharacterSize(18);
  hint.setFillColor(sf::Color(120, 120, 120));
  sf::FloatRect hintBounds = hint.getLocalBounds();
  hint.setPosition({(LOGICAL_WIDTH - hintBounds.size.x) / 2.f, LOGICAL_HEIGHT - 60.f});
  m_window.draw(hint);
}

void Game::renderModeSelect()
{
  m_window.setView(m_uiView);

  // 标题
  sf::Text title(m_font);
  title.setString(m_isMultiplayer ? "MULTIPLAYER" : "SINGLE PLAYER");
  title.setCharacterSize(56);
  title.setFillColor(sf::Color::White);
  title.setStyle(sf::Text::Bold);
  sf::FloatRect titleBounds = title.getLocalBounds();
  title.setPosition({(LOGICAL_WIDTH - titleBounds.size.x) / 2.f, 100.f});
  m_window.draw(title);

  // 副标题
  sf::Text subtitle(m_font);
  subtitle.setString("Select Game Mode");
  subtitle.setCharacterSize(28);
  subtitle.setFillColor(sf::Color(180, 180, 180));
  sf::FloatRect subtitleBounds = subtitle.getLocalBounds();
  subtitle.setPosition({(LOGICAL_WIDTH - subtitleBounds.size.x) / 2.f, 170.f});
  m_window.draw(subtitle);

  // 模式选项
  float startY = 280.f;
  float spacing = 80.f;

  // Escape Mode
  {
    sf::Text optionText(m_font);
    std::string modeStr = "Escape Mode";
    optionText.setCharacterSize(36);

    if (m_gameModeOption == GameModeOption::EscapeMode)
    {
      optionText.setFillColor(sf::Color::Yellow);
      optionText.setString("> " + modeStr + " <");
    }
    else
    {
      optionText.setFillColor(sf::Color(180, 180, 180));
      optionText.setString(modeStr);
    }

    sf::FloatRect bounds = optionText.getLocalBounds();
    optionText.setPosition({(LOGICAL_WIDTH - bounds.size.x) / 2.f, startY});
    m_window.draw(optionText);

    // 模式描述
    sf::Text desc(m_font);
    if (m_isMultiplayer)
    {
      desc.setString("Cooperate with your teammate to escape!");
    }
    else
    {
      desc.setString("Reach the exit to win!");
    }
    desc.setCharacterSize(20);
    desc.setFillColor(sf::Color(100, 180, 100));
    sf::FloatRect descBounds = desc.getLocalBounds();
    desc.setPosition({(LOGICAL_WIDTH - descBounds.size.x) / 2.f, startY + 40.f});
    m_window.draw(desc);
  }

  // Battle Mode
  {
    sf::Text optionText(m_font);
    std::string modeStr = "Battle Mode";
    // Single Player Battle Mode 还在开发中
    if (!m_isMultiplayer)
    {
      modeStr += " [Coming Soon]";
    }
    optionText.setCharacterSize(36);

    if (m_gameModeOption == GameModeOption::BattleMode)
    {
      optionText.setFillColor(!m_isMultiplayer ? sf::Color(180, 180, 100) : sf::Color::Yellow);
      optionText.setString("> " + modeStr + " <");
    }
    else
    {
      optionText.setFillColor(!m_isMultiplayer ? sf::Color(120, 120, 120) : sf::Color(180, 180, 180));
      optionText.setString(modeStr);
    }

    sf::FloatRect bounds = optionText.getLocalBounds();
    optionText.setPosition({(LOGICAL_WIDTH - bounds.size.x) / 2.f, startY + spacing});
    m_window.draw(optionText);

    // 模式描述
    sf::Text desc(m_font);
    desc.setString("Defeat your opponent or reach the exit first!");
    desc.setCharacterSize(20);
    desc.setFillColor(sf::Color(180, 100, 100));
    sf::FloatRect descBounds = desc.getLocalBounds();
    desc.setPosition({(LOGICAL_WIDTH - descBounds.size.x) / 2.f, startY + spacing + 40.f});
    m_window.draw(desc);
  }

  // 暗黑模式选项（单人模式）- 在 Back 上方
  if (!m_isMultiplayer)
  {
    sf::Text darkModeText(m_font);
    std::string darkStr = "Dark Mode: " + std::string(m_darkModeOption ? "ON" : "OFF");
    darkModeText.setCharacterSize(28);
    darkModeText.setFillColor(m_darkModeOption ? sf::Color(200, 100, 255) : sf::Color(150, 150, 150));
    darkModeText.setString(darkStr);
    sf::FloatRect darkBounds = darkModeText.getLocalBounds();
    darkModeText.setPosition({(LOGICAL_WIDTH - darkBounds.size.x) / 2.f, startY + spacing * 2 + 20.f});
    m_window.draw(darkModeText);

    // 暗黑模式描述
    sf::Text darkDesc(m_font);
    darkDesc.setString("Limited vision with fog of war (D to toggle)");
    darkDesc.setCharacterSize(18);
    darkDesc.setFillColor(sf::Color(120, 120, 120));
    sf::FloatRect darkDescBounds = darkDesc.getLocalBounds();
    darkDesc.setPosition({(LOGICAL_WIDTH - darkDescBounds.size.x) / 2.f, startY + spacing * 2 + 55.f});
    m_window.draw(darkDesc);
  }

  // Back
  {
    sf::Text optionText(m_font);
    std::string str = "Back";
    optionText.setCharacterSize(32);

    if (m_gameModeOption == GameModeOption::Back)
    {
      optionText.setFillColor(sf::Color::Yellow);
      optionText.setString("> " + str + " <");
    }
    else
    {
      optionText.setFillColor(sf::Color(150, 150, 150));
      optionText.setString(str);
    }

    sf::FloatRect bounds = optionText.getLocalBounds();
    // 单人模式有暗黑模式选项，Back 位置下移
    float backY = m_isMultiplayer ? (startY + spacing * 2 + 40.f) : (startY + spacing * 2 + 110.f);
    optionText.setPosition({(LOGICAL_WIDTH - bounds.size.x) / 2.f, backY});
    m_window.draw(optionText);
  }

  // 提示
  sf::Text hint(m_font);
  hint.setString("W/S: Game Mode | D: Dark Mode | Enter: Select | ESC: Back");
  hint.setCharacterSize(18);
  hint.setFillColor(sf::Color(120, 120, 120));
  sf::FloatRect hintBounds = hint.getLocalBounds();
  hint.setPosition({(LOGICAL_WIDTH - hintBounds.size.x) / 2.f, LOGICAL_HEIGHT - 60.f});
  m_window.draw(hint);
}

void Game::renderGame()
{
  // 使用游戏视图绘制游戏世界
  m_window.setView(m_gameView);

  // 绘制迷宫
  m_maze.draw(m_window);

  // 如果处于放置模式，绘制预览
  if (m_placementMode && m_player && m_player->getWallsInBag() > 0)
  {
    // 获取鼠标在世界坐标中的位置
    sf::Vector2i mousePixelPos = sf::Mouse::getPosition(m_window);
    sf::Vector2f mouseWorldPos = m_window.mapPixelToCoords(mousePixelPos, m_gameView);

    // 获取对应的格子位置
    GridPos grid = m_maze.worldToGrid(mouseWorldPos);
    sf::Vector2f gridCenter = m_maze.gridToWorld(grid);

    // 检查是否有坦克在该位置
    bool hasTankAtPos = false;
    float checkRadius = m_maze.getTileSize() * 1.0f;

    // 检查玩家
    if (m_player)
    {
      float dist = std::hypot(m_player->getPosition().x - gridCenter.x, m_player->getPosition().y - gridCenter.y);
      if (dist < checkRadius)
        hasTankAtPos = true;
    }
    // 检查另一个玩家（联机模式）
    if (!hasTankAtPos && m_otherPlayer)
    {
      float dist = std::hypot(m_otherPlayer->getPosition().x - gridCenter.x, m_otherPlayer->getPosition().y - gridCenter.y);
      if (dist < checkRadius)
        hasTankAtPos = true;
    }
    // 检查NPC
    if (!hasTankAtPos)
    {
      for (const auto &enemy : m_enemies)
      {
        if (!enemy->isDead())
        {
          float dist = std::hypot(enemy->getPosition().x - gridCenter.x, enemy->getPosition().y - gridCenter.y);
          if (dist < checkRadius)
          {
            hasTankAtPos = true;
            break;
          }
        }
      }
    }

    // 绘制预览方块
    float tileSize = m_maze.getTileSize();
    sf::RectangleShape preview({tileSize - 4.f, tileSize - 4.f});
    preview.setPosition({gridCenter.x - (tileSize - 4.f) / 2.f, gridCenter.y - (tileSize - 4.f) / 2.f});

    if (!hasTankAtPos && m_maze.canPlaceWall(mouseWorldPos))
    {
      // 可放置：绿色半透明
      preview.setFillColor(sf::Color(100, 200, 100, 150));
      preview.setOutlineColor(sf::Color(50, 150, 50, 200));
    }
    else
    {
      // 不可放置：红色半透明
      preview.setFillColor(sf::Color(200, 100, 100, 150));
      preview.setOutlineColor(sf::Color(150, 50, 50, 200));
    }
    preview.setOutlineThickness(2.f);
    m_window.draw(preview);
  }

  // 绘制子弹
  for (const auto &bullet : m_bullets)
  {
    bullet->draw(m_window);
  }

  // 绘制玩家
  if (m_player)
  {
    m_player->draw(m_window);
  }

  // 绘制敌人（跳过死亡的）
  for (const auto &enemy : m_enemies)
  {
    if (!enemy->isDead())
    {
      enemy->draw(m_window);
      enemy->drawHealthBar(m_window);
    }
  }

  // 单人模式暗黑模式遮罩（在游戏世界上方，UI下方）
  if (!m_isMultiplayer)
  {
    if (m_darkModeOption)
      renderDarkModeOverlay();
    else
      renderMinimap();
  }

  // 渲染终点交互提示和进度（单人模式，在终点区域内时）
  if (!m_isMultiplayer && m_isAtExitZone && m_player && !m_gameOver)
  {
    sf::Vector2f exitPos = m_maze.getExitPosition();

    if (m_isHoldingExit)
    {
      // 显示确认进度条
      const float EXIT_HOLD_TIME = 3.0f;
      float progress = m_exitHoldProgress / EXIT_HOLD_TIME;

      // 进度条背景
      sf::RectangleShape bgBar({80.f, 10.f});
      bgBar.setFillColor(sf::Color(50, 50, 50, 200));
      bgBar.setPosition({exitPos.x - 40.f, exitPos.y - 60.f});
      m_window.draw(bgBar);

      // 进度条
      sf::RectangleShape progressBar({80.f * progress, 10.f});
      progressBar.setFillColor(sf::Color(50, 200, 255, 255));
      progressBar.setPosition({exitPos.x - 40.f, exitPos.y - 60.f});
      m_window.draw(progressBar);

      // 显示确认中文字
      sf::Text confirmText(m_font);
      confirmText.setString("Exiting...");
      confirmText.setCharacterSize(16);
      confirmText.setFillColor(sf::Color::Cyan);
      sf::FloatRect bounds = confirmText.getLocalBounds();
      confirmText.setPosition({exitPos.x - bounds.size.x / 2.f, exitPos.y - 85.f});
      m_window.draw(confirmText);
    }
    else
    {
      // 显示按 E 确认提示
      sf::Text exitHint(m_font);
      exitHint.setString("Hold E to exit");
      exitHint.setCharacterSize(16);
      exitHint.setFillColor(sf::Color::Cyan);
      sf::FloatRect bounds = exitHint.getLocalBounds();
      exitHint.setPosition({exitPos.x - bounds.size.x / 2.f, exitPos.y - 60.f});
      m_window.draw(exitHint);
    }
  }

  // 切换到 UI 视图绘制 UI
  m_window.setView(m_uiView);

  // 绘制玩家 UI（血条）
  if (m_player)
  {
    m_player->drawUI(m_window);

    float uiY = 50.f; // UI 起始 Y 位置

    // Battle 模式显示金币数量
    if (m_gameModeOption == GameModeOption::BattleMode)
    {
      sf::Text coinsText(m_font);
      coinsText.setString("Coins: " + std::to_string(m_player->getCoins()));
      coinsText.setCharacterSize(24);
      coinsText.setFillColor(sf::Color(255, 200, 50)); // 金色
      coinsText.setPosition({20.f, uiY});
      m_window.draw(coinsText);
      uiY += 30.f;
    }

    // 绘制背包中的墙壁数量
    sf::Text wallsText(m_font);
    wallsText.setString("Walls: " + std::to_string(m_player->getWallsInBag()));
    wallsText.setCharacterSize(24);
    wallsText.setFillColor(sf::Color(139, 90, 43)); // 棕色
    wallsText.setPosition({20.f, uiY});
    m_window.draw(wallsText);
    uiY += 30.f;

    // Escape 模式或暗黑模式下显示剩余存活的敌人数量
    if ((m_gameModeOption == GameModeOption::EscapeMode || m_darkModeOption) && !m_isMultiplayer)
    {
      int aliveEnemies = 0;
      for (const auto &enemy : m_enemies)
      {
        if (!enemy->isDead())
          aliveEnemies++;
      }
      sf::Text enemyCountText(m_font);
      enemyCountText.setString("Enemies: " + std::to_string(aliveEnemies));
      enemyCountText.setCharacterSize(24);
      enemyCountText.setFillColor(sf::Color(255, 100, 100)); // 红色
      enemyCountText.setPosition({20.f, uiY});
      m_window.draw(enemyCountText);
      uiY += 30.f;
    }

    // 如果处于放置模式，显示提示
    if (m_placementMode)
    {
      sf::Text placeHint(m_font);
      placeHint.setString("[PLACEMENT MODE] Click to place wall, Space to cancel");
      placeHint.setCharacterSize(20);
      placeHint.setFillColor(sf::Color::Yellow);
      sf::FloatRect hintBounds = placeHint.getLocalBounds();
      placeHint.setPosition({(LOGICAL_WIDTH - hintBounds.size.x) / 2.f, 20.f});
      m_window.draw(placeHint);
    }
    else if (m_player->getWallsInBag() > 0)
    {
      // 提示可以按 SPACE 进入放置模式
      sf::Text bagHint(m_font);
      bagHint.setString("Press SPACE to place walls");
      bagHint.setCharacterSize(18);
      bagHint.setFillColor(sf::Color(150, 150, 150));
      bagHint.setPosition({20.f, uiY});
      m_window.draw(bagHint);
    }
  }
}

void Game::renderPaused()
{
  m_window.setView(m_uiView);

  // 半透明背景
  sf::RectangleShape overlay({static_cast<float>(LOGICAL_WIDTH), static_cast<float>(LOGICAL_HEIGHT)});
  overlay.setFillColor(sf::Color(0, 0, 0, 180));
  m_window.draw(overlay);

  // 暂停标题
  sf::Text title(m_font);
  title.setString("PAUSED");
  title.setCharacterSize(72);
  title.setFillColor(sf::Color::Yellow);
  title.setStyle(sf::Text::Bold);
  sf::FloatRect titleBounds = title.getLocalBounds();
  title.setPosition({(LOGICAL_WIDTH - titleBounds.size.x) / 2.f, LOGICAL_HEIGHT / 2.f - 100.f});
  m_window.draw(title);

  // 提示信息
  sf::Text hint(m_font);
  hint.setString("Press P or ESC to resume\nPress Q to quit to menu");
  hint.setCharacterSize(28);
  hint.setFillColor(sf::Color::White);
  sf::FloatRect hintBounds = hint.getLocalBounds();
  hint.setPosition({(LOGICAL_WIDTH - hintBounds.size.x) / 2.f, LOGICAL_HEIGHT / 2.f + 20.f});
  m_window.draw(hint);
}

void Game::renderGameOver()
{
  m_window.setView(m_uiView);

  // 半透明背景
  sf::RectangleShape overlay({static_cast<float>(LOGICAL_WIDTH), static_cast<float>(LOGICAL_HEIGHT)});
  overlay.setFillColor(sf::Color(0, 0, 0, 150));
  m_window.draw(overlay);

  sf::Text text(m_font);

  // 根据游戏模式和结果显示不同文本
  if (m_mpState.isMultiplayer)
  {
    if (m_gameState == GameState::Victory)
    {
      text.setString("VICTORY!");
      text.setFillColor(sf::Color::Green);
    }
    else
    {
      text.setString("DEFEATED!");
      text.setFillColor(sf::Color::Red);
    }
  }
  else
  {
    // 单机模式
    if (m_gameWon)
    {
      text.setString("YOU WIN!");
      text.setFillColor(sf::Color::Green);
    }
    else
    {
      text.setString("GAME OVER");
      text.setFillColor(sf::Color::Red);
    }
  }

  text.setCharacterSize(64);
  sf::FloatRect bounds = text.getLocalBounds();
  text.setPosition({(LOGICAL_WIDTH - bounds.size.x) / 2.f, LOGICAL_HEIGHT / 2.f - 80.f});
  m_window.draw(text);

  sf::Text hint(m_font);

  // 多人模式显示不同的提示
  if (m_mpState.isMultiplayer)
  {
    hint.setString("Press R to return to room, ESC for menu");
  }
  else
  {
    hint.setString("Press R to restart, ESC for menu");
  }

  hint.setCharacterSize(28);
  hint.setFillColor(sf::Color::White);
  sf::FloatRect hintBounds = hint.getLocalBounds();
  hint.setPosition({(LOGICAL_WIDTH - hintBounds.size.x) / 2.f, LOGICAL_HEIGHT / 2.f + 20.f});
  m_window.draw(hint);
}

void Game::setupNetworkCallbacks()
{
  auto &net = NetworkManager::getInstance();

  net.setOnConnected([this]()
                     { m_mpState.connectionStatus = "Connected! Choose action:"; });

  net.setOnDisconnected([this]()
                        {
    if (m_gameState == GameState::Multiplayer || 
        m_gameState == GameState::WaitingForPlayer ||
        m_gameState == GameState::RoomLobby ||
        m_gameState == GameState::Connecting) {
      m_mpState.connectionStatus = "Disconnected from server";
      resetGame();
    } });

  // 对方看到出口，同步播放高潮BGM
  net.setOnClimaxStart([this]()
                       {
    if (!m_exitVisible) {
      m_exitVisible = true;
      AudioManager::getInstance().playBGM(BGMType::Climax);
    } });

  // 对方玩家离开房间，返回房间大厅
  net.setOnPlayerLeft([this](bool becameHost)
                      {
    // 清理游戏状态
    m_otherPlayer.reset();
    m_enemies.clear();
    m_bullets.clear();
    
    // 重置多人游戏状态
    m_mpState.localPlayerReachedExit = false;
    m_mpState.otherPlayerReachedExit = false;
    m_mpState.multiplayerWin = false;
    m_mpState.localPlayerDead = false;
    m_mpState.otherPlayerDead = false;
    m_mpState.otherPlayerInRoom = false;
    m_mpState.otherPlayerReady = false;
    m_mpState.otherPlayerIP = "";
    m_gameOver = false;
    m_gameWon = false;
    
    // 更新房主状态
    if (becameHost) {
      m_mpState.isHost = true;
      m_mpState.localPlayerReady = true;  // 新房主默认准备
      
      // 同步 m_gameModeOption 与 m_mpState.isEscapeMode
      // 这样在游戏开始时不会错误地覆盖 isEscapeMode
      m_gameModeOption = m_mpState.isEscapeMode ? GameModeOption::EscapeMode : GameModeOption::BattleMode;
      
      // 保留当前的游戏设置（尺寸、NPC数量、模式）
      // 新地图会在开始游戏时生成
      if (!m_mpState.generatedMazeData.empty()) {
        m_mpState.mazeHeight = static_cast<int>(m_mpState.generatedMazeData.size());
        m_mpState.mazeWidth = static_cast<int>(m_mpState.generatedMazeData[0].size());
        
        // 重新计算NPC数量
        int npcCount = 0;
        for (const auto& row : m_mpState.generatedMazeData) {
          for (char c : row) {
            if (c == 'X') npcCount++;
          }
        }
        m_mpState.npcCount = npcCount;
      }
      
      std::cout << "[DEBUG] Became host. Settings: " << m_mpState.mazeWidth << "x" << m_mpState.mazeHeight 
                << ", NPCs: " << m_mpState.npcCount << ", Mode: " << (m_mpState.isEscapeMode ? "Escape" : "Battle") 
                << " (New map will be generated on game start)" << std::endl;
    }
    
    // 返回房间大厅
    m_gameState = GameState::RoomLobby;
    if (becameHost) {
      m_mpState.connectionStatus = "Other player left. You are now the host.";
    } else {
      m_mpState.connectionStatus = "Other player left. Waiting for new player...";
    }
    
    std::cout << "[DEBUG] Player left, becameHost=" << becameHost << std::endl; });

  net.setOnRoomCreated([this](const std::string &roomCode)
                       {
    m_mpState.roomCode = roomCode;
    m_mpState.isHost = true;
    m_mpState.localPlayerReady = true;  // 房主默认准备
    m_mpState.otherPlayerReady = false;
    m_mpState.otherPlayerInRoom = false;
    
    // 房主先设置游戏模式
    m_mpState.isEscapeMode = (m_gameModeOption == GameModeOption::EscapeMode);
    
    // 保存迷宫设置
    m_mpState.mazeWidth = m_mazeWidth;
    m_mpState.mazeHeight = m_mazeHeight;
    m_mpState.npcCount = m_enemyOptions[m_enemyIndex];
    
    // 房主生成迷宫（使用菜单选择的NPC数量，联机模式生成特殊方块，根据游戏模式决定墙体类型）
    int npcCount = m_enemyOptions[m_enemyIndex];
    std::cout << "[DEBUG] Creating room with " << npcCount << " NPCs, isEscapeMode=" << m_mpState.isEscapeMode << std::endl;
    m_maze.generateRandomMaze(m_mazeWidth, m_mazeHeight, 0, npcCount, true, m_mpState.isEscapeMode);
    m_mpState.generatedMazeData = m_maze.getMazeData();
    
    // 检查迷宫数据中是否有敌人标记 'X'
    int xCount = 0;
    for (const auto& row : m_mpState.generatedMazeData) {
      for (char c : row) {
        if (c == 'X') xCount++;
      }
    }
    std::cout << "[DEBUG] Maze data contains " << xCount << " enemy markers (X)" << std::endl;
    
    NetworkManager::getInstance().sendMazeData(m_mpState.generatedMazeData, m_mpState.isEscapeMode, m_mpState.isDarkMode);
    
    // 进入房间大厅
    m_gameState = GameState::RoomLobby;
    m_mpState.connectionStatus = "Room created! Code: " + roomCode; });

  net.setOnRoomJoined([this](const std::string &roomCode)
                      {
    m_mpState.roomCode = roomCode;
    m_mpState.isHost = false;
    m_mpState.localPlayerReady = false;  // 非房主需要手动准备
    m_mpState.otherPlayerReady = true;   // 房主默认准备
    m_mpState.otherPlayerInRoom = true;  // 房主在房间中
    
    // 进入房间大厅
    m_gameState = GameState::RoomLobby;
    m_mpState.connectionStatus = "Joined room: " + roomCode; });

  net.setOnMazeData([this](const std::vector<std::string> &mazeData, bool isDarkMode)
                    {
    // 收到迷宫数据（非房主）
    m_mpState.generatedMazeData = mazeData;
    m_mpState.isDarkMode = isDarkMode;
    
    // 解析迷宫尺寸
    if (!mazeData.empty()) {
      m_mpState.mazeHeight = static_cast<int>(mazeData.size());
      m_mpState.mazeWidth = static_cast<int>(mazeData[0].size());
      
      // 计算NPC数量
      int npcCount = 0;
      for (const auto& row : mazeData) {
        for (char c : row) {
          if (c == 'X') npcCount++;
        }
      }
      m_mpState.npcCount = npcCount;
    }
    
    m_mpState.connectionStatus = "Maze received!"; });

  net.setOnGameModeReceived([this](bool isEscapeMode)
                            {
    // 非房主接收游戏模式
    m_mpState.isEscapeMode = isEscapeMode;
    std::cout << "[DEBUG] Received game mode: " << (isEscapeMode ? "Escape" : "Battle") << std::endl; });

  net.setOnRequestMaze([this]()
                       {
    // 服务器请求迷宫数据（房主收到）
    if (m_mpState.isHost && !m_mpState.generatedMazeData.empty()) {
      NetworkManager::getInstance().sendMazeData(m_mpState.generatedMazeData, m_mpState.isEscapeMode, m_mpState.isDarkMode);
    } });

  net.setOnGameStart([this]()
                     {
    m_mpState.isMultiplayer = true;
    
    // 使用已接收/生成的迷宫数据
    if (!m_mpState.generatedMazeData.empty()) {
      m_maze.loadFromString(m_mpState.generatedMazeData);
    }
    
    // 获取两个出生点位置（从迷宫数据中解析的 '1' 和 '2' 标记）
    sf::Vector2f spawn1Pos = m_maze.getSpawn1Position();
    sf::Vector2f spawn2Pos = m_maze.getSpawn2Position();
    
    // 如果没有设置出生点，回退到起点
    if (spawn1Pos.x == 0 && spawn1Pos.y == 0) {
      spawn1Pos = m_maze.getPlayerStartPosition();
    }
    if (spawn2Pos.x == 0 && spawn2Pos.y == 0) {
      spawn2Pos = m_maze.getPlayerStartPosition();
    }
    
    // 房主使用 spawn1，非房主使用 spawn2
    sf::Vector2f mySpawn = m_mpState.isHost ? spawn1Pos : spawn2Pos;
    sf::Vector2f otherSpawn = m_mpState.isHost ? spawn2Pos : spawn1Pos;
    
    // 设置游戏模式：房主根据菜单选择，非房主已经通过网络接收
    if (m_mpState.isHost) {
      m_mpState.isEscapeMode = (m_gameModeOption == GameModeOption::EscapeMode);
    }
    // 非房主的 isEscapeMode 已在 setOnGameModeReceived 回调中设置
    
    m_mpState.localPlayerDead = false;
    m_mpState.otherPlayerDead = false;
    m_mpState.isRescuing = false;
    m_mpState.beingRescued = false;
    m_mpState.rescueProgress = 0.f;
    m_mpState.fKeyHeld = false;
    m_mpState.canRescue = false;
    
    // 重置终点交互状态
    m_mpState.isAtExitZone = false;
    m_mpState.isHoldingExit = false;
    m_mpState.exitHoldProgress = 0.f;
    m_mpState.eKeyHeld = false;
    
    // 创建本地玩家并加载贴图
    std::string resPath = getResourcePath();
    m_player = std::make_unique<Tank>();
    m_player->loadTextures(resPath + "tank_assets/PNG/Hulls_Color_A/Hull_01.png",
                           resPath + "tank_assets/PNG/Weapon_Color_A/Gun_01.png");
    m_player->setPosition(mySpawn);
    m_player->setScale(m_tankScale);
    m_player->setCoins(10);  // 初始10个金币
    
    // 设置第二个玩家（另一个客户端）- 使用不同颜色贴图
    m_otherPlayer = std::make_unique<Tank>();
    m_otherPlayer->loadTextures(resPath + "tank_assets/PNG/Hulls_Color_B/Hull_01.png",
                                resPath + "tank_assets/PNG/Weapon_Color_B/Gun_01.png");
    m_otherPlayer->setPosition(otherSpawn);
    m_otherPlayer->setScale(m_tankScale);
    
    // 设置阵营：Escape 模式下双方同队，Battle 模式下对立
    if (m_mpState.isEscapeMode) {
      // Escape 模式：双方都是队伍1，队友之间不会伤害
      m_player->setTeam(1);
      m_otherPlayer->setTeam(1);
      std::cout << "[DEBUG] Escape mode: Both players team=1" << std::endl;
    } else {
      // Battle 模式：对立阵营
      m_player->setTeam(m_mpState.isHost ? 1 : 2);
      m_otherPlayer->setTeam(m_mpState.isHost ? 2 : 1);
      std::cout << "[DEBUG] Battle mode: localTeam=" << m_player->getTeam() 
                << ", otherTeam=" << m_otherPlayer->getTeam() << std::endl;
    }
    
    m_mpState.localPlayerReachedExit = false;
    m_mpState.otherPlayerReachedExit = false;
    
    // 多人模式：生成NPC坦克
    m_enemies.clear();
    
    // 调试：检查敌人生成点数量
    const auto& spawnPoints = m_maze.getEnemySpawnPoints();
    std::cout << "[DEBUG] Multiplayer: Enemy spawn points count = " << spawnPoints.size() << std::endl;
    
    spawnEnemies();  // 使用现有的敌人生成逻辑
    
    std::cout << "[DEBUG] Multiplayer: Enemies spawned = " << m_enemies.size() << std::endl;
    
    // 设置NPC状态
    int npcId = 0;
    for (auto& enemy : m_enemies) {
      enemy->setId(npcId++);
      if (m_mpState.isEscapeMode) {
        // Escape 模式：NPC自动激活，攻击玩家（和单人模式一样）
        // NPC会通过 checkAutoActivation 自动激活
      } else {
        // Battle 模式：需要手动激活
      }
    }
    
    m_bullets.clear();
    m_mpState.nearbyNpcIndex = -1;
    
    // 初始化相机位置和缩放
    m_gameView.setCenter(spawn1Pos);
    m_gameView.setSize({LOGICAL_WIDTH * VIEW_ZOOM, LOGICAL_HEIGHT * VIEW_ZOOM});
    m_currentCameraPos = spawn1Pos;
    
    // 重置终点可见状态并播放开始BGM
    m_exitVisible = false;
    AudioManager::getInstance().playBGM(BGMType::Start);
    
    m_gameState = GameState::Multiplayer; });

  net.setOnPlayerUpdate([this](const PlayerState &state)
                        {
    if (m_otherPlayer) {
      m_otherPlayer->setPosition({state.x, state.y});
      m_otherPlayer->setRotation(state.rotation);
      m_otherPlayer->setTurretRotation(state.turretAngle);
      m_otherPlayer->setHealth(state.health);
      m_mpState.otherPlayerReachedExit = state.reachedExit;
      
      // 同步死亡状态：
      // - 如果对方从死亡变为存活（被救援），更新状态
      // - 如果对方从存活变为死亡，更新状态
      // - 如果本地正在救援且对方发送死亡状态，保持死亡（这是预期的）
      bool wasDead = m_mpState.otherPlayerDead;
      bool nowDead = state.isDead;
      
      // 始终同步死亡状态，除非本地刚刚完成救援
      // （在救援完成时，本地已经设置 otherPlayerDead=false，
      //  但对方可能还没发送更新后的状态）
      m_mpState.otherPlayerDead = nowDead;
      
      // 如果对方复活了，取消救援状态
      if (m_mpState.isEscapeMode && wasDead && !nowDead) {
        m_mpState.isRescuing = false;
        m_mpState.rescueProgress = 0.f;
      }
    } });

  net.setOnPlayerShoot([this](float x, float y, float angle)
                       {
    // 创建另一个玩家的子弹 - 紫色
    auto bullet = std::make_unique<Bullet>(x, y, angle, false, GameColors::EnemyPlayerBullet);
    bullet->setOwner(BulletOwner::OtherPlayer);  // 标记为对方玩家的子弹
    // 对方玩家的 team 和 otherPlayer 一样
    if (m_otherPlayer) {
      bullet->setTeam(m_otherPlayer->getTeam());
    }
    m_bullets.push_back(std::move(bullet));
    
    // 播放对方射击音效（基于本地玩家位置的距离衰减）
    if (m_player)
    {
      AudioManager::getInstance().playSFX(SFXType::Shoot, {x, y}, m_player->getPosition());
    } });

  net.setOnGameResult([this](bool otherPlayerResult)
                      {
    // 收到对方发来的游戏结果（otherPlayerResult 是对方认为的自己的结果）
    // Escape 模式：队友，结果相同（对方输=我也输，对方赢=我也赢）
    // Battle 模式：对手，结果相反（对方输=我赢，对方赢=我输）
    
    std::cout << "[DEBUG] Received GameResult: otherPlayerResult=" << otherPlayerResult 
              << ", isEscapeMode=" << m_mpState.isEscapeMode 
              << ", isHost=" << m_mpState.isHost << std::endl;
    
    bool localResult;
    if (m_mpState.isEscapeMode) {
      // Escape 模式：队友关系，结果相同
      localResult = otherPlayerResult;
    } else {
      // Battle 模式：对手关系，结果相反
      localResult = !otherPlayerResult;
    }
    
    std::cout << "[DEBUG] Final localResult=" << localResult << std::endl;
    
    m_mpState.multiplayerWin = localResult;
    m_gameState = localResult ? GameState::Victory : GameState::GameOver; });

  net.setOnRestartRequest([this]()
                          {
                            // 对方已返回房间大厅
                            // 不强制本地也返回，只是知道对方已经在房间大厅等待
                            // 本地玩家需要自己按 R 返回
                            // 这里不做任何状态改变，让玩家自己决定何时返回
                          });

  // NPC同步回调
  net.setOnNpcActivate([this](int npcId, int team, int activatorId)
                       {
    // 对方激活了NPC
    if (npcId >= 0 && npcId < static_cast<int>(m_enemies.size())) {
      // 检查 NPC 是否已经被激活（避免重复激活）
      if (!m_enemies[npcId]->isActivated()) {
        m_enemies[npcId]->activate(team, activatorId);
        std::cout << "[DEBUG] Remote NPC " << npcId << " activated with team " << team 
                  << ", activatorId " << activatorId << std::endl;
        
        // 如果是房主收到激活请求，需要转发给所有客户端（包括原发送者）
        // 确保双方同步
        if (m_mpState.isHost) {
          NetworkManager::getInstance().sendNpcActivate(npcId, team, activatorId);
          std::cout << "[DEBUG] Host forwarding NPC activation to all clients" << std::endl;
        }
      }
    } });

  net.setOnNpcUpdate([this](const NpcState &state)
                     {
    // 更新NPC状态（仅非房主接收）- 直接设置位置，不使用插值
    if (!m_mpState.isHost && state.id >= 0 && state.id < static_cast<int>(m_enemies.size())) {
      auto& npc = m_enemies[state.id];
      
      // 如果 NPC 在本地已经死亡，不要让远程数据覆盖
      if (npc->isDead()) {
        return;
      }
      
      // 直接设置NPC位置、旋转、炮塔角度
      npc->setPosition({state.x, state.y});
      npc->setRotation(state.rotation);
      npc->setTurretRotation(state.turretAngle);
      
      // 更新血量（只有当远程血量更低时才更新）
      if (state.health < npc->getHealth()) {
        npc->setHealth(state.health);
      }
      
      // 更新激活状态
      if (state.activated && !npc->isActivated()) {
        npc->activate(state.team);
      }
    } });

  net.setOnNpcShoot([this](int npcId, float x, float y, float angle)
                    {
    // NPC射击（创建子弹）- 非房主接收
    if (!m_mpState.isHost) {
      int npcTeam = 0;
      if (npcId >= 0 && npcId < static_cast<int>(m_enemies.size())) {
        npcTeam = m_enemies[npcId]->getTeam();
      }
      // NPC子弹颜色：Escape模式全红（敌方），Battle模式根据team判断
      // 己方NPC（team与本地玩家相同）浅蓝色，敌方NPC红色
      sf::Color bulletColor;
      if (m_mpState.isEscapeMode) {
        bulletColor = GameColors::EnemyNpcBullet;  // Escape模式所有NPC都是敌方
      } else {
        int localTeam = m_player ? m_player->getTeam() : 1;
        bulletColor = (npcTeam == localTeam) ? GameColors::AllyNpcBullet : GameColors::EnemyNpcBullet;
      }
      // NPC子弹使用 BulletOwner::Enemy 标识，并设置阵营
      auto bullet = std::make_unique<Bullet>(x, y, angle, false, bulletColor);
      bullet->setTeam(npcTeam);
      bullet->setDamage(12.5f);  // NPC子弹伤害12.5%
      m_bullets.push_back(std::move(bullet));
      
      // 播放NPC射击音效（基于本地玩家位置的距离衰减）
      if (m_player)
      {
        AudioManager::getInstance().playSFX(SFXType::Shoot, {x, y}, m_player->getPosition());
      }
    } });

  net.setOnNpcDamage([this](int npcId, float damage)
                     {
    // NPC受伤（对方玩家造成的伤害）
    if (npcId >= 0 && npcId < static_cast<int>(m_enemies.size())) {
      auto& npc = m_enemies[npcId];
      // 只有 NPC 还活着时才处理伤害
      if (!npc->isDead()) {
        npc->takeDamage(damage);
        
        // 房主收到非房主的伤害消息后，需要再同步给非房主
        // 因为服务器只转发给"其他玩家"，非房主收不到自己发的消息
        if (m_mpState.isHost) {
          NetworkManager::getInstance().sendNpcDamage(npcId, damage);
        }
        
        // 如果 NPC 死亡，播放爆炸音效
        if (npc->isDead() && m_player) {
          AudioManager::getInstance().playSFX(SFXType::Explode, npc->getPosition(), m_player->getPosition());
        }
      }
    } });

  net.setOnWallPlace([this](float x, float y)
                     {
    // 对方放置了墙壁
    m_maze.placeWall({x, y}); });

  net.setOnWallDamage([this](int row, int col, float damage, bool destroyed, int attribute, int destroyerId)
                      {
    // 非房主接收墙壁伤害同步
    if (!m_mpState.isHost) {
      WallDestroyResult result = m_maze.applyWallDamage(row, col, damage, destroyed);
      
      // 如果墙被摧毁，检查是否是本地玩家（非房主）的子弹造成的
      // destroyerId: 0=房主, 1=非房主（本地玩家）
      if (destroyed && result.destroyed && m_player) {
        sf::Vector2f listenerPos = m_player->getPosition();
        WallAttribute attr = static_cast<WallAttribute>(attribute);
        
        // 如果是非房主（本地玩家）打掉的墙，给本地玩家加增益
        if (destroyerId == 1) {
          switch (attr) {
            case WallAttribute::Gold:
              m_player->addCoins(2);  // 金色墙：获得2金币
              AudioManager::getInstance().playSFX(SFXType::CollectCoins, result.position, listenerPos);
              break;
            case WallAttribute::Heal:
              m_player->heal(0.25f);  // 治疗墙：恢复25%血量
              AudioManager::getInstance().playSFX(SFXType::Bingo, result.position, listenerPos);
              break;
            case WallAttribute::None:
              m_player->addWallToBag();  // 棕色墙：收集到背包
              AudioManager::getInstance().playSFX(SFXType::WallBroken, result.position, listenerPos);
              break;
            default:
              break;
          }
        } else {
          // 房主打掉的墙，只播放音效，不给本地玩家加增益
          switch (attr) {
            case WallAttribute::Gold:
              AudioManager::getInstance().playSFX(SFXType::CollectCoins, result.position, listenerPos);
              break;
            case WallAttribute::Heal:
              AudioManager::getInstance().playSFX(SFXType::Bingo, result.position, listenerPos);
              break;
            case WallAttribute::None:
              AudioManager::getInstance().playSFX(SFXType::WallBroken, result.position, listenerPos);
              break;
            default:
              break;
          }
        }
      }
    } });

  // Escape 模式救援回调
  net.setOnRescueStart([this]()
                       {
    // 对方开始救援我
    m_mpState.beingRescued = true;
    m_mpState.rescueProgress = 0.f; });

  net.setOnRescueProgress([this](float progress)
                          {
    // 更新救援进度（progress 是 0-1 的比例，转换为秒数）
    m_mpState.rescueProgress = progress * 3.0f; });

  net.setOnRescueComplete([this]()
                          {
    // 救援完成，复活
    std::cout << "[DEBUG] Received RescueComplete! Reviving local player." << std::endl;
    m_mpState.localPlayerDead = false;
    m_mpState.beingRescued = false;
    m_mpState.rescueProgress = 0.f;
    if (m_player) {
      m_player->setHealth(50.f);  // 复活时50%血量
      std::cout << "[DEBUG] Set local player health to 50, isDead=" << m_player->isDead() << std::endl;
    } });

  net.setOnRescueCancel([this]()
                        {
    // 对方取消救援
    m_mpState.beingRescued = false;
    m_mpState.rescueProgress = 0.f; });

  net.setOnPlayerReady([this](bool isReady)
                       {
    // 收到对方准备状态
    m_mpState.otherPlayerReady = isReady;
    std::cout << "[DEBUG] Other player ready: " << isReady << std::endl; });

  net.setOnRoomInfo([this](const std::string &hostIP, const std::string &guestIP, bool guestReady, bool isDarkMode)
                    {
    // 收到房间信息
    if (m_mpState.isHost) {
      m_mpState.localPlayerIP = hostIP;
      m_mpState.otherPlayerIP = guestIP;
      m_mpState.otherPlayerInRoom = !guestIP.empty();
      m_mpState.otherPlayerReady = guestReady;
    } else {
      m_mpState.otherPlayerIP = hostIP;
      m_mpState.localPlayerIP = guestIP;
      m_mpState.otherPlayerInRoom = true;  // 房主肯定在
      m_mpState.otherPlayerReady = true;   // 房主默认准备
    }
    m_mpState.isDarkMode = isDarkMode;
    std::cout << "[DEBUG] Room info: host=" << hostIP << ", guest=" << guestIP << ", guestReady=" << guestReady << ", darkMode=" << isDarkMode << std::endl; });

  net.setOnError([this](const std::string &error)
                 { m_mpState.connectionStatus = "Error: " + error; });
}

void Game::processConnectingEvents(const sf::Event &event)
{
  if (const auto *keyPressed = event.getIf<sf::Event::KeyPressed>())
  {
    if (keyPressed->code == sf::Keyboard::Key::Escape)
    {
      NetworkManager::getInstance().disconnect();
      resetGame();
      return;
    }

    if (keyPressed->code == sf::Keyboard::Key::Enter)
    {
      switch (m_inputMode)
      {
      case InputMode::ServerIP:
        m_serverIP = m_inputText;
        if (NetworkManager::getInstance().connect(m_serverIP, 9999))
        {
          AudioManager::getInstance().playSFXGlobal(SFXType::MenuConfirm);
          m_mpState.connectionStatus = "Connected! Enter room code or press C to create:";
          m_inputMode = InputMode::RoomCode;
          m_inputText = "";
        }
        else
        {
          m_mpState.connectionStatus = "Failed to connect to " + m_serverIP;
        }
        break;
      case InputMode::RoomCode:
        if (!m_inputText.empty())
        {
          AudioManager::getInstance().playSFXGlobal(SFXType::MenuConfirm);
          NetworkManager::getInstance().joinRoom(m_inputText);
        }
        break;
      default:
        break;
      }
    }

    // 按 C 创建房间
    // 按 C 进入创建房间模式（选择游戏模式）
    if (keyPressed->code == sf::Keyboard::Key::C &&
        m_inputMode == InputMode::RoomCode &&
        NetworkManager::getInstance().isConnected())
    {
      AudioManager::getInstance().playSFXGlobal(SFXType::MenuConfirm);
      m_gameState = GameState::CreatingRoom;
      m_gameModeOption = GameModeOption::EscapeMode; // 默认选中 Escape
    }

    // 退格键
    if (keyPressed->code == sf::Keyboard::Key::Backspace && !m_inputText.empty())
    {
      m_inputText.pop_back();
    }
  }

  // 文本输入
  if (const auto *textEntered = event.getIf<sf::Event::TextEntered>())
  {
    if (m_inputMode == InputMode::RoomCode)
    {
      // 房间号只允许输入数字，最多4位
      if (textEntered->unicode >= '0' && textEntered->unicode <= '9' && m_inputText.length() < 4)
      {
        m_inputText += static_cast<char>(textEntered->unicode);
      }
    }
    else if (textEntered->unicode >= 32 && textEntered->unicode < 127)
    {
      m_inputText += static_cast<char>(textEntered->unicode);
    }
  }
}

MultiplayerContext Game::getMultiplayerContext()
{
  return MultiplayerContext{
      m_window,
      m_gameView,
      m_uiView,
      m_font,
      m_player.get(),
      m_otherPlayer.get(),
      m_enemies,
      m_bullets,
      m_maze,
      LOGICAL_WIDTH,  // 使用逻辑分辨率
      LOGICAL_HEIGHT, // 使用逻辑分辨率
      m_tankScale,
      m_placementMode,
      m_mpState.isEscapeMode,
      m_mpState.isDarkMode};
}

void Game::updateMultiplayer(float dt)
{
  // R键状态已经在 processEvents 中通过事件驱动设置

  auto ctx = getMultiplayerContext();
  MultiplayerHandler::update(ctx, m_mpState, dt, [this]()
                             { m_gameState = GameState::Victory; }, [this]()
                             { m_gameState = GameState::GameOver; });
}

void Game::renderConnecting()
{
  MultiplayerHandler::renderConnecting(
      m_window, m_uiView, m_font,
      LOGICAL_WIDTH, LOGICAL_HEIGHT,
      m_mpState.connectionStatus, m_inputText,
      m_inputMode == InputMode::ServerIP);
}

void Game::renderWaitingForPlayer()
{
  MultiplayerHandler::renderWaitingForPlayer(
      m_window, m_uiView, m_font,
      LOGICAL_WIDTH, LOGICAL_HEIGHT,
      m_mpState.roomCode);
}

void Game::renderMultiplayer()
{
  auto ctx = getMultiplayerContext();
  MultiplayerHandler::renderMultiplayer(ctx, m_mpState);
}

void Game::processRoomLobbyEvents(const sf::Event &event)
{
  if (const auto *keyPressed = event.getIf<sf::Event::KeyPressed>())
  {
    if (keyPressed->code == sf::Keyboard::Key::Escape)
    {
      // 退出房间
      NetworkManager::getInstance().disconnect();
      resetGame();
      return;
    }

    if (keyPressed->code == sf::Keyboard::Key::R)
    {
      // 切换准备状态（非房主）
      if (!m_mpState.isHost)
      {
        m_mpState.localPlayerReady = !m_mpState.localPlayerReady;
        NetworkManager::getInstance().sendPlayerReady(m_mpState.localPlayerReady);
        AudioManager::getInstance().playSFXGlobal(SFXType::MenuSelect);
      }
    }

    if (keyPressed->code == sf::Keyboard::Key::Enter)
    {
      // 房主开始游戏
      if (m_mpState.isHost && m_mpState.otherPlayerInRoom && m_mpState.otherPlayerReady)
      {
        // 每局游戏开始前，房主生成新的随机地图
        m_mpState.isEscapeMode = (m_gameModeOption == GameModeOption::EscapeMode);
        int npcCount = m_mpState.npcCount;

        // 设置迷宫生成器模式
        m_mazeGenerator.setEscapeMode(m_mpState.isEscapeMode);

        // 生成新地图
        m_maze.generateRandomMaze(m_mpState.mazeWidth, m_mpState.mazeHeight, 0, npcCount, true, m_mpState.isEscapeMode);
        m_mpState.generatedMazeData = m_maze.getMazeData();

        // 发送新地图给服务器和对方玩家
        NetworkManager::getInstance().sendMazeData(m_mpState.generatedMazeData, m_mpState.isEscapeMode, m_mpState.isDarkMode);

        std::cout << "[DEBUG] Host generated new maze before game start: "
                  << m_mpState.mazeWidth << "x" << m_mpState.mazeHeight
                  << ", NPCs: " << npcCount
                  << ", Mode: " << (m_mpState.isEscapeMode ? "Escape" : "Battle") << std::endl;

        // 发送开始游戏信号
        NetworkManager::getInstance().sendHostStartGame();
        AudioManager::getInstance().playSFXGlobal(SFXType::MenuConfirm);
      }
    }
  }
}

void Game::renderRoomLobby()
{
  m_window.clear(sf::Color(30, 30, 30));
  m_window.setView(m_uiView);

  float centerX = LOGICAL_WIDTH / 2.f;
  float startY = 80.f;

  // 标题
  sf::Text title(m_font);
  title.setString("ROOM LOBBY");
  title.setCharacterSize(56);
  title.setFillColor(sf::Color::White);
  title.setStyle(sf::Text::Bold);
  sf::FloatRect titleBounds = title.getLocalBounds();
  title.setPosition({centerX - titleBounds.size.x / 2.f, startY});
  m_window.draw(title);

  // 房间信息框
  float boxY = startY + 100.f;
  float boxWidth = 700.f;
  float boxHeight = 500.f;
  float boxX = centerX - boxWidth / 2.f;

  sf::RectangleShape infoBox({boxWidth, boxHeight});
  infoBox.setPosition({boxX, boxY});
  infoBox.setFillColor(sf::Color(50, 50, 50, 200));
  infoBox.setOutlineColor(sf::Color::White);
  infoBox.setOutlineThickness(2.f);
  m_window.draw(infoBox);

  float textX = boxX + 30.f;
  float textY = boxY + 20.f;
  float lineHeight = 45.f;

  // 房间号
  sf::Text roomCodeText(m_font);
  roomCodeText.setString("Room Code: " + m_mpState.roomCode);
  roomCodeText.setCharacterSize(32);
  roomCodeText.setFillColor(sf::Color::Yellow);
  roomCodeText.setPosition({textX, textY});
  m_window.draw(roomCodeText);
  textY += lineHeight;

  // 分隔线
  sf::RectangleShape separator({boxWidth - 60.f, 2.f});
  separator.setPosition({textX, textY});
  separator.setFillColor(sf::Color(100, 100, 100));
  m_window.draw(separator);
  textY += 20.f;

  // 游戏模式
  sf::Text modeText(m_font);
  modeText.setString(std::string("Game Mode: ") + (m_mpState.isEscapeMode ? "Escape (Co-op)" : "Battle (PvP)"));
  modeText.setCharacterSize(28);
  modeText.setFillColor(m_mpState.isEscapeMode ? sf::Color::Green : sf::Color::Red);
  modeText.setPosition({textX, textY});
  m_window.draw(modeText);
  textY += lineHeight;

  // 暗黑模式
  sf::Text darkModeText(m_font);
  darkModeText.setString(std::string("Dark Mode: ") + (m_mpState.isDarkMode ? "ON" : "OFF"));
  darkModeText.setCharacterSize(28);
  darkModeText.setFillColor(m_mpState.isDarkMode ? sf::Color(200, 100, 255) : sf::Color(150, 150, 150));
  darkModeText.setPosition({textX, textY});
  m_window.draw(darkModeText);
  textY += lineHeight;

  // 迷宫尺寸
  sf::Text mazeText(m_font);
  mazeText.setString("Maze Size: " + std::to_string(m_mpState.mazeWidth) + " x " + std::to_string(m_mpState.mazeHeight));
  mazeText.setCharacterSize(28);
  mazeText.setFillColor(sf::Color::White);
  mazeText.setPosition({textX, textY});
  m_window.draw(mazeText);
  textY += lineHeight;

  // NPC数量
  sf::Text npcText(m_font);
  npcText.setString("NPCs: " + std::to_string(m_mpState.npcCount));
  npcText.setCharacterSize(28);
  npcText.setFillColor(sf::Color::White);
  npcText.setPosition({textX, textY});
  m_window.draw(npcText);
  textY += lineHeight + 10.f;

  // 分隔线
  separator.setPosition({textX, textY});
  m_window.draw(separator);
  textY += 20.f;

  // 玩家列表标题
  sf::Text playersTitle(m_font);
  playersTitle.setString("Players:");
  playersTitle.setCharacterSize(28);
  playersTitle.setFillColor(sf::Color::Cyan);
  playersTitle.setPosition({textX, textY});
  m_window.draw(playersTitle);
  textY += lineHeight;

  // 玩家1（房主或本地）
  sf::Text player1Text(m_font);
  std::string p1Status = m_mpState.isHost ? "[HOST] You" : ("[HOST] " + (m_mpState.otherPlayerIP.empty() ? "Unknown" : m_mpState.otherPlayerIP));
  if (m_mpState.isHost)
  {
    p1Status += " - " + m_mpState.localPlayerIP;
  }
  player1Text.setString(p1Status);
  player1Text.setCharacterSize(26);
  player1Text.setFillColor(m_mpState.isHost ? sf::Color::Yellow : sf::Color::White);
  player1Text.setPosition({textX + 20.f, textY});
  m_window.draw(player1Text);

  // 准备状态（房主默认准备）- 右对齐
  float readyRightEdge = boxX + boxWidth - 30.f; // 准备状态右边界
  sf::Text p1Ready(m_font);
  p1Ready.setString("READY");
  p1Ready.setCharacterSize(24);
  p1Ready.setFillColor(sf::Color::Green);
  sf::FloatRect p1ReadyBounds = p1Ready.getLocalBounds();
  p1Ready.setPosition({readyRightEdge - p1ReadyBounds.size.x, textY});
  m_window.draw(p1Ready);
  textY += lineHeight;

  // 玩家2
  sf::Text player2Text(m_font);
  if (m_mpState.otherPlayerInRoom)
  {
    std::string p2Status = m_mpState.isHost ? ("Player 2: " + m_mpState.otherPlayerIP) : ("[YOU] " + m_mpState.localPlayerIP);
    player2Text.setString(p2Status);
    player2Text.setFillColor(m_mpState.isHost ? sf::Color::White : sf::Color::Yellow);
  }
  else
  {
    player2Text.setString("Waiting for player to join...");
    player2Text.setFillColor(sf::Color(150, 150, 150));
  }
  player2Text.setCharacterSize(26);
  player2Text.setPosition({textX + 20.f, textY});
  m_window.draw(player2Text);

  // 玩家2准备状态 - 右对齐
  if (m_mpState.otherPlayerInRoom)
  {
    sf::Text p2Ready(m_font);
    bool isP2Ready = m_mpState.isHost ? m_mpState.otherPlayerReady : m_mpState.localPlayerReady;
    p2Ready.setString(isP2Ready ? "READY" : "NOT READY");
    p2Ready.setCharacterSize(24);
    p2Ready.setFillColor(isP2Ready ? sf::Color::Green : sf::Color::Red);
    sf::FloatRect p2ReadyBounds = p2Ready.getLocalBounds();
    p2Ready.setPosition({readyRightEdge - p2ReadyBounds.size.x, textY});
    m_window.draw(p2Ready);
  }
  textY += lineHeight + 30.f;

  // 操作提示
  sf::Text hintText(m_font);
  if (m_mpState.isHost)
  {
    if (m_mpState.otherPlayerInRoom && m_mpState.otherPlayerReady)
    {
      hintText.setString("Press ENTER to start game");
      hintText.setFillColor(sf::Color::Green);
    }
    else if (m_mpState.otherPlayerInRoom)
    {
      hintText.setString("Waiting for player to ready...");
      hintText.setFillColor(sf::Color::Yellow);
    }
    else
    {
      hintText.setString("Waiting for player to join...");
      hintText.setFillColor(sf::Color(150, 150, 150));
    }
  }
  else
  {
    if (m_mpState.localPlayerReady)
    {
      hintText.setString("Waiting for host to start... (Press R to cancel ready)");
      hintText.setFillColor(sf::Color::Yellow);
    }
    else
    {
      hintText.setString("Press R to ready up");
      hintText.setFillColor(sf::Color::Cyan);
    }
  }
  hintText.setCharacterSize(28);
  sf::FloatRect hintBounds = hintText.getLocalBounds();
  hintText.setPosition({centerX - hintBounds.size.x / 2.f, textY});
  m_window.draw(hintText);

  // ESC退出提示
  sf::Text escText(m_font);
  escText.setString("Press ESC to leave room");
  escText.setCharacterSize(22);
  escText.setFillColor(sf::Color(150, 150, 150));
  sf::FloatRect escBounds = escText.getLocalBounds();
  escText.setPosition({centerX - escBounds.size.x / 2.f, boxY + boxHeight + 20.f});
  m_window.draw(escText);

  m_window.display();
}

void Game::processCreatingRoomEvents(const sf::Event &event)
{
  if (const auto *keyPressed = event.getIf<sf::Event::KeyPressed>())
  {
    switch (keyPressed->code)
    {
    case sf::Keyboard::Key::Up:
    case sf::Keyboard::Key::W:
    case sf::Keyboard::Key::Down:
    case sf::Keyboard::Key::S:
    {
      // 在 Escape 和 Battle 之间切换
      int current = static_cast<int>(m_gameModeOption);
      int maxOption = 1; // 只有两个选项: EscapeMode(0) 和 BattleMode(1)
      if (keyPressed->code == sf::Keyboard::Key::Up || keyPressed->code == sf::Keyboard::Key::W)
        current = (current - 1 + maxOption + 1) % (maxOption + 1);
      else
        current = (current + 1) % (maxOption + 1);
      m_gameModeOption = static_cast<GameModeOption>(current);
      AudioManager::getInstance().playSFXGlobal(SFXType::MenuSelect);
      break;
    }
    case sf::Keyboard::Key::D:
      // 切换暗黑模式
      m_darkModeOption = !m_darkModeOption;
      AudioManager::getInstance().playSFXGlobal(SFXType::MenuSelect);
      break;
    case sf::Keyboard::Key::Escape:
      // 返回连接界面
      AudioManager::getInstance().playSFXGlobal(SFXType::MenuSelect);
      m_gameState = GameState::Connecting;
      break;
    case sf::Keyboard::Key::Enter:
    case sf::Keyboard::Key::Space:
      // 确认选择并创建房间
      AudioManager::getInstance().playSFXGlobal(SFXType::MenuConfirm);
      m_mpState.isEscapeMode = (m_gameModeOption == GameModeOption::EscapeMode);
      m_mpState.isDarkMode = m_darkModeOption;
      NetworkManager::getInstance().createRoom(m_mazeWidth, m_mazeHeight, m_darkModeOption);
      break;
    default:
      break;
    }
  }
}

void Game::renderCreatingRoom()
{
  m_window.clear(sf::Color(30, 30, 40));
  m_window.setView(m_uiView);

  float centerX = LOGICAL_WIDTH / 2.f;

  // 标题
  sf::Text title(m_font);
  title.setString("MULTIPLAYER");
  title.setCharacterSize(56);
  title.setFillColor(sf::Color::White);
  title.setStyle(sf::Text::Bold);
  sf::FloatRect titleBounds = title.getLocalBounds();
  title.setPosition({centerX - titleBounds.size.x / 2.f, 100.f});
  m_window.draw(title);

  // 副标题
  sf::Text subtitle(m_font);
  subtitle.setString("Select Game Mode");
  subtitle.setCharacterSize(28);
  subtitle.setFillColor(sf::Color(180, 180, 180));
  sf::FloatRect subtitleBounds = subtitle.getLocalBounds();
  subtitle.setPosition({centerX - subtitleBounds.size.x / 2.f, 170.f});
  m_window.draw(subtitle);

  float startY = 280.f;
  float spacing = 80.f;

  // Escape Mode
  {
    sf::Text optionText(m_font);
    std::string modeStr = "Escape Mode";
    optionText.setCharacterSize(36);

    if (m_gameModeOption == GameModeOption::EscapeMode)
    {
      optionText.setFillColor(sf::Color::Yellow);
      optionText.setString("> " + modeStr + " <");
    }
    else
    {
      optionText.setFillColor(sf::Color(180, 180, 180));
      optionText.setString(modeStr);
    }

    sf::FloatRect bounds = optionText.getLocalBounds();
    optionText.setPosition({centerX - bounds.size.x / 2.f, startY});
    m_window.draw(optionText);

    // 模式描述
    sf::Text desc(m_font);
    desc.setString("Cooperate with your teammate to escape!");
    desc.setCharacterSize(20);
    desc.setFillColor(sf::Color(100, 180, 100));
    sf::FloatRect descBounds = desc.getLocalBounds();
    desc.setPosition({centerX - descBounds.size.x / 2.f, startY + 40.f});
    m_window.draw(desc);
  }

  // Battle Mode
  {
    sf::Text optionText(m_font);
    std::string modeStr = "Battle Mode";
    optionText.setCharacterSize(36);

    if (m_gameModeOption == GameModeOption::BattleMode)
    {
      optionText.setFillColor(sf::Color::Yellow);
      optionText.setString("> " + modeStr + " <");
    }
    else
    {
      optionText.setFillColor(sf::Color(180, 180, 180));
      optionText.setString(modeStr);
    }

    sf::FloatRect bounds = optionText.getLocalBounds();
    optionText.setPosition({centerX - bounds.size.x / 2.f, startY + spacing});
    m_window.draw(optionText);

    // 模式描述
    sf::Text desc(m_font);
    desc.setString("Defeat your opponent or reach the exit first!");
    desc.setCharacterSize(20);
    desc.setFillColor(sf::Color(180, 100, 100));
    sf::FloatRect descBounds = desc.getLocalBounds();
    desc.setPosition({centerX - descBounds.size.x / 2.f, startY + spacing + 40.f});
    m_window.draw(desc);
  }

  // 暗黑模式选项
  {
    sf::Text darkModeText(m_font);
    std::string darkStr = "Dark Mode: " + std::string(m_darkModeOption ? "ON" : "OFF");
    darkModeText.setCharacterSize(28);
    darkModeText.setFillColor(m_darkModeOption ? sf::Color(200, 100, 255) : sf::Color(150, 150, 150));
    darkModeText.setString(darkStr);
    sf::FloatRect bounds = darkModeText.getLocalBounds();
    darkModeText.setPosition({centerX - bounds.size.x / 2.f, startY + spacing * 2 + 20.f});
    m_window.draw(darkModeText);

    // 暗黑模式描述
    sf::Text desc(m_font);
    desc.setString("Limited vision with fog of war (D to toggle)");
    desc.setCharacterSize(18);
    desc.setFillColor(sf::Color(120, 120, 120));
    sf::FloatRect descBounds = desc.getLocalBounds();
    desc.setPosition({centerX - descBounds.size.x / 2.f, startY + spacing * 2 + 55.f});
    m_window.draw(desc);
  }

  // 地图信息
  sf::Text mapInfo(m_font);
  mapInfo.setString("Map: " + std::to_string(m_mazeWidth) + " x " + std::to_string(m_mazeHeight) +
                    "  |  NPCs: " + std::to_string(m_enemyOptions[m_enemyIndex]));
  mapInfo.setCharacterSize(24);
  mapInfo.setFillColor(sf::Color(100, 200, 100));
  sf::FloatRect mapBounds = mapInfo.getLocalBounds();
  mapInfo.setPosition({centerX - mapBounds.size.x / 2.f, 560.f});
  m_window.draw(mapInfo);

  // 提示
  sf::Text hint(m_font);
  hint.setString("W/S: Game Mode | D: Dark Mode | Enter: Create Room | ESC: Back");
  hint.setCharacterSize(18);
  hint.setFillColor(sf::Color(120, 120, 120));
  sf::FloatRect hintBounds = hint.getLocalBounds();
  hint.setPosition({centerX - hintBounds.size.x / 2.f, LOGICAL_HEIGHT - 60.f});
  m_window.draw(hint);

  m_window.display();
}

void Game::handleWindowResize()
{
  sf::Vector2u windowSize = m_window.getSize();
  float windowRatio = static_cast<float>(windowSize.x) / windowSize.y;

  sf::FloatRect viewport;

  if (windowRatio > ASPECT_RATIO)
  {
    // 窗口太宽，左右留黑边
    float viewportWidth = ASPECT_RATIO / windowRatio;
    viewport = sf::FloatRect({(1.f - viewportWidth) / 2.f, 0.f}, {viewportWidth, 1.f});
  }
  else
  {
    // 窗口太高，上下留黑边
    float viewportHeight = windowRatio / ASPECT_RATIO;
    viewport = sf::FloatRect({0.f, (1.f - viewportHeight) / 2.f}, {1.f, viewportHeight});
  }

  m_gameView.setViewport(viewport);
  m_uiView.setViewport(viewport);
}

bool Game::isExitInView() const
{
  if (!m_player)
    return false;

  sf::Vector2f exitPos = m_maze.getExitPosition();
  sf::Vector2f viewCenter = m_gameView.getCenter();
  sf::Vector2f viewSize = m_gameView.getSize();

  // 检查终点是否在当前视野范围内
  float halfWidth = viewSize.x / 2.f;
  float halfHeight = viewSize.y / 2.f;

  return (exitPos.x >= viewCenter.x - halfWidth && exitPos.x <= viewCenter.x + halfWidth &&
          exitPos.y >= viewCenter.y - halfHeight && exitPos.y <= viewCenter.y + halfHeight);
}

void Game::renderDarkModeOverlay()
{
  if (!m_player)
    return;

  // 保存当前视图
  sf::View currentView = m_window.getView();

  // 切换到游戏视图来绘制遮罩
  m_window.setView(m_gameView);

  // 获取玩家位置和视图尺寸
  sf::Vector2f playerPos = m_player->getPosition();
  sf::Vector2f viewSize = m_gameView.getSize();

  // 使用类成员纹理（非静态，确保在窗口销毁前释放）
  // 纹理尺寸为视图的2倍，以覆盖更大区域
  unsigned int texWidth = static_cast<unsigned int>(viewSize.x * 2);
  unsigned int texHeight = static_cast<unsigned int>(viewSize.y * 2);

  if (!m_darkModeTexture || m_darkModeTexWidth != texWidth || m_darkModeTexHeight != texHeight)
  {
    // 创建图像
    sf::Image image({texWidth, texHeight}, sf::Color::Transparent);

    // 椭圆参数（基于原始视图尺寸，不是纹理尺寸）
    float ellipseB = viewSize.y * 0.28f;
    float ellipseA = viewSize.x * 0.22f;
    float fadeScale = 0.3f;
    float fadeA = ellipseA * fadeScale;
    float fadeB = ellipseB * fadeScale;

    float centerX = texWidth / 2.f;
    float centerY = texHeight / 2.f;

    for (unsigned int y = 0; y < texHeight; y++)
    {
      for (unsigned int x = 0; x < texWidth; x++)
      {
        float dx = x - centerX;
        float dy = y - centerY;

        float ellipseDist = std::sqrt((dx * dx) / (ellipseA * ellipseA) + (dy * dy) / (ellipseB * ellipseB));

        uint8_t alpha;
        if (ellipseDist <= 1.0f)
        {
          alpha = 0;
        }
        else
        {
          float outerA = ellipseA + fadeA;
          float outerB = ellipseB + fadeB;
          float outerDist = std::sqrt((dx * dx) / (outerA * outerA) + (dy * dy) / (outerB * outerB));

          if (outerDist >= 1.0f)
          {
            alpha = 255;
          }
          else
          {
            float fadeProgress = (ellipseDist - 1.0f) / ((outerA / ellipseA) - 1.0f);
            fadeProgress = std::min(1.0f, std::max(0.0f, fadeProgress));
            alpha = static_cast<uint8_t>(255 * fadeProgress);
          }
        }

        image.setPixel({x, y}, sf::Color(0, 0, 0, alpha));
      }
    }

    m_darkModeTexture = std::make_unique<sf::Texture>(image);
    m_darkModeSprite = std::make_unique<sf::Sprite>(*m_darkModeTexture);
    m_darkModeTexWidth = texWidth;
    m_darkModeTexHeight = texHeight;
  }

  // 绘制遮罩（纹理是2倍大小，所以偏移也要相应调整）
  if (m_darkModeSprite)
  {
    m_darkModeSprite->setPosition({playerPos.x - viewSize.x, playerPos.y - viewSize.y});
    m_window.draw(*m_darkModeSprite);
  }

  // 恢复之前的视图
  m_window.setView(currentView);
}

void Game::renderMinimap()
{
  // 保存当前视图
  sf::View currentView = m_window.getView();

  // 切换到UI视图绘制小地图
  m_window.setView(m_uiView);

  // 小地图参数
  const float minimapSize = 150.f;
  const float minimapMargin = 20.f;
  const float minimapX = minimapMargin;
  const float minimapY = static_cast<float>(LOGICAL_HEIGHT) - minimapSize - minimapMargin - 35.f;

  // 绘制小地图背景
  sf::RectangleShape minimapBg({minimapSize, minimapSize});
  minimapBg.setPosition({minimapX, minimapY});
  minimapBg.setFillColor(sf::Color(20, 20, 20, 200));
  minimapBg.setOutlineColor(sf::Color(100, 100, 100, 255));
  minimapBg.setOutlineThickness(2.f);
  m_window.draw(minimapBg);

  // 计算地图范围（基于迷宫大小）
  sf::Vector2f mazeSize = m_maze.getSize();
  float mapWidth = mazeSize.x;
  float mapHeight = mazeSize.y;
  float scale = std::min(minimapSize / mapWidth, minimapSize / mapHeight) * 0.9f;

  // 计算小地图中心偏移（使内容居中）
  float offsetX = minimapX + (minimapSize - mapWidth * scale) / 2.f;
  float offsetY = minimapY + (minimapSize - mapHeight * scale) / 2.f;

  // 将世界坐标转换为小地图坐标的lambda
  auto worldToMinimap = [&](sf::Vector2f worldPos) -> sf::Vector2f
  {
    return sf::Vector2f(
        offsetX + worldPos.x * scale,
        offsetY + worldPos.y * scale);
  };

  // 绘制NPC
  for (const auto &enemy : m_enemies)
  {
    if (enemy->isDead())
      continue;

    sf::Vector2f npcPos = enemy->getPosition();
    sf::Vector2f npcMiniPos = worldToMinimap(npcPos);

    sf::CircleShape npcDot(3.f);
    npcDot.setPosition({npcMiniPos.x - 3.f, npcMiniPos.y - 3.f});

    // 根据激活状态显示不同颜色
    if (enemy->isActivated())
    {
      npcDot.setFillColor(GameColors::MinimapEnemyNpc); // 已激活：红色
    }
    else
    {
      npcDot.setFillColor(GameColors::MinimapInactiveNpc); // 未激活：灰色
    }
    m_window.draw(npcDot);
  }

  // 绘制玩家（黄色，最后绘制以确保在最上层）
  if (m_player)
  {
    sf::Vector2f playerPos = m_player->getPosition();
    sf::Vector2f playerMiniPos = worldToMinimap(playerPos);

    sf::CircleShape playerDot(4.f);
    playerDot.setPosition({playerMiniPos.x - 4.f, playerMiniPos.y - 4.f});
    playerDot.setFillColor(GameColors::MinimapPlayer);
    m_window.draw(playerDot);
  }

  // 小地图标签
  sf::Text minimapLabel(m_font);
  minimapLabel.setString("Minimap");
  minimapLabel.setCharacterSize(12);
  minimapLabel.setFillColor(sf::Color(180, 180, 180));
  minimapLabel.setPosition({minimapX + 5.f, minimapY + 3.f});
  m_window.draw(minimapLabel);

  // 恢复之前的视图
  m_window.setView(currentView);
}
