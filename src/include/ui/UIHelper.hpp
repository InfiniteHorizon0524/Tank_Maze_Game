#pragma once

#include <SFML/Graphics.hpp>
#include <string>

// UI绘制辅助函数
namespace UIHelper
{
  // 绘制居中文字
  inline void drawCenteredText(
      sf::RenderWindow &window,
      sf::Font &font,
      const std::string &text,
      unsigned int size,
      sf::Color color,
      float y,
      float screenWidth)
  {
    sf::Text textObj(font);
    textObj.setString(text);
    textObj.setCharacterSize(size);
    textObj.setFillColor(color);
    sf::FloatRect bounds = textObj.getLocalBounds();
    textObj.setPosition({(screenWidth - bounds.size.x) / 2.f, y});
    window.draw(textObj);
  }

  // 绘制带边框的血条
  inline void drawHealthBar(
      sf::RenderWindow &window,
      float x, float y,
      float width, float height,
      float healthPercent,
      sf::Color fillColor,
      sf::Color bgColor = sf::Color(60, 60, 60),
      sf::Color outlineColor = sf::Color::White,
      float outlineThickness = 2.f)
  {
    // 背景
    sf::RectangleShape bgBar({width, height});
    bgBar.setPosition({x, y});
    bgBar.setFillColor(bgColor);
    bgBar.setOutlineColor(outlineColor);
    bgBar.setOutlineThickness(outlineThickness);
    window.draw(bgBar);

    // 血条
    sf::RectangleShape bar({width * healthPercent, height});
    bar.setPosition({x, y});
    bar.setFillColor(fillColor);
    window.draw(bar);
  }

  // 绘制输入框
  inline void drawInputBox(
      sf::RenderWindow &window,
      sf::Font &font,
      const std::string &inputText,
      float x, float y,
      float width, float height,
      sf::Color bgColor = sf::Color(50, 50, 70),
      sf::Color textColor = sf::Color::White)
  {
    sf::RectangleShape inputBox({width, height});
    inputBox.setFillColor(bgColor);
    inputBox.setOutlineColor(sf::Color::White);
    inputBox.setOutlineThickness(2.f);
    inputBox.setPosition({x, y});
    window.draw(inputBox);

    sf::Text text(font);
    text.setString(inputText + "_");
    text.setCharacterSize(28);
    text.setFillColor(textColor);
    text.setPosition({x + 10.f, y + 8.f});
    window.draw(text);
  }

  // 绘制团队标记圆圈
  inline void drawTeamMarker(
      sf::RenderWindow &window,
      sf::Vector2f position,
      float radius,
      sf::Color color)
  {
    sf::CircleShape marker(radius);
    marker.setFillColor(color);
    marker.setPosition({position.x - radius, position.y - radius});
    window.draw(marker);
  }
}
