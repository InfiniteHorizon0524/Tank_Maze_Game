#pragma once

#include <SFML/Graphics.hpp>
#include <cmath>
#include <string>

#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#endif

// 全局常量
constexpr float TILE_SIZE = 60.f;

// 获取资源路径（macOS app bundle 支持）
inline std::string getResourcePath()
{
#ifdef __APPLE__
  CFBundleRef mainBundle = CFBundleGetMainBundle();
  if (mainBundle)
  {
    CFURLRef resourcesURL = CFBundleCopyResourcesDirectoryURL(mainBundle);
    if (resourcesURL)
    {
      char path[PATH_MAX];
      if (CFURLGetFileSystemRepresentation(resourcesURL, TRUE, (UInt8 *)path, PATH_MAX))
      {
        CFRelease(resourcesURL);
        return std::string(path) + "/";
      }
      CFRelease(resourcesURL);
    }
  }
#endif
  // 非 macOS 或获取失败时使用当前目录
  return "";
}

// 游戏颜色常量
namespace GameColors
{
  // 子弹颜色
  const sf::Color PlayerBullet = sf::Color::Yellow;         // 己方玩家子弹
  const sf::Color EnemyPlayerBullet = sf::Color::Magenta;   // 敌方玩家子弹（联机）
  const sf::Color AllyNpcBullet = sf::Color(100, 180, 255); // 己方NPC子弹（浅蓝）
  const sf::Color EnemyNpcBullet = sf::Color::Red;          // 敌方NPC子弹

  // 小地图颜色
  const sf::Color MinimapPlayer = sf::Color::Yellow;             // 己方玩家
  const sf::Color MinimapAlly = sf::Color::Cyan;                 // 队友（Escape模式）
  const sf::Color MinimapEnemy = sf::Color::Magenta;             // 敌方玩家（Battle模式）
  const sf::Color MinimapAllyNpc = sf::Color(100, 180, 255);     // 己方NPC（浅蓝）
  const sf::Color MinimapEnemyNpc = sf::Color::Red;              // 敌方NPC
  const sf::Color MinimapInactiveNpc = sf::Color(128, 128, 128); // 未激活NPC
  const sf::Color MinimapDowned = sf::Color(100, 100, 100);      // 倒地状态
}

namespace Utils
{
  constexpr float PI = 3.14159265f;

  // 计算从 from 到 to 的角度（度）
  inline float getAngle(sf::Vector2f from, sf::Vector2f to)
  {
    sf::Vector2f diff = to - from;
    return std::atan2(diff.y, diff.x) * 180.f / PI + 90.f;
  }

  // 计算向量方向的角度（度）
  inline float getDirectionAngle(sf::Vector2f dir)
  {
    return std::atan2(dir.y, dir.x) * 180.f / PI + 90.f;
  }

  // 平滑插值角度（处理 -180 到 180 的跨越）
  inline float lerpAngle(float current, float target, float t)
  {
    float diff = target - current;

    while (diff > 180.f)
      diff -= 360.f;
    while (diff < -180.f)
      diff += 360.f;

    return current + diff * t;
  }
}
