#pragma once

#include <SFML/Graphics.hpp>
#include <cmath>
#include <array>

// 可选择性圆角的矩形形状类
// 每个角可以独立设置是否为圆角
class SelectiveRoundedRectShape : public sf::Shape
{
public:
  explicit SelectiveRoundedRectShape(sf::Vector2f size = {0, 0}, float radius = 0, unsigned int cornerPoints = 6)
      : m_size(size), m_radius(radius), m_cornerPoints(cornerPoints)
  {
    // 默认所有角都不是圆角
    m_roundedCorners = {false, false, false, false};
    update();
  }

  void setSize(sf::Vector2f size)
  {
    m_size = size;
    update();
  }

  sf::Vector2f getSize() const
  {
    return m_size;
  }

  void setCornerRadius(float radius)
  {
    m_radius = radius;
    update();
  }

  float getCornerRadius() const
  {
    return m_radius;
  }

  // 设置哪些角是圆角
  // corners: [左上, 右上, 右下, 左下]
  void setRoundedCorners(bool topLeft, bool topRight, bool bottomRight, bool bottomLeft)
  {
    m_roundedCorners = {topLeft, topRight, bottomRight, bottomLeft};
    update();
  }

  void setRoundedCorners(const std::array<bool, 4> &corners)
  {
    m_roundedCorners = corners;
    update();
  }

  std::array<bool, 4> getRoundedCorners() const
  {
    return m_roundedCorners;
  }

  void setCornerPointCount(unsigned int count)
  {
    m_cornerPoints = count;
    update();
  }

  std::size_t getPointCount() const override
  {
    // 计算总点数：每个圆角有 m_cornerPoints 个点，直角只有1个点
    std::size_t count = 0;
    for (int i = 0; i < 4; ++i)
    {
      count += m_roundedCorners[i] ? m_cornerPoints : 1;
    }
    return count;
  }

  sf::Vector2f getPoint(std::size_t index) const override
  {
    float radius = std::min(m_radius, std::min(m_size.x / 2.f, m_size.y / 2.f));

    // 计算点属于哪个角，以及在该角中的位置
    std::size_t currentIndex = 0;
    for (int corner = 0; corner < 4; ++corner)
    {
      std::size_t pointsInCorner = m_roundedCorners[corner] ? m_cornerPoints : 1;

      if (index < currentIndex + pointsInCorner)
      {
        std::size_t pointInCorner = index - currentIndex;
        return getCornerPoint(corner, pointInCorner, radius);
      }
      currentIndex += pointsInCorner;
    }

    return {0, 0};
  }

private:
  sf::Vector2f getCornerPoint(int corner, std::size_t pointInCorner, float radius) const
  {
    // 四个角的位置
    // 0: 左上, 1: 右上, 2: 右下, 3: 左下

    if (!m_roundedCorners[corner])
    {
      // 直角 - 返回角落点
      switch (corner)
      {
      case 0:
        return {0, 0};
      case 1:
        return {m_size.x, 0};
      case 2:
        return {m_size.x, m_size.y};
      case 3:
        return {0, m_size.y};
      }
      return {0, 0};
    }

    // 圆角 - 计算圆弧上的点
    float angleStep = (m_cornerPoints > 1) ? (90.f / (m_cornerPoints - 1)) : 0;
    float localAngle = pointInCorner * angleStep;

    sf::Vector2f center;
    float startAngle;

    switch (corner)
    {
    case 0: // 左上角
      center = {radius, radius};
      startAngle = 180.f;
      break;
    case 1: // 右上角
      center = {m_size.x - radius, radius};
      startAngle = 270.f;
      break;
    case 2: // 右下角
      center = {m_size.x - radius, m_size.y - radius};
      startAngle = 0.f;
      break;
    case 3: // 左下角
      center = {radius, m_size.y - radius};
      startAngle = 90.f;
      break;
    default:
      center = {0, 0};
      startAngle = 0;
    }

    float angle = (startAngle + localAngle) * 3.14159265f / 180.f;
    return {center.x + radius * std::cos(angle), center.y + radius * std::sin(angle)};
  }

  sf::Vector2f m_size;
  float m_radius;
  unsigned int m_cornerPoints;
  std::array<bool, 4> m_roundedCorners; // [左上, 右上, 右下, 左下]
};
