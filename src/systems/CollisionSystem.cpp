#include "CollisionSystem.hpp"
#include "AudioManager.hpp"
#include <cmath>
#include <algorithm>

bool CollisionSystem::checkBulletWallCollision(Bullet *bullet, Maze &maze)
{
  return maze.bulletHit(bullet->getPosition(), bullet->getDamage());
}

WallDestroyResult CollisionSystem::checkBulletWallCollisionWithResult(Bullet *bullet, Maze &maze)
{
  return maze.bulletHitWithResult(bullet->getPosition(), bullet->getDamage());
}

void CollisionSystem::handleWallDestroyEffect(const WallDestroyResult &result, Tank *shooter, Maze &maze)
{
  if (!result.destroyed || !shooter)
    return;

  sf::Vector2f listenerPos = shooter->getPosition();

  switch (result.attribute)
  {
  case WallAttribute::Gold:
    // 金色墙：获得2金币
    shooter->addCoins(2);
    // 播放收集金币音效
    AudioManager::getInstance().playSFX(SFXType::CollectCoins, result.position, listenerPos);
    break;

  case WallAttribute::Heal:
    // 治疗墙：恢复25%血量
    shooter->heal(0.25f);
    // 播放 bingo 音效
    AudioManager::getInstance().playSFX(SFXType::Bingo, result.position, listenerPos);
    break;

  case WallAttribute::None:
    // 棕色墙：收集到背包
    shooter->addWallToBag();
    // 播放墙被打爆音效
    AudioManager::getInstance().playSFX(SFXType::WallBroken, result.position, listenerPos);
    break;

  default:
    break;
  }
}

bool CollisionSystem::checkBulletTankCollision(Bullet *bullet, Tank *tank, float extraRadius)
{
  sf::Vector2f bulletPos = bullet->getPosition();
  sf::Vector2f tankPos = tank->getPosition();
  float dist = std::hypot(bulletPos.x - tankPos.x, bulletPos.y - tankPos.y);
  return dist < tank->getCollisionRadius() + extraRadius;
}

bool CollisionSystem::checkBulletNpcCollision(Bullet *bullet, Enemy *npc, float extraRadius)
{
  sf::Vector2f bulletPos = bullet->getPosition();
  sf::Vector2f npcPos = npc->getPosition();
  float dist = std::hypot(bulletPos.x - npcPos.x, bulletPos.y - npcPos.y);
  return dist < npc->getCollisionRadius() + extraRadius;
}

void CollisionSystem::checkSinglePlayerCollisions(
    Tank *player,
    std::vector<std::unique_ptr<Enemy>> &enemies,
    std::vector<std::unique_ptr<Bullet>> &bullets,
    Maze &maze)
{
  if (!player)
    return;

  sf::Vector2f listenerPos = player->getPosition();

  // 检查子弹与墙壁、玩家、敌人的碰撞
  for (auto &bullet : bullets)
  {
    if (!bullet->isAlive())
      continue;

    // 检查与墙壁碰撞（使用带属性返回的版本）
    WallDestroyResult wallResult = checkBulletWallCollisionWithResult(bullet.get(), maze);
    bool hitWall = (wallResult.position.x != 0 || wallResult.position.y != 0);

    if (hitWall || wallResult.destroyed)
    {
      // 播放子弹击中墙壁音效
      AudioManager::getInstance().playSFX(SFXType::BulletHitWall, bullet->getPosition(), listenerPos);

      // 如果墙被摧毁且是玩家子弹，处理增益效果
      if (wallResult.destroyed && bullet->getOwner() == BulletOwner::Player)
      {
        handleWallDestroyEffect(wallResult, player, maze);
      }

      bullet->setInactive();
      continue;
    }

    // 检查与玩家的碰撞（敌人子弹）
    if (bullet->getOwner() == BulletOwner::Enemy)
    {
      if (checkBulletTankCollision(bullet.get(), player))
      {
        player->takeDamage(bullet->getDamage());
        // 播放子弹击中坦克音效
        AudioManager::getInstance().playSFX(SFXType::BulletHitTank, bullet->getPosition(), listenerPos);
        bullet->setInactive();
        continue;
      }
    }

    // 检查与敌人的碰撞（玩家子弹）
    if (bullet->getOwner() == BulletOwner::Player)
    {
      for (auto &enemy : enemies)
      {
        if (checkBulletNpcCollision(bullet.get(), enemy.get()))
        {
          enemy->takeDamage(bullet->getDamage());
          // 播放子弹击中坦克音效
          AudioManager::getInstance().playSFX(SFXType::BulletHitTank, bullet->getPosition(), listenerPos);

          // 如果敌人死亡，播放爆炸音效
          if (enemy->isDead())
          {
            AudioManager::getInstance().playSFX(SFXType::Explode, enemy->getPosition(), listenerPos);
          }

          bullet->setInactive();
          break;
        }
      }
    }
  }

  // 删除无效子弹
  bullets.erase(
      std::remove_if(bullets.begin(), bullets.end(),
                     [](const std::unique_ptr<Bullet> &b)
                     { return !b->isAlive(); }),
      bullets.end());
}

