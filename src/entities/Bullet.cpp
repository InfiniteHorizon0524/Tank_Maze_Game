#include "Bullet.hpp"
#include "Maze.hpp"
#include <algorithm>
#include <cmath>

Bullet::Bullet(const sf::Texture &texture, sf::Vector2f position, float angleDegrees, float speed, BulletOwner owner)
    : m_texture(&texture), m_owner(owner), m_speed(speed), m_angle(angleDegrees)
{
  m_sprite = std::make_unique<sf::Sprite>(texture);
  m_sprite->setOrigin(sf::Vector2f(texture.getSize()) / 2.f);
  m_sprite->setPosition(position);
  m_sprite->setRotation(sf::degrees(angleDegrees));
  m_sprite->setScale({0.35f, 0.35f});
  m_position = position;
  m_useSimpleGraphics = false;

  // 计算速度向量
  float angleRad = (angleDegrees - 90.f) * Utils::PI / 180.f;
  m_velocity = {std::cos(angleRad) * speed, std::sin(angleRad) * speed};
}

Bullet::Bullet(float x, float y, float angleDegrees, bool isPlayer, sf::Color color)
    : m_position(x, y), m_color(color), m_owner(isPlayer ? BulletOwner::Player : BulletOwner::Enemy),
      m_speed(500.f), m_angle(angleDegrees), m_useSimpleGraphics(true)
{
  float angleRad = (angleDegrees - 90.f) * Utils::PI / 180.f;
  m_velocity = {std::cos(angleRad) * m_speed, std::sin(angleRad) * m_speed};
}

void Bullet::update(float dt)
{
  m_position += m_velocity * dt;
  if (m_sprite)
  {
    m_sprite->move(m_velocity * dt);
  }

  // 自动失活如果走太远
  if (m_position.x < -100 || m_position.x > 10000 ||
      m_position.y < -100 || m_position.y > 10000)
  {
    m_active = false;
  }
}

void Bullet::draw(sf::RenderWindow &window) const
{
  if (m_useSimpleGraphics || !m_sprite)
  {
    sf::CircleShape bullet(5.f);
    bullet.setOrigin({5.f, 5.f});
    bullet.setPosition(m_position);
    bullet.setFillColor(m_color);
    window.draw(bullet);
  }
  else
  {
    window.draw(*m_sprite);
  }
}

sf::Vector2f Bullet::getPosition() const
{
  return m_position;
}

void Bullet::checkBounds(float width, float height)
{
  sf::Vector2f pos = m_position;
  if (pos.x < -50 || pos.x > width + 50 || pos.y < -50 || pos.y > height + 50)
  {
    m_active = false;
  }
}

// BulletManager 实现
void BulletManager::spawn(sf::Vector2f position, float angleDegrees, float speed, BulletOwner owner, float damage)
{
  if (m_texture)
  {
    m_bullets.emplace_back(*m_texture, position, angleDegrees, speed, owner);
    m_bullets.back().setDamage(damage);
  }
}

void BulletManager::update(float dt, float screenWidth, float screenHeight)
{
  for (auto &bullet : m_bullets)
  {
    bullet.update(dt);
    bullet.checkBounds(screenWidth, screenHeight);
  }

  // 移除不活跃的子弹
  m_bullets.erase(
      std::remove_if(m_bullets.begin(), m_bullets.end(),
                     [](const Bullet &b)
                     { return !b.isActive(); }),
      m_bullets.end());
}

void BulletManager::draw(sf::RenderWindow &window) const
{
  for (const auto &bullet : m_bullets)
  {
    bullet.draw(window);
  }
}

float BulletManager::checkCollision(sf::Vector2f targetPos, float targetRadius, BulletOwner ignoreOwner)
{
  float totalDamage = 0.f;

  for (auto &bullet : m_bullets)
  {
    if (!bullet.isActive() || bullet.getOwner() == ignoreOwner)
      continue;

    sf::Vector2f bulletPos = bullet.getPosition();
    float dx = bulletPos.x - targetPos.x;
    float dy = bulletPos.y - targetPos.y;
    float distance = std::sqrt(dx * dx + dy * dy);

    if (distance < targetRadius)
    {
      bullet.setInactive();
      totalDamage += bullet.getDamage();
    }
  }

  return totalDamage;
}

void BulletManager::checkWallCollision(Maze &maze)
{
  for (auto &bullet : m_bullets)
  {
    if (!bullet.isActive())
      continue;

    // 检查子弹是否击中墙壁
    if (maze.bulletHit(bullet.getPosition(), bullet.getDamage()))
    {
      bullet.setInactive();
    }
  }
}
