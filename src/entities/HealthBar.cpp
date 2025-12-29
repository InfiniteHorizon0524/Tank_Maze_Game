#include "HealthBar.hpp"

HealthBar::HealthBar(float width, float height)
    : m_width(width), m_height(height)
{
  m_background.setSize({width, height});
  m_background.setFillColor(sf::Color(50, 50, 50));
  m_background.setOutlineColor(sf::Color::White);
  m_background.setOutlineThickness(1.f);

  m_foreground.setSize({width, height});
  m_foreground.setFillColor(sf::Color::Green);
}

void HealthBar::setMaxHealth(float maxHealth)
{
  m_maxHealth = maxHealth;
  updateBar();
}

void HealthBar::setHealth(float health)
{
  m_health = std::max(0.f, std::min(health, m_maxHealth));
  updateBar();
}

void HealthBar::setPosition(sf::Vector2f position)
{
  m_background.setPosition(position);
  m_foreground.setPosition(position);
}

void HealthBar::updateBar()
{
  float ratio = m_health / m_maxHealth;
  m_foreground.setSize({m_width * ratio, m_height});

  // 根据血量改变颜色
  if (ratio > 0.6f)
  {
    m_foreground.setFillColor(sf::Color::Green);
  }
  else if (ratio > 0.3f)
  {
    m_foreground.setFillColor(sf::Color::Yellow);
  }
  else
  {
    m_foreground.setFillColor(sf::Color::Red);
  }
}

void HealthBar::draw(sf::RenderWindow &window) const
{
  window.draw(m_background);
  window.draw(m_foreground);
}