void CollisionSystem::checkMultiplayerCollisions(
    Tank *player,
    Tank *otherPlayer,
    std::vector<std::unique_ptr<Enemy>> &enemies,
    std::vector<std::unique_ptr<Bullet>> &bullets,
    Maze &maze,
    bool isHost)
{
  if (!player || !otherPlayer)
    return;

  int localTeam = player->getTeam();
  sf::Vector2f listenerPos = player->getPosition();

  for (auto &bullet : bullets)
  {
    if (!bullet->isAlive())
      continue;

    sf::Vector2f bulletPos = bullet->getPosition();
    int bulletTeam = bullet->getTeam();

    // 墙壁碰撞检测：只有房主处理伤害和同步
    // 非房主只检测是否击中（用于播放音效和销毁子弹），不处理墙壁伤害
    if (isHost)
    {
      // 房主：处理墙壁伤害并同步给非房主
      WallDestroyResult wallResult = checkBulletWallCollisionWithResult(bullet.get(), maze);
      bool hitWall = (wallResult.position.x != 0 || wallResult.position.y != 0);

      if (hitWall || wallResult.destroyed)
      {
        // 播放子弹击中墙壁音效
        AudioManager::getInstance().playSFX(SFXType::BulletHitWall, bulletPos, listenerPos);

        // 判断子弹是谁发射的
        // Player = 本地玩家（房主），OtherPlayer = 对方玩家（非房主），Enemy = NPC
        BulletOwner owner = bullet->getOwner();
        // destroyerId: 0=房主（本地玩家），1=非房主（对方玩家）
        // NPC 打掉的墙不给玩家奖励，设为 -1
        int destroyerId = (owner == BulletOwner::Player) ? 0 : (owner == BulletOwner::OtherPlayer) ? 1
                                                                                                   : -1;

        // 同步墙壁伤害给非房主（包含摧毁者ID）
        NetworkManager::getInstance().sendWallDamage(
            wallResult.gridY, wallResult.gridX,
            bullet->getDamage(),
            wallResult.destroyed,
            static_cast<int>(wallResult.attribute),
            destroyerId);

        // 如果墙被摧毁且有属性效果，处理增益
        if (wallResult.destroyed)
        {
          // 房主端：房主打掉的墙，给房主加效果
          if (owner == BulletOwner::Player)
          {
            handleWallDestroyEffect(wallResult, player, maze);
          }
          // 非房主打掉的墙，增益效果由非房主端的回调处理
        }

        bullet->setInactive();
        continue;
      }
    }
    else
    {
      // 非房主：只检测是否击中墙壁（用于播放音效），不处理伤害
      // 墙壁伤害由房主同步过来
      int c = static_cast<int>(bulletPos.x / maze.getTileSize());
      int r = static_cast<int>(bulletPos.y / maze.getTileSize());
      if (c >= 0 && r >= 0)
      {
        // 简单检查是否在墙壁位置
        if (maze.checkCollision(bulletPos, 1.f))
        {
          // 播放子弹击中墙壁音效
          AudioManager::getInstance().playSFX(SFXType::BulletHitWall, bulletPos, listenerPos);
          bullet->setInactive();
          continue;
        }
      }
    }

    // 判断子弹是否是本地玩家发射的
    bool isLocalPlayerBullet = bullet->getOwner() == BulletOwner::Player;

    // 检查与本地玩家的碰撞（跳过已死亡的玩家）
    bool canHitLocalPlayer = !player->isDead() &&
                             !isLocalPlayerBullet &&
                             (bulletTeam == 0 || bulletTeam != localTeam);
    if (canHitLocalPlayer)
    {
      if (checkBulletTankCollision(bullet.get(), player))
      {
        player->takeDamage(bullet->getDamage());
        // 播放子弹击中坦克音效
        AudioManager::getInstance().playSFX(SFXType::BulletHitTank, bulletPos, listenerPos);

        // 如果本地玩家死亡，播放爆炸音效
        if (player->isDead())
        {
          AudioManager::getInstance().playSFX(SFXType::Explode, player->getPosition(), listenerPos);
        }
        bullet->setInactive();
        continue;
      }
    }

    // 检查与对方玩家的碰撞（跳过已死亡的玩家）
    // 只有当本地玩家和对方玩家是敌对关系时才能击中
    int otherTeam = otherPlayer->getTeam();
    bool canHitOtherPlayer = false;

    // 跳过已死亡的玩家
    if (otherPlayer->isDead())
    {
      canHitOtherPlayer = false;
    }
    else if (isLocalPlayerBullet)
    {
      // 本地玩家的子弹：只有当对方是敌人（不同阵营）时才能击中
      // Escape 模式下双方都是 team=1，不能互相伤害
      canHitOtherPlayer = (localTeam != otherTeam);
    }
    else
    {
      // NPC子弹（bulletTeam）：
      // team=0 的 NPC 可以攻击任何玩家
      // 其他阵营的 NPC 可以攻击不同阵营的玩家
      canHitOtherPlayer = (bulletTeam == 0) || (bulletTeam != otherTeam);
    }

    if (canHitOtherPlayer)
    {
      if (checkBulletTankCollision(bullet.get(), otherPlayer))
      {
        // 播放子弹击中坦克音效
        AudioManager::getInstance().playSFX(SFXType::BulletHitTank, bulletPos, listenerPos);
        bullet->setInactive();
        continue;
      }
    }

    // 检查与NPC的碰撞
    for (auto &npc : enemies)
    {
      if (!npc->isActivated() || npc->isDead())
        continue;

      int npcTeam = npc->getTeam();

      // 判断子弹是否能击中这个NPC
      bool canHitNpc = false;
      bool isNpcBullet = (bullet->getOwner() == BulletOwner::Enemy);

      if (isLocalPlayerBullet)
      {
        // 玩家子弹：可以打不同阵营的 NPC，或者 team=0 的 NPC（Escape 模式敌人）
        canHitNpc = (npcTeam != localTeam) || (npcTeam == 0);
      }
      else if (bulletTeam == 0)
      {
        // NPC (team=0) 的子弹：可以打玩家阵营的 NPC
        canHitNpc = (npcTeam != 0);
      }
      else
      {
        // 其他阵营 NPC 的子弹：可以打不同阵营的 NPC
        canHitNpc = (bulletTeam != npcTeam);
      }

      if (canHitNpc)
      {
        if (checkBulletNpcCollision(bullet.get(), npc.get()))
        {
          // 播放子弹击中坦克音效
          AudioManager::getInstance().playSFX(SFXType::BulletHitTank, bulletPos, listenerPos);

          // 玩家子弹伤害NPC：只处理本地玩家的子弹
          if (isLocalPlayerBullet)
          {
            if (isHost)
            {
              // 房主端：直接处理伤害并同步
              npc->takeDamage(bullet->getDamage());
              NetworkManager::getInstance().sendNpcDamage(npc->getId(), bullet->getDamage());

              if (npc->isDead())
              {
                AudioManager::getInstance().playSFX(SFXType::Explode, npc->getPosition(), listenerPos);
              }
            }
            else
            {
              // 非房主端：只发送伤害请求给房主，不在本地处理
              NetworkManager::getInstance().sendNpcDamage(npc->getId(), bullet->getDamage());
            }
          }
          // NPC子弹打NPC：房主端处理伤害
          else if (isNpcBullet && isHost)
          {
            // 房主端处理NPC打NPC的伤害（包括team=0的NPC和已激活的NPC）
            npc->takeDamage(bullet->getDamage());
            NetworkManager::getInstance().sendNpcDamage(npc->getId(), bullet->getDamage());

            if (npc->isDead())
            {
              AudioManager::getInstance().playSFX(SFXType::Explode, npc->getPosition(), listenerPos);
            }
          }
          // 对方玩家的子弹：不处理，伤害由网络消息处理

          bullet->setInactive();
          break;
        }
      }
    }
  }

  // 删除无效子弹
  bullets.erase(
      std::remove_if(bullets.begin(), bullets.end(),
                     [](const std::unique_ptr<Bullet> &b)
                     { return !b->isAlive(); }),
      bullets.end());
}
