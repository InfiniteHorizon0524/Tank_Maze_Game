#pragma once

#include <SFML/Graphics.hpp>
#include <vector>
#include <memory>
#include "Utils.hpp"

// 前向声明
class Maze;

enum class BulletOwner
{
  Player,      // 本地玩家
  OtherPlayer, // 对方玩家（多人模式）
  Enemy        // NPC/敌人
};

class Bullet
{
public:
  Bullet(const sf::Texture &texture, sf::Vector2f position, float angleDegrees, float speed, BulletOwner owner);

  // 简易构造函数（不需要纹理）
  Bullet(float x, float y, float angleDegrees, bool isPlayer, sf::Color color = sf::Color::Yellow);

  void update(float dt);
  void draw(sf::RenderWindow &window) const;
  void render(sf::RenderWindow &window) const { draw(window); }

  bool isActive() const { return m_active; }
  bool isAlive() const { return m_active; }
  void setInactive() { m_active = false; }

  // 检查是否出界
  void checkBounds(float width, float height);

  // 碰撞检测
  sf::Vector2f getPosition() const;
  BulletOwner getOwner() const { return m_owner; }
  void setOwner(BulletOwner owner) { m_owner = owner; }

  // 获取伤害值
  float getDamage() const { return m_damage; }
  void setDamage(float damage) { m_damage = damage; }

  // 阵营信息（用于多人模式NPC）
  void setTeam(int team) { m_team = team; }
  int getTeam() const { return m_team; }

private:
  std::unique_ptr<sf::Sprite> m_sprite;
  const sf::Texture *m_texture = nullptr;
  sf::Vector2f m_velocity;
  sf::Vector2f m_position;
  sf::Color m_color = sf::Color::Yellow;
  bool m_active = true;
  bool m_useSimpleGraphics = false;
  BulletOwner m_owner;
  float m_damage = 25.f;
  float m_speed = 500.f;
  float m_angle = 0.f;
  int m_team = 0; // 0=中立, 1=房主阵营, 2=非房主阵营
};

// 管理所有子弹
class BulletManager
{
public:
  void setTexture(const sf::Texture &texture) { m_texture = &texture; }

  void spawn(sf::Vector2f position, float angleDegrees, float speed, BulletOwner owner = BulletOwner::Player, float damage = 25.f);
  void update(float dt, float screenWidth, float screenHeight);
  void draw(sf::RenderWindow &window) const;

  // 检查与目标的碰撞，返回伤害值
  float checkCollision(sf::Vector2f targetPos, float targetRadius, BulletOwner ignoreOwner);

  // 检查与墙壁的碰撞
  void checkWallCollision(Maze &maze);

  // 清空所有子弹
  void clear() { m_bullets.clear(); }

private:
  const sf::Texture *m_texture = nullptr;
  std::vector<Bullet> m_bullets;
};
