#include "MultiplayerHandler.hpp"
#include "UIHelper.hpp"
#include "CollisionSystem.hpp"
#include "Utils.hpp"
#include "AudioManager.hpp"
#include <cmath>
#include <iostream>
#include <limits>

// 静态成员定义
std::unique_ptr<sf::Texture> MultiplayerHandler::s_darkModeTexture;
std::unique_ptr<sf::Sprite> MultiplayerHandler::s_darkModeSprite;
bool MultiplayerHandler::s_darkModeTextureInitialized = false;
unsigned int MultiplayerHandler::s_lastTextureWidth = 0;
unsigned int MultiplayerHandler::s_lastTextureHeight = 0;

void MultiplayerHandler::initDarkModeTexture(unsigned int width, unsigned int height)
{
  // width/height 是原始视图尺寸，纹理实际为2倍大小
  unsigned int texWidth = width * 2;
  unsigned int texHeight = height * 2;

  // 如果尺寸没变且已初始化，直接返回
  if (s_darkModeTextureInitialized &&
      s_lastTextureWidth == texWidth &&
      s_lastTextureHeight == texHeight)
  {
    return;
  }

  // 创建图像（2倍尺寸）
  sf::Image image({texWidth, texHeight}, sf::Color::Transparent);

  // 椭圆参数：基于原始视图尺寸（不是纹理尺寸）
  float ellipseB = height * 0.28f; // 椭圆短半轴（垂直方向）
  float ellipseA = width * 0.22f;  // 椭圆长半轴（水平方向）

  // 渐变区域
  float fadeScale = 0.3f;
  float fadeA = ellipseA * fadeScale;
  float fadeB = ellipseB * fadeScale;

  float centerX = texWidth / 2.f;
  float centerY = texHeight / 2.f;

  // 遍历每个像素
  for (unsigned int y = 0; y < texHeight; y++)
  {
    for (unsigned int x = 0; x < texWidth; x++)
    {
      float dx = x - centerX;
      float dy = y - centerY;

      // 计算归一化椭圆距离
      float ellipseDist = std::sqrt((dx * dx) / (ellipseA * ellipseA) + (dy * dy) / (ellipseB * ellipseB));

      uint8_t alpha;
      if (ellipseDist <= 1.0f)
      {
        // 在椭圆内部，完全透明
        alpha = 0;
      }
      else
      {
        // 计算外圈椭圆的归一化距离
        float outerA = ellipseA + fadeA;
        float outerB = ellipseB + fadeB;
        float outerDist = std::sqrt((dx * dx) / (outerA * outerA) + (dy * dy) / (outerB * outerB));

        if (outerDist >= 1.0f)
        {
          // 在渐变区域外，完全黑色
          alpha = 255;
        }
        else
        {
          // 在渐变区域内，计算渐变
          float fadeProgress = (ellipseDist - 1.0f) / ((outerA / ellipseA) - 1.0f);
          fadeProgress = std::min(1.0f, std::max(0.0f, fadeProgress));
          alpha = static_cast<uint8_t>(255 * fadeProgress);
        }
      }

      image.setPixel({x, y}, sf::Color(0, 0, 0, alpha));
    }
  }

  // 创建纹理
  s_darkModeTexture = std::make_unique<sf::Texture>(image);
  if (!s_darkModeTexture)
  {
    std::cerr << "Failed to create dark mode texture!" << std::endl;
    return;
  }

  // 使用纹理创建 Sprite
  s_darkModeSprite = std::make_unique<sf::Sprite>(*s_darkModeTexture);
  s_darkModeTextureInitialized = true;
  s_lastTextureWidth = texWidth;
  s_lastTextureHeight = texHeight;

  std::cout << "[DEBUG] Dark mode texture initialized: " << texWidth << "x" << texHeight << std::endl;
}

void MultiplayerHandler::cleanup()
{
  // 释放静态资源（在窗口关闭前调用）
  s_darkModeSprite.reset();
  s_darkModeTexture.reset();
  s_darkModeTextureInitialized = false;
  s_lastTextureWidth = 0;
  s_lastTextureHeight = 0;
}

