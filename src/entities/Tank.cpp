#include "Tank.hpp"
#include "AudioManager.hpp"

Tank::Tank()
    : m_healthBar(200.f, 20.f)
{
  m_healthBar.setMaxHealth(100.f);
  m_healthBar.setHealth(100.f);
  m_healthBar.setPosition({20.f, 20.f});
  m_useSimpleGraphics = true;
}

Tank::Tank(float x, float y, sf::Color color)
    : m_healthBar(200.f, 20.f), m_color(color), m_position(x, y)
{
  m_healthBar.setMaxHealth(100.f);
  m_healthBar.setHealth(100.f);
  m_healthBar.setPosition({20.f, 20.f});
  m_useSimpleGraphics = true;
}

bool Tank::loadTextures(const std::string &hullPath, const std::string &turretPath)
{
  if (!m_hullTexture.loadFromFile(hullPath))
    return false;
  if (!m_turretTexture.loadFromFile(turretPath))
    return false;

  // 创建并设置车身
  m_hull = std::make_unique<sf::Sprite>(m_hullTexture);
  m_hull->setOrigin(sf::Vector2f(m_hullTexture.getSize()) / 2.f);
  m_hull->setPosition({640.f, 360.f});
  m_hull->setScale({m_scale, m_scale});

  // 创建并设置炮塔
  m_turret = std::make_unique<sf::Sprite>(m_turretTexture);
  // 炮塔旋转中心在底部中心（炮塔底座位置）
  auto turretSize = sf::Vector2f(m_turretTexture.getSize());
  m_turret->setOrigin({turretSize.x / 2.f, turretSize.y * 0.75f});
  m_turret->setScale({m_scale, m_scale});

  m_position = m_hull->getPosition();
  m_useSimpleGraphics = false;

  return true;
}
void Tank::handleInput(const sf::Event &event)
{
  // 键盘按下
  if (const auto *keyPressed = event.getIf<sf::Event::KeyPressed>())
  {
    if (keyPressed->code == sf::Keyboard::Key::W)
      m_keyW = true;
    if (keyPressed->code == sf::Keyboard::Key::S)
      m_keyS = true;
    if (keyPressed->code == sf::Keyboard::Key::A)
      m_keyA = true;
    if (keyPressed->code == sf::Keyboard::Key::D)
      m_keyD = true;
  }

  // 键盘释放
  if (const auto *keyReleased = event.getIf<sf::Event::KeyReleased>())
  {
    if (keyReleased->code == sf::Keyboard::Key::W)
      m_keyW = false;
    if (keyReleased->code == sf::Keyboard::Key::S)
      m_keyS = false;
    if (keyReleased->code == sf::Keyboard::Key::A)
      m_keyA = false;
    if (keyReleased->code == sf::Keyboard::Key::D)
      m_keyD = false;
  }

  // 鼠标按下
  if (const auto *mousePressed = event.getIf<sf::Event::MouseButtonPressed>())
  {
    if (mousePressed->button == sf::Mouse::Button::Left)
      m_mouseHeld = true;
  }

  // 鼠标释放
  if (const auto *mouseReleased = event.getIf<sf::Event::MouseButtonReleased>())
  {
    if (mouseReleased->button == sf::Mouse::Button::Left)
      m_mouseHeld = false;
  }
}

void Tank::update(float dt, sf::Vector2f mousePos)
{
  // 更新射击计时器
  m_shootTimer += dt;
  m_firedBullet = false;

  if (m_mouseHeld && m_shootTimer >= m_shootCooldown)
  {
    m_firedBullet = true;
    m_shootTimer = 0.f;
  }

  // 计算移动
  sf::Vector2f movement{0.f, 0.f};
  if (m_keyW)
    movement.y -= m_moveSpeed * dt;
  if (m_keyS)
    movement.y += m_moveSpeed * dt;
  if (m_keyA)
    movement.x -= m_moveSpeed * dt;
  if (m_keyD)
    movement.x += m_moveSpeed * dt;

  // 移动并转向
  if (movement.x != 0.f || movement.y != 0.f)
  {
    m_position += movement;

    float targetAngle = Utils::getDirectionAngle(movement);
    m_hullAngle = Utils::lerpAngle(m_hullAngle, targetAngle, m_rotationSpeed * dt);

    if (m_hull)
    {
      m_hull->move(movement);
      m_hull->setRotation(sf::degrees(m_hullAngle));
    }
  }

  // 炮塔朝向鼠标
  m_turretAngle = Utils::getAngle(m_position, mousePos);

  if (m_hull && m_turret)
  {
    m_turret->setPosition(m_hull->getPosition());
    m_turret->setRotation(sf::degrees(m_turretAngle));
  }
}

void Tank::draw(sf::RenderWindow &window) const
{
  if (m_hull && m_turret && !m_useSimpleGraphics)
  {
    window.draw(*m_hull);
    window.draw(*m_turret);
  }
  else
  {
    // 简易图形模式
    float size = 20.f * m_scale / 0.25f;

    // 车身
    sf::RectangleShape hull({size * 1.5f, size});
    hull.setOrigin({size * 0.75f, size * 0.5f});
    hull.setPosition(m_position);
    hull.setRotation(sf::degrees(m_hullAngle));
    hull.setFillColor(m_color);
    hull.setOutlineColor(sf::Color::Black);
    hull.setOutlineThickness(2.f);
    window.draw(hull);

    // 炮塔
    sf::CircleShape turretBase(size * 0.4f);
    turretBase.setOrigin({size * 0.4f, size * 0.4f});
    turretBase.setPosition(m_position);
    turretBase.setFillColor(sf::Color(m_color.r * 0.7f, m_color.g * 0.7f, m_color.b * 0.7f));
    window.draw(turretBase);

    // 炮管
    sf::RectangleShape barrel({size * 1.2f, size * 0.2f});
    barrel.setOrigin({0.f, size * 0.1f});
    barrel.setPosition(m_position);
    barrel.setRotation(sf::degrees(m_turretAngle - 90.f));
    barrel.setFillColor(sf::Color(80, 80, 80));
    window.draw(barrel);
  }
}

void Tank::drawUI(sf::RenderWindow &window) const
{
  m_healthBar.draw(window);
}

void Tank::setPosition(sf::Vector2f pos)
{
  m_position = pos;
  if (m_hull)
  {
    m_hull->setPosition(pos);
  }
  if (m_turret)
  {
    m_turret->setPosition(pos);
  }
}

sf::Vector2f Tank::getPosition() const
{
  return m_position;
}

float Tank::getTurretAngle() const
{
  return m_turretAngle;
}

sf::Vector2f Tank::getGunPosition() const
{
  float size = 20.f * m_scale / 0.25f;
  float angleRad = (m_turretAngle - 90.f) * Utils::PI / 180.f;
  sf::Vector2f offset = {std::cos(angleRad) * size * 1.2f,
                         std::sin(angleRad) * size * 1.2f};
  return m_position + offset;
}

bool Tank::hasFiredBullet()
{
  bool fired = m_firedBullet;
  m_firedBullet = false;
  return fired;
}

void Tank::takeDamage(float damage)
{
  m_healthBar.setHealth(m_healthBar.getHealth() - damage);
}

sf::Vector2f Tank::getMovement(float dt) const
{
  sf::Vector2f movement{0.f, 0.f};
  if (m_keyW)
    movement.y -= m_moveSpeed * dt;
  if (m_keyS)
    movement.y += m_moveSpeed * dt;
  if (m_keyA)
    movement.x -= m_moveSpeed * dt;
  if (m_keyD)
    movement.x += m_moveSpeed * dt;
  return movement;
}
