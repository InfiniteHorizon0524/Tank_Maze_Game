#pragma once

#include <SFML/Graphics.hpp>
#include <memory>
#include "Utils.hpp"
#include "HealthBar.hpp"

class Tank
{
public:
  Tank();
  Tank(float x, float y, sf::Color color = sf::Color::Blue);

  bool loadTextures(const std::string &hullPath, const std::string &turretPath);

  void handleInput(const sf::Event &event);
  void update(float dt, sf::Vector2f mousePos);
  void draw(sf::RenderWindow &window) const;
  void render(sf::RenderWindow &window) const { draw(window); }
  void drawUI(sf::RenderWindow &window) const; // 绘制 UI（血条在左上角）

  void setPosition(sf::Vector2f pos);
  sf::Vector2f getPosition() const;
  float getTurretAngle() const;

  // 获取/设置旋转角度
  float getRotation() const { return m_hullAngle; }
  void setRotation(float angle)
  {
    m_hullAngle = angle;
    if (m_hull)
      m_hull->setRotation(sf::degrees(m_hullAngle));
  }
  float getTurretRotation() const { return m_turretAngle; }
  void setTurretRotation(float angle)
  {
    m_turretAngle = angle;
    if (m_turret)
      m_turret->setRotation(sf::degrees(m_turretAngle));
  }

  // 获取枪口位置
  sf::Vector2f getGunPosition() const;
  sf::Vector2f getBulletSpawnPosition() const { return getGunPosition(); }

  // 检查是否在射击
  bool isShooting() const { return m_mouseHeld; }

  // 检查是否发射了子弹（用于网络同步）
  bool hasFiredBullet();

  // 获取/设置生命值
  float getHealth() const { return m_healthBar.getHealth(); }
  void setHealth(float health) { m_healthBar.setHealth(health); }

  // 受到伤害
  void takeDamage(float damage);
  bool isDead() const { return m_healthBar.isDead(); }

  // 治疗（恢复百分比血量）
  void heal(float percent)
  {
    float maxHp = m_healthBar.getMaxHealth();
    float current = m_healthBar.getHealth();
    float healAmount = maxHp * percent;
    m_healthBar.setHealth(std::min(current + healAmount, maxHp));
  }

  // 获取碰撞半径 (减小以适应迷宫通道)
  float getCollisionRadius() const { return 12.f * m_scale / 0.25f; }

  // 获取当前移动向量（用于墙壁滑动）
  sf::Vector2f getMovement(float dt) const;

  // 设置缩放
  void setScale(float scale) { m_scale = scale; }

  // 金币系统
  int getCoins() const { return m_coins; }
  void setCoins(int coins) { m_coins = coins; }
  bool spendCoins(int amount)
  {
    if (m_coins >= amount)
    {
      m_coins -= amount;
      return true;
    }
    return false;
  }
  void addCoins(int amount) { m_coins += amount; }

  // 背包系统（存储棕色格子）
  int getWallsInBag() const { return m_wallsInBag; }
  void setWallsInBag(int count) { m_wallsInBag = count; }
  void addWallToBag() { m_wallsInBag++; }
  bool useWallFromBag()
  {
    if (m_wallsInBag > 0)
    {
      m_wallsInBag--;
      return true;
    }
    return false;
  }

  // 阵营（用于多人模式，0=未设置，1=玩家1阵营，2=玩家2阵营）
  int getTeam() const { return m_team; }
  void setTeam(int team) { m_team = team; }

private:
  sf::Texture m_hullTexture;
  sf::Texture m_turretTexture;
  std::unique_ptr<sf::Sprite> m_hull;
  std::unique_ptr<sf::Sprite> m_turret;

  // 简易绘图模式（无纹理时）
  sf::Color m_color = sf::Color::Blue;
  bool m_useSimpleGraphics = true;

  HealthBar m_healthBar;

  // 移动状态
  bool m_keyW = false;
  bool m_keyS = false;
  bool m_keyA = false;
  bool m_keyD = false;
  bool m_mouseHeld = false;

  // 角度
  float m_hullAngle = 0.f;
  float m_turretAngle = 0.f;

  // 射击检测
  bool m_firedBullet = false;
  float m_shootTimer = 0.f;
  const float m_shootCooldown = 0.3f;

  // 配置
  float m_moveSpeed = 200.f;
  const float m_rotationSpeed = 5.f;
  float m_scale = 0.25f;
  const float m_gunLength = 25.f;

  sf::Vector2f m_position;

  // 金币系统（多人模式）
  int m_coins = 10; // 初始10个金币

  // 背包系统
  int m_wallsInBag = 0; // 初始0个棕色格子

  // 阵营（多人模式）
  int m_team = 0; // 0=未设置，1=玩家1，2=玩家2
};