void MultiplayerHandler::update(
    MultiplayerContext &ctx,
    MultiplayerState &state,
    float dt,
    std::function<void()> onVictory,
    std::function<void()> onDefeat)
{
  if (!ctx.player)
    return;

  auto &net = NetworkManager::getInstance();

  // 检查本地玩家死亡状态
  if (ctx.player->isDead() && !state.localPlayerDead)
  {
    state.localPlayerDead = true;
    // 在 Escape 模式下，死亡不直接结束游戏，等待队友救援
    if (!state.isEscapeMode)
    {
      // Battle 模式：直接失败
      state.multiplayerWin = false;
      std::cout << "[DEBUG] Battle mode: Local player died, sending lose" << std::endl;
      net.sendGameResult(false);
      onDefeat();
      return;
    }
    else
    {
      std::cout << "[DEBUG] Escape mode: Local player downed, waiting for rescue" << std::endl;
    }
  }

  // Escape 模式：检查是否两个玩家都死亡（只由房主判定，避免竞争条件）
  if (state.isEscapeMode && state.isHost && state.localPlayerDead && state.otherPlayerDead)
  {
    state.multiplayerWin = false;
    std::cout << "[DEBUG] Escape mode: Both dead, sending lose" << std::endl;
    net.sendGameResult(false); // 通知对方：游戏失败
    onDefeat();
    return;
  }

  // 获取鼠标在世界坐标中的位置
  sf::Vector2i mousePixelPos = sf::Mouse::getPosition(ctx.window);
  sf::Vector2f mouseWorldPos = ctx.window.mapPixelToCoords(mousePixelPos, ctx.gameView);

  // 如果本地玩家没死，正常更新
  if (!state.localPlayerDead)
  {
    // 保存旧位置
    sf::Vector2f oldPos = ctx.player->getPosition();
    sf::Vector2f movement = ctx.player->getMovement(dt);

    // 更新本地玩家
    ctx.player->update(dt, mouseWorldPos);

    // 碰撞检测
    sf::Vector2f newPos = ctx.player->getPosition();
    float radius = ctx.player->getCollisionRadius();

    if (ctx.maze.checkCollision(newPos, radius))
    {
      sf::Vector2f posX = {oldPos.x + movement.x, oldPos.y};
      sf::Vector2f posY = {oldPos.x, oldPos.y + movement.y};

      bool canMoveX = !ctx.maze.checkCollision(posX, radius);
      bool canMoveY = !ctx.maze.checkCollision(posY, radius);

      if (canMoveX && canMoveY)
      {
        if (std::abs(movement.x) > std::abs(movement.y))
          ctx.player->setPosition(posX);
        else
          ctx.player->setPosition(posY);
      }
      else if (canMoveX)
      {
        ctx.player->setPosition(posX);
      }
      else if (canMoveY)
      {
        ctx.player->setPosition(posY);
      }
      else
      {
        ctx.player->setPosition(oldPos);
      }
    }

    // 处理射击（只有活着的玩家可以射击）
    if (ctx.player->hasFiredBullet())
    {
      sf::Vector2f bulletPos = ctx.player->getBulletSpawnPosition();
      float bulletAngle = ctx.player->getTurretRotation();
      auto bullet = std::make_unique<Bullet>(bulletPos.x, bulletPos.y, bulletAngle, true);
      bullet->setTeam(ctx.player->getTeam()); // 设置子弹阵营
      ctx.bullets.push_back(std::move(bullet));
      net.sendShoot(bulletPos.x, bulletPos.y, bulletAngle);

      // 播放射击音效
      AudioManager::getInstance().playSFX(SFXType::Shoot, bulletPos, ctx.player->getPosition());
    }
  }

  // Escape 模式：处理救援逻辑
  if (state.isEscapeMode && !state.localPlayerDead && state.otherPlayerDead && ctx.otherPlayer)
  {
    // 检查是否靠近倒地的队友
    sf::Vector2f myPos = ctx.player->getPosition();
    sf::Vector2f otherPos = ctx.otherPlayer->getPosition();
    float distToTeammate = std::hypot(myPos.x - otherPos.x, myPos.y - otherPos.y);

    const float RESCUE_DISTANCE = 60.f;
    const float RESCUE_TIME = 3.0f; // 3秒救援时间

    if (distToTeammate < RESCUE_DISTANCE)
    {
      state.canRescue = true;

      if (state.fKeyHeld)
      {
        // 正在施救
        if (!state.isRescuing)
        {
          state.isRescuing = true;
          state.rescueProgress = 0.f;
          state.rescueSyncTimer = 0.f;
          net.sendRescueStart();
          std::cout << "[DEBUG] Started rescue!" << std::endl;
        }

        state.rescueProgress += dt;

        // 每0.1秒同步进度
        state.rescueSyncTimer += dt;
        if (state.rescueSyncTimer >= 0.1f)
        {
          net.sendRescueProgress(state.rescueProgress / RESCUE_TIME);
          state.rescueSyncTimer = 0.f;
        }

        // 救援完成
        if (state.rescueProgress >= RESCUE_TIME)
        {
          std::cout << "[DEBUG] Rescue complete! Reviving teammate." << std::endl;
          state.isRescuing = false;
          state.rescueProgress = 0.f;
          state.otherPlayerDead = false;
          net.sendRescueComplete();

          // 复活队友：恢复50%血量
          if (ctx.otherPlayer)
          {
            ctx.otherPlayer->setHealth(50.f);
            std::cout << "[DEBUG] Set otherPlayer health to 50" << std::endl;
          }
        }
      }
      else
      {
        // F键松开，取消救援
        if (state.isRescuing)
        {
          state.isRescuing = false;
          state.rescueProgress = 0.f;
          net.sendRescueCancel();
          std::cout << "[DEBUG] Rescue cancelled (F key released)" << std::endl;
        }
      }
    }
    else
    {
      state.canRescue = false;
      // 离开救援范围，取消救援
      if (state.isRescuing)
      {
        state.isRescuing = false;
        state.rescueProgress = 0.f;
        net.sendRescueCancel();
      }
    }
  }
  else
  {
    state.canRescue = false;
    if (state.isRescuing)
    {
      state.isRescuing = false;
      state.rescueProgress = 0.f;
      net.sendRescueCancel();
    }
  }

  // 检查玩家是否接近未激活的NPC（Battle 模式才需要手动激活）
  if (!state.isEscapeMode)
  {
    checkNearbyNpc(ctx, state);
    handleNpcActivation(ctx, state);
  }

  // Escape 模式：NPC 自动激活（类似单人模式）
  // 双方都可以触发激活，只要本地玩家接近 NPC 就发送激活消息
  if (state.isEscapeMode && !state.localPlayerDead && ctx.player)
  {
    for (auto &npc : ctx.enemies)
    {
      if (!npc->isActivated() && !npc->isDead())
      {
        // 只检查本地玩家的距离
        sf::Vector2f toPlayer = ctx.player->getPosition() - npc->getPosition();
        float dist = std::sqrt(toPlayer.x * toPlayer.x + toPlayer.y * toPlayer.y);

        if (dist < 600.f)
        {
          // 本地玩家触发激活
          int activatorId = state.isHost ? 0 : 1; // 房主=0, 非房主=1

          // 自动激活，team=0 表示攻击玩家
          npc->activate(0, activatorId);
          net.sendNpcActivate(npc->getId(), 0, activatorId);
          std::cout << "[DEBUG] Escape mode: Local player activated NPC " << npc->getId() << std::endl;
        }
      }
    }
  }

  // 更新NPC AI（仅房主执行）
  // 非房主的NPC位置通过网络回调直接设置，不需要本地更新
  if (state.isHost)
  {
    updateNpcAI(ctx, state, dt);
  }

  // 发送位置到服务器
  PlayerState pstate;
  pstate.x = ctx.player->getPosition().x;
  pstate.y = ctx.player->getPosition().y;
  pstate.rotation = ctx.player->getRotation();
  pstate.turretAngle = ctx.player->getTurretRotation();
  pstate.health = ctx.player->getHealth();
  pstate.reachedExit = state.localPlayerReachedExit;
  pstate.isDead = state.localPlayerDead;
  net.sendPosition(pstate);

  // 更新迷宫
  ctx.maze.update(dt);

  // 更新子弹
  for (auto &bullet : ctx.bullets)
  {
    bullet->update(dt);
  }

  // 子弹碰撞检测
  CollisionSystem::checkMultiplayerCollisions(
      ctx.player, ctx.otherPlayer, ctx.enemies, ctx.bullets, ctx.maze, state.isHost);

  // 删除超出范围的子弹
  ctx.bullets.erase(
      std::remove_if(ctx.bullets.begin(), ctx.bullets.end(),
                     [](const std::unique_ptr<Bullet> &b)
                     { return !b->isAlive(); }),
      ctx.bullets.end());

  // 检查玩家是否到达终点（只有活着的玩家才能到达终点，需要按住E键3秒）
  const float EXIT_HOLD_TIME = 3.0f;
  sf::Vector2f exitPos = ctx.maze.getExitPosition();
  if (!state.localPlayerDead)
  {
    float distToExit = std::hypot(ctx.player->getPosition().x - exitPos.x,
                                  ctx.player->getPosition().y - exitPos.y);

    if (distToExit < TILE_SIZE)
    {
      state.isAtExitZone = true;

      if (state.eKeyHeld)
      {
        // 正在按住E键
        if (!state.isHoldingExit)
        {
          state.isHoldingExit = true;
          state.exitHoldProgress = 0.f;
          std::cout << "[DEBUG] Started holding E at exit" << std::endl;
        }

        state.exitHoldProgress += dt;

        // 完成确认
        if (state.exitHoldProgress >= EXIT_HOLD_TIME && !state.localPlayerReachedExit)
        {
          state.localPlayerReachedExit = true;
          state.isHoldingExit = false;
          state.exitHoldProgress = 0.f;
          std::cout << "[DEBUG] Exit confirmed! localPlayerReachedExit = true" << std::endl;

          if (state.isEscapeMode)
          {
            // Escape 模式：两个人都到达才算胜利（只由房主判定）
            if (state.isHost && state.otherPlayerReachedExit && !state.otherPlayerDead)
            {
              state.multiplayerWin = true;
              std::cout << "[DEBUG] Escape mode: Both reached exit, sending win" << std::endl;
              net.sendGameResult(true);
              onVictory();
              return;
            }
            // 非房主或对方还没到达：只标记自己到达了，等待
          }
          else
          {
            // Battle 模式：先到达者胜利
            state.multiplayerWin = true;
            std::cout << "[DEBUG] Battle mode: Reached exit first, sending win" << std::endl;
            net.sendGameResult(true);
            onVictory();
            return;
          }
        }
      }
      else
      {
        // 松开E键，取消进度
        if (state.isHoldingExit)
        {
          state.isHoldingExit = false;
          state.exitHoldProgress = 0.f;
          std::cout << "[DEBUG] Exit hold cancelled (E key released)" << std::endl;
        }
      }
    }
    else
    {
      // 离开终点区域，重置状态
      if (state.isAtExitZone)
      {
        state.isAtExitZone = false;
        state.isHoldingExit = false;
        state.exitHoldProgress = 0.f;
      }
    }
  }

  // Escape 模式：检查是否双方都到达终点（只由房主判定）
  if (state.isEscapeMode && state.isHost &&
      state.localPlayerReachedExit && state.otherPlayerReachedExit &&
      !state.localPlayerDead && !state.otherPlayerDead)
  {
    state.multiplayerWin = true;
    std::cout << "[DEBUG] Escape mode (second check): Both reached exit, sending win" << std::endl;
    net.sendGameResult(true);
    onVictory();
    return;
  }

  // Battle 模式：检查本地玩家是否死亡（这个检查可能是重复的，但为了安全保留）
  if (!state.isEscapeMode && ctx.player->isDead() && !state.localPlayerDead)
  {
    state.localPlayerDead = true;
    state.multiplayerWin = false;
    std::cout << "[DEBUG] Battle mode (second check): Local player died, sending lose" << std::endl;
    net.sendGameResult(false);
    onDefeat();
    return;
  }

  // 更新相机
  ctx.gameView.setCenter(ctx.player->getPosition());
}

