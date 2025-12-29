#pragma once

#include <SFML/Graphics.hpp>

class HealthBar
{
public:
  HealthBar(float width, float height);

  void setMaxHealth(float maxHealth);
  void setHealth(float health);
  void setPosition(sf::Vector2f position);

  float getHealth() const { return m_health; }
  float getMaxHealth() const { return m_maxHealth; }
  bool isDead() const { return m_health <= 0; }

  void draw(sf::RenderWindow &window) const;

private:
  void updateBar();

  sf::RectangleShape m_background;
  sf::RectangleShape m_foreground;

  float m_width;
  float m_height;
  float m_maxHealth = 100.f;
  float m_health = 100.f;
};
