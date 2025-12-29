#pragma once

#include <SFML/Graphics.hpp>
#include <vector>
#include <string>
#include <queue>
#include <unordered_map>
#include "MazeGenerator.hpp"
#include "Utils.hpp"
#include "RoundedRectangle.hpp"

// 墙体类型
enum class WallType
{
  None,         // 空地
  Destructible, // 可破坏墙体
  Solid,        // 不可破坏墙体
  Exit          // 出口
};

// 可破坏墙体属性
enum class WallAttribute
{
  None, // 无属性（普通）
  Gold, // 金色 - 打掉获得2金币
  Heal  // 浅蓝色 - 恢复25%血量
};

// 墙体被摧毁时的结果
struct WallDestroyResult
{
  bool destroyed = false;
  WallAttribute attribute = WallAttribute::None;
  sf::Vector2f position = {0, 0};
  int gridX = 0;
  int gridY = 0;
};

// 圆角半径常量
constexpr float WALL_CORNER_RADIUS = 12.f;

// 单个墙体
struct Wall
{
  SelectiveRoundedRectShape shape;
  WallType type;
  WallAttribute attribute; // 墙体属性
  float health;
  float maxHealth;
  std::array<bool, 4> roundedCorners; // [左上, 右上, 右下, 左下]

  Wall() : shape({0, 0}, WALL_CORNER_RADIUS, 6), type(WallType::None), attribute(WallAttribute::None), health(0), maxHealth(0), roundedCorners({false, false, false, false}) {}
};

// 网格坐标
struct GridPos
{
  int x, y;
  bool operator==(const GridPos &other) const { return x == other.x && y == other.y; }
  bool operator!=(const GridPos &other) const { return !(*this == other); }
};

// GridPos 哈希函数
struct GridPosHash
{
  std::size_t operator()(const GridPos &pos) const
  {
    return std::hash<int>()(pos.x) ^ (std::hash<int>()(pos.y) << 16);
  }
};

class Maze
{
public:
  Maze();

  // 从字符串地图加载迷宫
  // '#' = 不可破坏墙
  // '*' = 可破坏墙
  // '.' = 空地
  // 'S' = 起点
  // 'E' = 出口
  // 'X' = 敌人位置
  void loadFromString(const std::vector<std::string> &map);

  // 生成随机迷宫（使用 MazeGenerator）
  void generateRandomMaze(int width, int height, unsigned int seed = 0, int enemyCount = 8, bool multiplayerMode = false, bool escapeMode = false);

  // 获取迷宫数据（用于网络传输）
  std::vector<std::string> getMazeData() const { return m_mazeData; }

  void update(float dt);
  void draw(sf::RenderWindow &window) const;
  void render(sf::RenderWindow &window) const { draw(window); } // 别名

  // 碰撞检测
  bool checkCollision(sf::Vector2f position, float radius) const;

  // 子弹与墙壁碰撞，返回是否击中，同时处理可破坏墙
  bool bulletHit(sf::Vector2f bulletPos, float damage);

  // 子弹与墙壁碰撞（带属性返回）
  WallDestroyResult bulletHitWithResult(sf::Vector2f bulletPos, float damage);

  // 获取起点位置
  sf::Vector2f getStartPosition() const { return m_startPosition; }
  sf::Vector2f getPlayerStartPosition() const { return m_startPosition; }

  // 获取出口位置
  sf::Vector2f getExitPosition() const { return m_exitPosition; }

  // 检查是否到达出口
  bool isAtExit(sf::Vector2f position, float radius) const;

  // 获取敌人生成点
  const std::vector<sf::Vector2f> &getEnemySpawnPoints() const { return m_enemySpawnPoints; }

  // 获取多人模式出生点
  sf::Vector2f getSpawn1Position() const { return m_spawn1Position; }
  sf::Vector2f getSpawn2Position() const { return m_spawn2Position; }