void MultiplayerHandler::checkNearbyNpc(
    MultiplayerContext &ctx,
    MultiplayerState &state)
{
  state.nearbyNpcIndex = -1;
  sf::Vector2f playerPos = ctx.player->getPosition();

  for (size_t i = 0; i < ctx.enemies.size(); ++i)
  {
    auto &npc = ctx.enemies[i];
    if (!npc->isActivated())
    {
      sf::Vector2f npcPos = npc->getPosition();
      float dx = playerPos.x - npcPos.x;
      float dy = playerPos.y - npcPos.y;
      float dist = std::sqrt(dx * dx + dy * dy);

      if (dist < 80.f)
      {
        state.nearbyNpcIndex = static_cast<int>(i);
        break;
      }
    }
  }
}

void MultiplayerHandler::handleNpcActivation(
    MultiplayerContext &ctx,
    MultiplayerState &state)
{
  if (state.nearbyNpcIndex >= 0 && state.rKeyJustPressed)
  {
    int localTeam = ctx.player->getTeam();
    std::cout << "[DEBUG] R key triggered! NPC index: " << state.nearbyNpcIndex << std::endl;
    if (ctx.player->getCoins() >= 3)
    {
      ctx.player->spendCoins(3);
      // Battle 模式：激活者为本地玩家 (activatorId=0)
      ctx.enemies[state.nearbyNpcIndex]->activate(localTeam, 0);
      NetworkManager::getInstance().sendNpcActivate(state.nearbyNpcIndex, localTeam, 0);
      std::cout << "[DEBUG] Activated NPC " << state.nearbyNpcIndex << ", coins left: " << ctx.player->getCoins() << std::endl;
      state.nearbyNpcIndex = -1;
    }
    else
    {
      std::cout << "[DEBUG] Not enough coins to activate NPC" << std::endl;
    }
  }
  state.rKeyJustPressed = false;
}

