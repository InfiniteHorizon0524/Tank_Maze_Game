#pragma once

#include <SFML/Graphics.hpp>
#include <vector>
#include <memory>
#include "Bullet.hpp"
#include "Tank.hpp"
#include "Enemy.hpp"
#include "Maze.hpp"
#include "NetworkManager.hpp"

class CollisionSystem
{
public:
  // 单人模式碰撞检测
  static void checkSinglePlayerCollisions(
      Tank *player,
      std::vector<std::unique_ptr<Enemy>> &enemies,
      std::vector<std::unique_ptr<Bullet>> &bullets,
      Maze &maze);

  // 多人模式碰撞检测
  static void checkMultiplayerCollisions(
      Tank *player,
      Tank *otherPlayer,
      std::vector<std::unique_ptr<Enemy>> &enemies,
      std::vector<std::unique_ptr<Bullet>> &bullets,
      Maze &maze,
      bool isHost);

private:
  // 检查子弹与墙壁碰撞（简单版本，单机模式用）
  static bool checkBulletWallCollision(Bullet *bullet, Maze &maze);

  // 检查子弹与墙壁碰撞（带属性版本，联机模式用）
  static WallDestroyResult checkBulletWallCollisionWithResult(Bullet *bullet, Maze &maze);

  // 处理墙体摧毁效果（给玩家加金币/治疗）
  static void handleWallDestroyEffect(const WallDestroyResult &result, Tank *shooter, Maze &maze);

  // 检查子弹与坦克碰撞
  static bool checkBulletTankCollision(Bullet *bullet, Tank *tank, float extraRadius = 5.f);

  // 检查子弹与NPC碰撞
  static bool checkBulletNpcCollision(Bullet *bullet, Enemy *npc, float extraRadius = 5.f);
};