  // 获取迷宫尺寸（像素）
  sf::Vector2f getSize() const { return sf::Vector2f(m_cols * m_tileSize, m_rows * m_tileSize); }

  // 获取单元格大小
  float getTileSize() const { return m_tileSize; }

  // A* 寻路：返回从 start 到 target 的路径（世界坐标点列表）
  std::vector<sf::Vector2f> findPath(sf::Vector2f start, sf::Vector2f target) const;

  // A* 寻路（考虑可破坏墙）：将可破坏墙视为可通行但代价较高
  // 返回路径和路径上第一个可破坏墙的位置（如果有）
  struct PathResult
  {
    std::vector<sf::Vector2f> path;
    bool hasDestructibleWall = false;
    sf::Vector2f firstDestructibleWallPos = {0, 0};
    GridPos firstDestructibleWallGrid = {-1, -1};
  };
  PathResult findPathThroughDestructible(sf::Vector2f start, sf::Vector2f target, float destructibleCost = 3.0f) const;

  // 检查某个格子是否是可破坏墙
  bool isDestructibleWall(int row, int col) const;

  // 检查某个网格是否可通行
  bool isWalkable(int row, int col) const;

  // 网络同步：应用墙壁伤害（用于非房主接收同步数据）
  WallDestroyResult applyWallDamage(int row, int col, float damage, bool forceDestroy = false);

  // 世界坐标转网格坐标
  GridPos worldToGrid(sf::Vector2f pos) const;

  // 网格坐标转世界坐标（返回格子中心）
  sf::Vector2f gridToWorld(GridPos grid) const;

  // 视线检测：检查从 start 到 end 是否有清晰视线
  // 返回值：0 = 无阻挡, 1 = 有可拆墙阻挡, 2 = 有不可拆墙阻挡
  int checkLineOfSight(sf::Vector2f start, sf::Vector2f end) const;

  // 精确射击检测：检查子弹从 start 射向 target 是否能命中
  // 返回值：0 = 可以命中, 1 = 会先命中可破坏墙, 2 = 会先命中不可破坏墙
  // 这个函数使用更细的步进来模拟子弹轨迹
  int checkBulletPath(sf::Vector2f start, sf::Vector2f target) const;

  // 获取视线方向上第一个被阻挡的位置（用于判断是否应该攻击可拆墙）
  sf::Vector2f getFirstBlockedPosition(sf::Vector2f start, sf::Vector2f end) const;

  // 检查某个位置是否可以放置墙壁（空地且不在出口/起点）
  bool canPlaceWall(sf::Vector2f worldPos) const;

  // 在指定位置放置一个棕色墙壁
  bool placeWall(sf::Vector2f worldPos);

private:
  // 检查某个格子是否是墙（用于圆角计算）
  bool isWall(int row, int col) const;

  // 计算所有墙体的圆角
  void calculateRoundedCorners();

  std::vector<std::vector<Wall>> m_walls;
  std::vector<std::string> m_mazeData; // 保存原始迷宫数据用于网络传输
  sf::Vector2f m_startPosition;
  sf::Vector2f m_exitPosition;
  std::vector<sf::Vector2f> m_enemySpawnPoints;
  sf::Vector2f m_spawn1Position; // 多人模式出生点1
  sf::Vector2f m_spawn2Position; // 多人模式出生点2

  int m_rows = 0;
  int m_cols = 0;
  float m_tileSize = TILE_SIZE;

  // 颜色
  const sf::Color m_solidColor = sf::Color(80, 80, 80);
  const sf::Color m_destructibleColor = sf::Color(139, 90, 43);
  const sf::Color m_destructibleDamagedColor = sf::Color(100, 60, 30);
  const sf::Color m_exitColor = sf::Color(0, 200, 0, 180);
  // 属性墙颜色（纯色，无棕色）
  const sf::Color m_goldWallColor = sf::Color(255, 200, 50); // 明亮金色
  const sf::Color m_healWallColor = sf::Color(80, 180, 255); // 明亮蓝色
};