void MultiplayerHandler::updateNpcAI(
    MultiplayerContext &ctx,
    MultiplayerState &state,
    float dt)
{
  auto &net = NetworkManager::getInstance();

  for (size_t i = 0; i < ctx.enemies.size(); ++i)
  {
    auto &npc = ctx.enemies[i];
    if (npc->isDead())
      continue;

    if (npc->isActivated())
    {
      int npcTeam = npc->getTeam();

      // 只有房主执行NPC AI逻辑
      if (state.isHost)
      {
        // 收集敌对目标
        std::vector<sf::Vector2f> targets;

        // Escape 模式：NPC (team=0) 攻击距离最近的活着的玩家
        if (state.isEscapeMode && npcTeam == 0)
        {
          sf::Vector2f npcPos = npc->getPosition();
          float closestDist = std::numeric_limits<float>::max();
          Tank *closestTarget = nullptr;

          // 检查本地玩家
          if (ctx.player && !state.localPlayerDead)
          {
            sf::Vector2f diff = ctx.player->getPosition() - npcPos;
            float dist = std::sqrt(diff.x * diff.x + diff.y * diff.y);
            if (dist < closestDist)
            {
              closestDist = dist;
              closestTarget = ctx.player;
            }
          }

          // 检查其他玩家
          if (ctx.otherPlayer && !state.otherPlayerDead)
          {
            sf::Vector2f diff = ctx.otherPlayer->getPosition() - npcPos;
            float dist = std::sqrt(diff.x * diff.x + diff.y * diff.y);
            if (dist < closestDist)
            {
              closestDist = dist;
              closestTarget = ctx.otherPlayer;
            }
          }

          // 设置最近的活着的玩家为攻击目标
          if (closestTarget)
          {
            targets.push_back(closestTarget->getPosition());
          }
        }
        else
        {
          // Battle 模式或其他情况：原有逻辑
          if (ctx.player && ctx.player->getTeam() != npcTeam && npcTeam != 0)
          {
            targets.push_back(ctx.player->getPosition());
          }

          if (ctx.otherPlayer && ctx.otherPlayer->getTeam() != npcTeam && npcTeam != 0)
          {
            targets.push_back(ctx.otherPlayer->getPosition());
          }

          for (const auto &otherNpc : ctx.enemies)
          {
            if (otherNpc.get() != npc.get() &&
                otherNpc->isActivated() &&
                !otherNpc->isDead() &&
                otherNpc->getTeam() != npcTeam &&
                otherNpc->getTeam() != 0)
            {
              targets.push_back(otherNpc->getPosition());
            }
          }
        }

        if (!targets.empty())
        {
          npc->setTargets(targets);
        }

        npc->update(dt, ctx.maze);

        // NPC射击
        if (npc->shouldShoot())
        {
          sf::Vector2f bulletPos = npc->getGunPosition();
          float bulletAngle = npc->getTurretAngle();
          // NPC子弹颜色：Escape模式全红（敌方），Battle模式根据team判断
          // 己方NPC（team与本地玩家相同）浅蓝色，敌方NPC红色
          sf::Color bulletColor;
          if (state.isEscapeMode)
          {
            bulletColor = GameColors::EnemyNpcBullet; // Escape模式所有NPC都是敌方
          }
          else
          {
            int localTeam = ctx.player ? ctx.player->getTeam() : 1;
            bulletColor = (npcTeam == localTeam) ? GameColors::AllyNpcBullet : GameColors::EnemyNpcBullet;
          }
          auto bullet = std::make_unique<Bullet>(bulletPos.x, bulletPos.y, bulletAngle, false, bulletColor);
          bullet->setTeam(npcTeam);
          bullet->setDamage(12.5f); // NPC子弹伤害12.5%
          ctx.bullets.push_back(std::move(bullet));
          net.sendNpcShoot(static_cast<int>(i), bulletPos.x, bulletPos.y, bulletAngle);

          // 播放NPC射击音效（基于本地玩家位置的距离衰减）
          AudioManager::getInstance().playSFX(SFXType::Shoot, bulletPos, ctx.player->getPosition());
        }

        // 每帧同步NPC状态
        NpcState npcState;
        npcState.id = static_cast<int>(i);
        npcState.x = npc->getPosition().x;
        npcState.y = npc->getPosition().y;
        npcState.rotation = npc->getRotation();
        npcState.turretAngle = npc->getTurretAngle();
        npcState.health = npc->getHealth();
        npcState.team = npc->getTeam();
        npcState.activated = npc->isActivated();
        net.sendNpcUpdate(npcState);
      }
    }
  }
}

void MultiplayerHandler::renderConnecting(
    sf::RenderWindow &window,
    sf::View &uiView,
    sf::Font &font,
    unsigned int screenWidth,
    unsigned int screenHeight,
    const std::string &connectionStatus,
    const std::string &inputText,
    bool isServerIPMode)
{
  window.setView(uiView);
  window.clear(sf::Color(30, 30, 50));

  UIHelper::drawCenteredText(window, font, "Multiplayer", 48, sf::Color::White, 80.f, static_cast<float>(screenWidth));
  UIHelper::drawCenteredText(window, font, connectionStatus, 24, sf::Color::Yellow, 180.f, static_cast<float>(screenWidth));

  std::string labelText = isServerIPMode ? "Server IP:" : "Room Code (or press C to create):";
  UIHelper::drawCenteredText(window, font, labelText, 24, sf::Color::White, 260.f, static_cast<float>(screenWidth));

  UIHelper::drawInputBox(window, font, inputText,
                         (screenWidth - 400.f) / 2.f, 300.f, 400.f, 50.f);

  UIHelper::drawCenteredText(window, font, "Press ENTER to confirm, ESC to cancel",
                             20, sf::Color(150, 150, 150), 400.f, static_cast<float>(screenWidth));

  window.display();
}

void MultiplayerHandler::renderWaitingForPlayer(
    sf::RenderWindow &window,
    sf::View &uiView,
    sf::Font &font,
    unsigned int screenWidth,
    unsigned int screenHeight,
    const std::string &roomCode)
{
  window.setView(uiView);
  window.clear(sf::Color(30, 30, 50));

  UIHelper::drawCenteredText(window, font, "Waiting for Player", 48, sf::Color::White, 80.f, static_cast<float>(screenWidth));
  UIHelper::drawCenteredText(window, font, "Room Code: " + roomCode, 36, sf::Color::Green, 200.f, static_cast<float>(screenWidth));
  UIHelper::drawCenteredText(window, font, "Share this code with your friend!", 24, sf::Color::Yellow, 280.f, static_cast<float>(screenWidth));

  // 动画点
  static float dotTime = 0;
  dotTime += 0.016f;
  int dots = static_cast<int>(dotTime * 2) % 4;
  std::string waiting = "Waiting";
  for (int i = 0; i < dots; i++)
    waiting += ".";

  UIHelper::drawCenteredText(window, font, waiting, 28, sf::Color::White, 360.f, static_cast<float>(screenWidth));
  UIHelper::drawCenteredText(window, font, "Press ESC to cancel", 20, sf::Color(150, 150, 150), 450.f, static_cast<float>(screenWidth));

  window.display();
}

void MultiplayerHandler::renderMultiplayer(
    MultiplayerContext &ctx,
    MultiplayerState &state)
{
  ctx.window.clear(sf::Color(30, 30, 30));
  ctx.window.setView(ctx.gameView);

  // 渲染迷宫
  ctx.maze.render(ctx.window);

  // 如果处于放置模式，绘制预览
  if (ctx.placementMode && ctx.player && ctx.player->getWallsInBag() > 0)
  {
    // 获取鼠标在世界坐标中的位置
    sf::Vector2i mousePixelPos = sf::Mouse::getPosition(ctx.window);
    sf::Vector2f mouseWorldPos = ctx.window.mapPixelToCoords(mousePixelPos, ctx.gameView);

    // 获取对应的格子位置
    GridPos grid = ctx.maze.worldToGrid(mouseWorldPos);
    sf::Vector2f gridCenter = ctx.maze.gridToWorld(grid);

    // 检查是否有坦克在该位置
    bool hasTankAtPos = false;
    float checkRadius = ctx.maze.getTileSize() * 1.0f;

    // 检查玩家
    if (ctx.player)
    {
      float dist = std::hypot(ctx.player->getPosition().x - gridCenter.x, ctx.player->getPosition().y - gridCenter.y);
      if (dist < checkRadius)
        hasTankAtPos = true;
    }
    // 检查另一个玩家
    if (!hasTankAtPos && ctx.otherPlayer)
    {
      float dist = std::hypot(ctx.otherPlayer->getPosition().x - gridCenter.x, ctx.otherPlayer->getPosition().y - gridCenter.y);
      if (dist < checkRadius)
        hasTankAtPos = true;
    }
    // 检查NPC
    if (!hasTankAtPos)
    {
      for (const auto &enemy : ctx.enemies)
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
    float tileSize = ctx.maze.getTileSize();
    sf::RectangleShape preview({tileSize - 4.f, tileSize - 4.f});
    preview.setPosition({gridCenter.x - (tileSize - 4.f) / 2.f, gridCenter.y - (tileSize - 4.f) / 2.f});

    if (!hasTankAtPos && ctx.maze.canPlaceWall(mouseWorldPos))
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
    ctx.window.draw(preview);
  }

  // 渲染终点
  sf::Vector2f exitPos = ctx.maze.getExitPosition();
  sf::RectangleShape exitMarker({TILE_SIZE * 0.8f, TILE_SIZE * 0.8f});
  exitMarker.setFillColor(sf::Color(0, 255, 0, 100));
  exitMarker.setOutlineColor(sf::Color::Green);
  exitMarker.setOutlineThickness(3.f);
  exitMarker.setPosition({exitPos.x - TILE_SIZE * 0.4f, exitPos.y - TILE_SIZE * 0.4f});
  ctx.window.draw(exitMarker);

  // 渲染NPC
  renderNpcs(ctx, state);

  // 渲染另一个玩家
  if (ctx.otherPlayer)
  {
    ctx.otherPlayer->render(ctx.window);

    // Escape 模式下显示倒地玩家的特殊标记
    if (state.isEscapeMode && state.otherPlayerDead)
    {
      sf::Vector2f pos = ctx.otherPlayer->getPosition();

      // 画一个红色十字表示需要救援
      sf::RectangleShape crossH({30.f, 8.f});
      crossH.setFillColor(sf::Color(255, 50, 50, 200));
      crossH.setPosition({pos.x - 15.f, pos.y - 4.f - 30.f});
      ctx.window.draw(crossH);

      sf::RectangleShape crossV({8.f, 30.f});
      crossV.setFillColor(sf::Color(255, 50, 50, 200));
      crossV.setPosition({pos.x - 4.f, pos.y - 15.f - 30.f});
      ctx.window.draw(crossV);

      // 显示 "DOWNED" 文字
      sf::Text downedText(ctx.font);
      downedText.setString("DOWNED");
      downedText.setCharacterSize(12);
      downedText.setFillColor(sf::Color::Red);
      sf::FloatRect bounds = downedText.getLocalBounds();
      downedText.setPosition({pos.x - bounds.size.x / 2.f, pos.y + 25.f});
      ctx.window.draw(downedText);
    }
    else if (state.otherPlayerReachedExit)
    {
      UIHelper::drawTeamMarker(ctx.window,
                               {ctx.otherPlayer->getPosition().x, ctx.otherPlayer->getPosition().y - 25.f},
                               15.f, sf::Color(0, 255, 0, 150));
    }
  }

  // 渲染本地玩家
  if (ctx.player)
  {
    ctx.player->render(ctx.window);

    // 如果本地玩家死亡，显示等待救援的 UI
    if (state.isEscapeMode && state.localPlayerDead)
    {
      sf::Vector2f pos = ctx.player->getPosition();

      // 画一个红色十字
      sf::RectangleShape crossH({30.f, 8.f});
      crossH.setFillColor(sf::Color(255, 50, 50, 200));
      crossH.setPosition({pos.x - 15.f, pos.y - 4.f - 30.f});
      ctx.window.draw(crossH);

      sf::RectangleShape crossV({8.f, 30.f});
      crossV.setFillColor(sf::Color(255, 50, 50, 200));
      crossV.setPosition({pos.x - 4.f, pos.y - 15.f - 30.f});
      ctx.window.draw(crossV);

      // 显示被救援进度（如果正在被救）
      if (state.beingRescued && state.rescueProgress > 0.f)
      {
        float progress = state.rescueProgress / 3.0f; // 3秒总时间

        // 进度条背景
        sf::RectangleShape bgBar({60.f, 8.f});
        bgBar.setFillColor(sf::Color(50, 50, 50, 200));
        bgBar.setPosition({pos.x - 30.f, pos.y + 35.f});
        ctx.window.draw(bgBar);

        // 进度条
        sf::RectangleShape progressBar({60.f * progress, 8.f});
        progressBar.setFillColor(sf::Color(50, 200, 50, 255));
        progressBar.setPosition({pos.x - 30.f, pos.y + 35.f});
        ctx.window.draw(progressBar);
      }
    }
    else if (state.localPlayerReachedExit)
    {
      UIHelper::drawTeamMarker(ctx.window,
                               {ctx.player->getPosition().x, ctx.player->getPosition().y - 25.f},
                               15.f, sf::Color(0, 255, 0, 150));
    }
  }

  // 渲染救援提示和进度（靠近倒地队友时）
  if (state.isEscapeMode && state.canRescue && ctx.otherPlayer)
  {
    sf::Vector2f otherPos = ctx.otherPlayer->getPosition();

    if (state.isRescuing)
    {
      // 显示救援进度条
      float progress = state.rescueProgress / 3.0f; // 3秒总时间

      // 进度条背景
      sf::RectangleShape bgBar({80.f, 10.f});
      bgBar.setFillColor(sf::Color(50, 50, 50, 200));
      bgBar.setPosition({otherPos.x - 40.f, otherPos.y - 60.f});
      ctx.window.draw(bgBar);

      // 进度条
      sf::RectangleShape progressBar({80.f * progress, 10.f});
      progressBar.setFillColor(sf::Color(50, 200, 50, 255));
      progressBar.setPosition({otherPos.x - 40.f, otherPos.y - 60.f});
      ctx.window.draw(progressBar);

      // 显示救援中文字
      sf::Text rescueText(ctx.font);
      rescueText.setString("Rescuing...");
      rescueText.setCharacterSize(14);
      rescueText.setFillColor(sf::Color::Yellow);
      sf::FloatRect bounds = rescueText.getLocalBounds();
      rescueText.setPosition({otherPos.x - bounds.size.x / 2.f, otherPos.y - 80.f});
      ctx.window.draw(rescueText);
    }
    else
    {
      // 显示按 F 救援提示
      sf::Text rescueHint(ctx.font);
      rescueHint.setString("Hold F to rescue");
      rescueHint.setCharacterSize(14);
      rescueHint.setFillColor(sf::Color::Yellow);
      sf::FloatRect bounds = rescueHint.getLocalBounds();
      rescueHint.setPosition({otherPos.x - bounds.size.x / 2.f, otherPos.y - 60.f});
      ctx.window.draw(rescueHint);
    }
  }

  // 渲染终点交互提示和进度（在终点区域内时，且游戏未结束）
  // Battle模式下对方先到达终点时游戏已结束，不再显示提示
  bool gameEnded = (!state.isEscapeMode && state.otherPlayerReachedExit);
  if (state.isAtExitZone && !state.localPlayerDead && !state.localPlayerReachedExit && !gameEnded)
  {
    sf::Vector2f exitPos = ctx.maze.getExitPosition();

    if (state.isHoldingExit)
    {
      // 显示确认进度条
      const float EXIT_HOLD_TIME = 3.0f;
      float progress = state.exitHoldProgress / EXIT_HOLD_TIME;

      // 进度条背景
      sf::RectangleShape bgBar({80.f, 10.f});
      bgBar.setFillColor(sf::Color(50, 50, 50, 200));
      bgBar.setPosition({exitPos.x - 40.f, exitPos.y - 60.f});
      ctx.window.draw(bgBar);

      // 进度条
      sf::RectangleShape progressBar({80.f * progress, 10.f});
      progressBar.setFillColor(sf::Color(50, 200, 255, 255));
      progressBar.setPosition({exitPos.x - 40.f, exitPos.y - 60.f});
      ctx.window.draw(progressBar);

      // 显示确认中文字
      sf::Text confirmText(ctx.font);
      confirmText.setString("Exiting...");
      confirmText.setCharacterSize(16);
      confirmText.setFillColor(sf::Color::Cyan);
      sf::FloatRect bounds = confirmText.getLocalBounds();
      confirmText.setPosition({exitPos.x - bounds.size.x / 2.f, exitPos.y - 85.f});
      ctx.window.draw(confirmText);
    }
    else
    {
      // 显示按 E 确认提示
      sf::Text exitHint(ctx.font);
      exitHint.setString("Hold E to exit");
      exitHint.setCharacterSize(16);
      exitHint.setFillColor(sf::Color::Cyan);
      sf::FloatRect bounds = exitHint.getLocalBounds();
      exitHint.setPosition({exitPos.x - bounds.size.x / 2.f, exitPos.y - 60.f});
      ctx.window.draw(exitHint);
    }
  }

  // 渲染子弹
  for (const auto &bullet : ctx.bullets)
  {
    bullet->render(ctx.window);
  }

  // NPC激活提示
  if (state.nearbyNpcIndex >= 0 && state.nearbyNpcIndex < static_cast<int>(ctx.enemies.size()))
  {
    auto &npc = ctx.enemies[state.nearbyNpcIndex];
    sf::Vector2f npcPos = npc->getPosition();

    sf::Text activateHint(ctx.font);
    if (ctx.player->getCoins() >= 3)
    {
      activateHint.setString("Press R (3 coins)");
      activateHint.setFillColor(sf::Color::Yellow);
    }
    else
    {
      activateHint.setString("Need 3 coins!");
      activateHint.setFillColor(sf::Color::Red);
    }
    activateHint.setCharacterSize(14);
    sf::FloatRect hintBounds = activateHint.getLocalBounds();
    activateHint.setPosition({npcPos.x - hintBounds.size.x / 2.f, npcPos.y - 55.f});
    ctx.window.draw(activateHint);
  }

  // 暗黑模式遮罩（在游戏世界上方，UI下方）
  if (ctx.isDarkMode)
  {
    renderDarkModeOverlay(ctx);
  }

  // 渲染UI
  renderUI(ctx, state);

  ctx.window.display();
}

void MultiplayerHandler::renderNpcs(
    MultiplayerContext &ctx,
    MultiplayerState &state)
{
  for (const auto &npc : ctx.enemies)
  {
    if (npc->isDead())
      continue;

    npc->draw(ctx.window);
    npc->drawHealthBar(ctx.window);

    sf::Vector2f npcPos = npc->getPosition();
    // Battle 模式：显示阵营标记
    if (!state.isEscapeMode)
    {
      if (npc->isActivated())
      {
        sf::Color markerColor = (npc->getTeam() == ctx.player->getTeam())
                                    ? sf::Color(0, 255, 0, 200)  // 己方：绿色
                                    : sf::Color(255, 0, 0, 200); // 敌方：红色
        UIHelper::drawTeamMarker(ctx.window, {npcPos.x, npcPos.y - 27.f}, 8.f, markerColor);
      }
      else
      {
        // 未激活：灰色
        UIHelper::drawTeamMarker(ctx.window, {npcPos.x, npcPos.y - 27.f}, 8.f, sf::Color(150, 150, 150, 200));
      }
    }
  }
}

void MultiplayerHandler::renderUI(
    MultiplayerContext &ctx,
    MultiplayerState &state)
{
  ctx.window.setView(ctx.uiView);

  float barWidth = 150.f;
  float barHeight = 20.f;
  float barX = 20.f;
  float barY = 20.f;

  // Self 标签和血条
  sf::Text selfLabel(ctx.font);
  if (state.isEscapeMode && state.localPlayerDead)
  {
    selfLabel.setString("Self [DOWNED]");
    selfLabel.setFillColor(sf::Color::Red);
  }
  else
  {
    selfLabel.setString("Self");
    selfLabel.setFillColor(sf::Color::White);
  }
  selfLabel.setCharacterSize(18);
  selfLabel.setPosition({barX, barY - 2.f});
  ctx.window.draw(selfLabel);

  float selfHealthPercent = ctx.player ? (ctx.player->getHealth() / 100.f) : 0.f;
  sf::Color selfBarColor = (state.isEscapeMode && state.localPlayerDead) ? sf::Color(100, 100, 100) : sf::Color::Green;
  UIHelper::drawHealthBar(ctx.window, barX + 50.f, barY, barWidth, barHeight,
                          selfHealthPercent, selfBarColor);

  // Other 标签和血条
  sf::Text otherLabel(ctx.font);
  if (state.isEscapeMode && state.otherPlayerDead)
  {
    otherLabel.setString("Teammate [DOWNED]");
    otherLabel.setFillColor(sf::Color::Red);
  }
  else if (state.isEscapeMode)
  {
    otherLabel.setString("Teammate");
    otherLabel.setFillColor(sf::Color::Cyan);
  }
  else
  {
    otherLabel.setString("Other");
    otherLabel.setFillColor(sf::Color::White);
  }
  otherLabel.setCharacterSize(18);
  otherLabel.setPosition({barX, barY + 30.f - 2.f});
  ctx.window.draw(otherLabel);

  float otherHealthPercent = ctx.otherPlayer ? (ctx.otherPlayer->getHealth() / 100.f) : 0.f;
  sf::Color otherBarColor = (state.isEscapeMode && state.otherPlayerDead) ? sf::Color(100, 100, 100) : sf::Color::Cyan;
  UIHelper::drawHealthBar(ctx.window, barX + 50.f, barY + 30.f, barWidth, barHeight,
                          otherHealthPercent, otherBarColor);

  // Escape 模式显示到达终点状态
  if (state.isEscapeMode)
  {
    float statusY = barY + 60.f;

    // 本地玩家到达状态
    sf::Text selfStatus(ctx.font);
    if (state.localPlayerReachedExit && !state.localPlayerDead)
    {
      selfStatus.setString("You: ESCAPED!");
      selfStatus.setFillColor(sf::Color::Green);
    }
    else if (state.localPlayerDead)
    {
      selfStatus.setString("You: DOWNED - Wait for rescue!");
      selfStatus.setFillColor(sf::Color::Red);
    }
    else
    {
      selfStatus.setString("You: Reach the exit!");
      selfStatus.setFillColor(sf::Color(180, 180, 180));
    }
    selfStatus.setCharacterSize(16);
    selfStatus.setPosition({barX, statusY});
    ctx.window.draw(selfStatus);

    // 队友到达状态
    sf::Text otherStatus(ctx.font);
    if (state.otherPlayerReachedExit && !state.otherPlayerDead)
    {
      otherStatus.setString("Teammate: ESCAPED!");
      otherStatus.setFillColor(sf::Color::Green);
    }
    else if (state.otherPlayerDead)
    {
      otherStatus.setString("Teammate: DOWNED - Go rescue!");
      otherStatus.setFillColor(sf::Color::Red);
    }
    else
    {
      otherStatus.setString("Teammate: Not escaped yet");
      otherStatus.setFillColor(sf::Color(180, 180, 180));
    }
    otherStatus.setCharacterSize(16);
    otherStatus.setPosition({barX, statusY + 22.f});
    ctx.window.draw(otherStatus);
  }
  else
  {
    // Battle 模式：金币显示
    sf::Text coinsText(ctx.font);
    coinsText.setString("Coins: " + std::to_string(ctx.player ? ctx.player->getCoins() : 0));
    coinsText.setCharacterSize(20);
    coinsText.setFillColor(sf::Color::Yellow);
    coinsText.setPosition({barX, barY + 60.f});
    ctx.window.draw(coinsText);
  }

  // 墙壁背包显示
  float wallsY = state.isEscapeMode ? barY + 110.f : barY + 85.f;
  sf::Text wallsText(ctx.font);
  wallsText.setString("Walls: " + std::to_string(ctx.player ? ctx.player->getWallsInBag() : 0));
  wallsText.setCharacterSize(20);
  wallsText.setFillColor(sf::Color(139, 90, 43)); // 棕色
  wallsText.setPosition({barX, wallsY});
  ctx.window.draw(wallsText);

  // Escape模式暗黑模式下显示剩余存活的敌人数量（Battle模式不显示）
  float enemyCountY = wallsY + 25.f;
  if (ctx.isDarkMode && state.isEscapeMode)
  {
    int aliveEnemies = 0;
    for (const auto &enemy : ctx.enemies)
    {
      if (!enemy->isDead())
        aliveEnemies++;
    }
    sf::Text enemyCountText(ctx.font);
    enemyCountText.setString("Enemies: " + std::to_string(aliveEnemies));
    enemyCountText.setCharacterSize(20);
    enemyCountText.setFillColor(sf::Color(255, 100, 100)); // 红色
    enemyCountText.setPosition({barX, enemyCountY});
    ctx.window.draw(enemyCountText);
    enemyCountY += 25.f;
  }

  // 墙壁放置模式提示
  if (ctx.placementMode)
  {
    sf::Text placeHint(ctx.font);
    placeHint.setString("[PLACEMENT MODE] Click to place wall, Space to cancel");
    placeHint.setCharacterSize(20);
    placeHint.setFillColor(sf::Color::Yellow);
    sf::FloatRect hintBounds = placeHint.getLocalBounds();
    placeHint.setPosition({(static_cast<float>(ctx.screenWidth) - hintBounds.size.x) / 2.f, 20.f});
    ctx.window.draw(placeHint);
  }
  else if (ctx.player && ctx.player->getWallsInBag() > 0)
  {
    // 提示可以按B进入放置模式
    sf::Text bagHint(ctx.font);
    bagHint.setString("Press SPACE to place walls");
    bagHint.setCharacterSize(18);
    bagHint.setFillColor(sf::Color(150, 150, 150));
    bagHint.setPosition({barX, enemyCountY});
    ctx.window.draw(bagHint);
  }

  // 显示操作提示
  sf::Text controlHint(ctx.font);
  if (state.isEscapeMode)
  {
    controlHint.setString("WASD: Move | Mouse: Aim | Click: Shoot | F: Rescue teammate");
  }
  else
  {
    controlHint.setString("WASD: Move | Mouse: Aim | Click: Shoot | R: Activate NPC");
  }
  controlHint.setCharacterSize(14);
  controlHint.setFillColor(sf::Color(150, 150, 150));
  controlHint.setPosition({barX, static_cast<float>(ctx.screenHeight) - 30.f});
  ctx.window.draw(controlHint);

  // 渲染小地图（左下角）- 暗黑模式下隐藏
  if (!ctx.isDarkMode)
  {
    renderMinimap(ctx, state);
  }
}

void MultiplayerHandler::renderMinimap(
    MultiplayerContext &ctx,
    MultiplayerState &state)
{
  // 小地图参数
  const float minimapSize = 150.f;  // 小地图尺寸
  const float minimapMargin = 20.f; // 边距
  const float minimapX = minimapMargin;
  const float minimapY = static_cast<float>(ctx.screenHeight) - minimapSize - minimapMargin - 35.f; // 留出操作提示空间

  // 绘制小地图背景
  sf::RectangleShape minimapBg({minimapSize, minimapSize});
  minimapBg.setPosition({minimapX, minimapY});
  minimapBg.setFillColor(sf::Color(20, 20, 20, 200));
  minimapBg.setOutlineColor(sf::Color(100, 100, 100, 255));
  minimapBg.setOutlineThickness(2.f);
  ctx.window.draw(minimapBg);

  // 计算地图范围（基于迷宫大小）
  sf::Vector2f mazeSize = ctx.maze.getSize();
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
  for (const auto &npc : ctx.enemies)
  {
    if (npc->isDead())
      continue;

    sf::Vector2f npcPos = npc->getPosition();
    sf::Vector2f npcMiniPos = worldToMinimap(npcPos);

    sf::CircleShape npcDot(3.f);
    npcDot.setPosition({npcMiniPos.x - 3.f, npcMiniPos.y - 3.f});

    if (state.isEscapeMode)
    {
      // Escape模式：未激活灰色，已激活是敌方（红色）
      if (npc->isActivated())
      {
        npcDot.setFillColor(GameColors::MinimapEnemyNpc);
      }
      else
      {
        npcDot.setFillColor(GameColors::MinimapInactiveNpc); // 未激活灰色
      }
    }
    else
    {
      // Battle模式：根据阵营显示
      if (npc->isActivated())
      {
        int localTeam = ctx.player ? ctx.player->getTeam() : 1;
        if (npc->getTeam() == localTeam)
        {
          npcDot.setFillColor(GameColors::MinimapAllyNpc); // 己方NPC浅蓝色
        }
        else
        {
          npcDot.setFillColor(GameColors::MinimapEnemyNpc); // 敌方NPC红色
        }
      }
      else
      {
        npcDot.setFillColor(GameColors::MinimapInactiveNpc); // 未激活灰色
      }
    }
    ctx.window.draw(npcDot);
  }

  // 绘制对方玩家
  if (ctx.otherPlayer)
  {
    sf::Vector2f otherPos = ctx.otherPlayer->getPosition();
    sf::Vector2f otherMiniPos = worldToMinimap(otherPos);

    sf::CircleShape otherDot(4.f);
    otherDot.setPosition({otherMiniPos.x - 4.f, otherMiniPos.y - 4.f});

    if (state.isEscapeMode)
    {
      // Escape模式：队友（青色）
      if (state.otherPlayerDead)
      {
        otherDot.setFillColor(GameColors::MinimapDowned); // 倒地灰色
      }
      else
      {
        otherDot.setFillColor(GameColors::MinimapAlly);
      }
    }
    else
    {
      // Battle模式：敌方玩家（紫色）
      otherDot.setFillColor(GameColors::MinimapEnemy);
    }
    ctx.window.draw(otherDot);
  }

  // 绘制本地玩家（黄色，最后绘制以确保在最上层）
  if (ctx.player)
  {
    sf::Vector2f playerPos = ctx.player->getPosition();
    sf::Vector2f playerMiniPos = worldToMinimap(playerPos);

    sf::CircleShape playerDot(4.f);
    playerDot.setPosition({playerMiniPos.x - 4.f, playerMiniPos.y - 4.f});

    if (state.isEscapeMode && state.localPlayerDead)
    {
      playerDot.setFillColor(GameColors::MinimapDowned); // 倒地灰色
    }
    else
    {
      playerDot.setFillColor(GameColors::MinimapPlayer);
    }
    ctx.window.draw(playerDot);
  }

  // 小地图标签
  sf::Text minimapLabel(ctx.font);
  minimapLabel.setString("Minimap");
  minimapLabel.setCharacterSize(12);
  minimapLabel.setFillColor(sf::Color(180, 180, 180));
  minimapLabel.setPosition({minimapX + 5.f, minimapY + 3.f});
  ctx.window.draw(minimapLabel);
}

void MultiplayerHandler::renderDarkModeOverlay(MultiplayerContext &ctx)
{
  if (!ctx.player)
    return;

  // 保存当前视图
  sf::View currentView = ctx.window.getView();

  // 切换到游戏视图来绘制遮罩
  ctx.window.setView(ctx.gameView);

  // 获取玩家位置和视图尺寸
  sf::Vector2f playerPos = ctx.player->getPosition();
  sf::Vector2f viewSize = ctx.gameView.getSize();

  // 确保纹理已初始化（使用视图尺寸）
  unsigned int texWidth = static_cast<unsigned int>(viewSize.x);
  unsigned int texHeight = static_cast<unsigned int>(viewSize.y);
  initDarkModeTexture(texWidth, texHeight);

  if (s_darkModeTextureInitialized && s_darkModeSprite)
  {
    // 将遮罩sprite定位到玩家位置（中心对齐，纹理是2倍大小）
    s_darkModeSprite->setPosition({playerPos.x - viewSize.x, playerPos.y - viewSize.y});
    ctx.window.draw(*s_darkModeSprite);
  }

  // 恢复之前的视图
  ctx.window.setView(currentView);
}