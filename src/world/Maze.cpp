#include "Maze.hpp"
#include <cmath>
#include <cstdint>
#include <algorithm>

Maze::Maze()
{
}

void Maze::loadFromString(const std::vector<std::string> &map)
{
  if (map.empty())
    return;

  // 保存原始迷宫数据用于网络传输
  m_mazeData = map;

  m_rows = static_cast<int>(map.size());
  m_cols = 0;
  for (const auto &row : map)
  {
    m_cols = std::max(m_cols, static_cast<int>(row.size()));
  }

  m_walls.clear();
  m_walls.resize(m_rows, std::vector<Wall>(m_cols));
  m_enemySpawnPoints.clear();
  m_spawn1Position = {0.f, 0.f};
  m_spawn2Position = {0.f, 0.f};

  for (int r = 0; r < m_rows; ++r)
  {
    for (int c = 0; c < static_cast<int>(map[r].size()); ++c)
    {
      char ch = map[r][c];
      Wall &wall = m_walls[r][c];

      float x = c * m_tileSize;
      float y = r * m_tileSize;

      wall.shape.setSize({m_tileSize - 2.f, m_tileSize - 2.f});
      wall.shape.setCornerRadius(WALL_CORNER_RADIUS);
      wall.shape.setPosition({x + 1.f, y + 1.f});

      switch (ch)
      {
      case '#': // 不可破坏墙
        wall.type = WallType::Solid;
        wall.shape.setFillColor(m_solidColor);
        wall.shape.setOutlineColor(sf::Color(60, 60, 60));
        wall.shape.setOutlineThickness(1.f);
        break;

      case '*': // 可破坏墙（普通）
        wall.type = WallType::Destructible;
        wall.attribute = WallAttribute::None;
        wall.health = 100.f;
        wall.maxHealth = 100.f;
        wall.shape.setFillColor(m_destructibleColor);
        wall.shape.setOutlineColor(sf::Color(100, 60, 20));
        wall.shape.setOutlineThickness(1.f);
        break;

      case 'G': // 金色墙 - 打掉获得2金币
        wall.type = WallType::Destructible;
        wall.attribute = WallAttribute::Gold;
        wall.health = 100.f;
        wall.maxHealth = 100.f;
        wall.shape.setFillColor(m_goldWallColor);
        wall.shape.setOutlineColor(sf::Color(220, 170, 30)); // 金色边框
        wall.shape.setOutlineThickness(1.f);
        break;

      case 'H': // 治疗墙 - 恢复25%血量
        wall.type = WallType::Destructible;
        wall.attribute = WallAttribute::Heal;
        wall.health = 100.f;
        wall.maxHealth = 100.f;
        wall.shape.setFillColor(m_healWallColor);
        wall.shape.setOutlineColor(sf::Color(50, 140, 220)); // 蓝色边框
        wall.shape.setOutlineThickness(1.f);
        break;

      case 'S': // 起点
        wall.type = WallType::None;
        m_startPosition = {x + m_tileSize / 2.f, y + m_tileSize / 2.f};
        break;

      case 'E': // 出口
        wall.type = WallType::Exit;
        wall.shape.setFillColor(m_exitColor);
        m_exitPosition = {x + m_tileSize / 2.f, y + m_tileSize / 2.f};
        break;

      case 'X': // 敌人位置
        wall.type = WallType::None;
        m_enemySpawnPoints.push_back({x + m_tileSize / 2.f, y + m_tileSize / 2.f});
        break;

      case '1': // 多人模式出生点1
        wall.type = WallType::None;
        m_spawn1Position = {x + m_tileSize / 2.f, y + m_tileSize / 2.f};
        break;

      case '2': // 多人模式出生点2
        wall.type = WallType::None;
        m_spawn2Position = {x + m_tileSize / 2.f, y + m_tileSize / 2.f};
        break;

      default: // 空地
        wall.type = WallType::None;
        break;
      }
    }
  }

  // 计算每个墙体的圆角
  calculateRoundedCorners();
}

void Maze::generateRandomMaze(int width, int height, unsigned int seed, int enemyCount, bool multiplayerMode, bool escapeMode)
{
  MazeGenerator generator(width, height);
  if (seed != 0)
  {
    generator.setSeed(seed);
  }
  // 设置NPC数量
  generator.setEnemyCount(enemyCount);
  // 设置联机模式（联机模式下生成特殊方块）
  generator.setMultiplayerMode(multiplayerMode);
  // 设置 Escape 模式（只生成蓝色和棕色墙）
  generator.setEscapeMode(escapeMode);
  std::vector<std::string> mazeData = generator.generate();
  loadFromString(mazeData);
}

void Maze::update(float dt)
{
  (void)dt;
  // 更新可破坏墙的颜色（根据血量）
  for (int r = 0; r < m_rows; ++r)
  {
    for (int c = 0; c < m_cols; ++c)
    {
      Wall &wall = m_walls[r][c];
      if (wall.type == WallType::Destructible)
      {
        float healthRatio = wall.health / wall.maxHealth;
        sf::Color color;

        // 根据墙体属性选择对应的颜色插值
        switch (wall.attribute)
        {
        case WallAttribute::Gold:
        {
          // 金色墙：从深金色到亮金色
          sf::Color dark(180, 140, 30);
          color.r = static_cast<std::uint8_t>(dark.r + (m_goldWallColor.r - dark.r) * healthRatio);
          color.g = static_cast<std::uint8_t>(dark.g + (m_goldWallColor.g - dark.g) * healthRatio);
          color.b = static_cast<std::uint8_t>(dark.b + (m_goldWallColor.b - dark.b) * healthRatio);
          break;
        }
        case WallAttribute::Heal:
        {
          // 蓝色墙：从深蓝色到亮蓝色
          sf::Color dark(40, 100, 180);
          color.r = static_cast<std::uint8_t>(dark.r + (m_healWallColor.r - dark.r) * healthRatio);
          color.g = static_cast<std::uint8_t>(dark.g + (m_healWallColor.g - dark.g) * healthRatio);
          color.b = static_cast<std::uint8_t>(dark.b + (m_healWallColor.b - dark.b) * healthRatio);
          break;
        }
        default: // WallAttribute::None - 普通可破坏墙（棕色）
          color.r = static_cast<std::uint8_t>(m_destructibleDamagedColor.r +
                                              (m_destructibleColor.r - m_destructibleDamagedColor.r) * healthRatio);
          color.g = static_cast<std::uint8_t>(m_destructibleDamagedColor.g +
                                              (m_destructibleColor.g - m_destructibleDamagedColor.g) * healthRatio);
          color.b = static_cast<std::uint8_t>(m_destructibleDamagedColor.b +
                                              (m_destructibleColor.b - m_destructibleDamagedColor.b) * healthRatio);
          break;
        }

        wall.shape.setFillColor(color);
      }
    }
  }
}

void Maze::draw(sf::RenderWindow &window) const
{
  for (int r = 0; r < m_rows; ++r)
  {
    for (int c = 0; c < m_cols; ++c)
    {
      const Wall &wall = m_walls[r][c];
      if (wall.type != WallType::None)
      {
        window.draw(wall.shape);
      }
    }
  }
}

bool Maze::checkCollision(sf::Vector2f position, float radius) const
{
  // 检查位置周围的墙体
  int minC = std::max(0, static_cast<int>((position.x - radius) / m_tileSize));
  int maxC = std::min(m_cols - 1, static_cast<int>((position.x + radius) / m_tileSize));
  int minR = std::max(0, static_cast<int>((position.y - radius) / m_tileSize));
  int maxR = std::min(m_rows - 1, static_cast<int>((position.y + radius) / m_tileSize));

  for (int r = minR; r <= maxR; ++r)
  {
    for (int c = minC; c <= maxC; ++c)
    {
      const Wall &wall = m_walls[r][c];
      if (wall.type == WallType::Solid || wall.type == WallType::Destructible)
      {
        // 选择性圆角矩形与圆形碰撞检测
        float wallLeft = c * m_tileSize + 1.f; // 考虑1像素偏移
        float wallRight = wallLeft + m_tileSize - 2.f;
        float wallTop = r * m_tileSize + 1.f;
        float wallBottom = wallTop + m_tileSize - 2.f;

        float cornerRadius = WALL_CORNER_RADIUS;

        // 计算内部矩形边界（不包含圆角区域）
        float innerLeft = wallLeft + cornerRadius;
        float innerRight = wallRight - cornerRadius;
        float innerTop = wallTop + cornerRadius;
        float innerBottom = wallBottom - cornerRadius;

        // 判断位置在哪个角落区域
        bool inLeftZone = position.x < innerLeft;
        bool inRightZone = position.x > innerRight;
        bool inTopZone = position.y < innerTop;
        bool inBottomZone = position.y > innerBottom;

        // 确定是哪个角并检查该角是否有圆角
        int cornerIndex = -1; // 0=左上, 1=右上, 2=右下, 3=左下
        if (inLeftZone && inTopZone)
          cornerIndex = 0;
        else if (inRightZone && inTopZone)
          cornerIndex = 1;
        else if (inRightZone && inBottomZone)
          cornerIndex = 2;
        else if (inLeftZone && inBottomZone)
          cornerIndex = 3;

        if (cornerIndex >= 0 && wall.roundedCorners[cornerIndex])
        {
          // 这个角是圆角 - 使用圆形碰撞检测
          float cornerCenterX = inLeftZone ? innerLeft : innerRight;
          float cornerCenterY = inTopZone ? innerTop : innerBottom;

          float dx = position.x - cornerCenterX;
          float dy = position.y - cornerCenterY;
          float distSq = dx * dx + dy * dy;
          float combinedRadius = radius + cornerRadius;

          if (distSq < combinedRadius * combinedRadius)
          {
            return true;
          }
        }
        else
        {
          // 直角或边缘区域 - 普通矩形碰撞检测
          float closestX = std::max(wallLeft, std::min(position.x, wallRight));
          float closestY = std::max(wallTop, std::min(position.y, wallBottom));

          float dx = position.x - closestX;
          float dy = position.y - closestY;

          if (dx * dx + dy * dy < radius * radius)
          {
            return true;
          }
        }
      }
    }
  }
  return false;
}

bool Maze::bulletHit(sf::Vector2f bulletPos, float damage)
{
  int c = static_cast<int>(bulletPos.x / m_tileSize);
  int r = static_cast<int>(bulletPos.y / m_tileSize);

  if (r < 0 || r >= m_rows || c < 0 || c >= m_cols)
    return false;

  Wall &wall = m_walls[r][c];

  if (wall.type == WallType::Solid)
  {
    return true; // 击中不可破坏墙
  }
  else if (wall.type == WallType::Destructible)
  {
    wall.health -= damage;
    if (wall.health <= 0)
    {
      wall.type = WallType::None; // 墙被摧毁
    }
    return true;
  }

  return false;
}

WallDestroyResult Maze::bulletHitWithResult(sf::Vector2f bulletPos, float damage)
{
  WallDestroyResult result;

  int c = static_cast<int>(bulletPos.x / m_tileSize);
  int r = static_cast<int>(bulletPos.y / m_tileSize);

  if (r < 0 || r >= m_rows || c < 0 || c >= m_cols)
    return result;

  Wall &wall = m_walls[r][c];

  // 如果是不可破坏墙，视为命中但无摧毁效果
  if (wall.type == WallType::Solid)
  {
    result.destroyed = false;
    result.attribute = WallAttribute::None;
    result.position = {c * m_tileSize + m_tileSize / 2.f, r * m_tileSize + m_tileSize / 2.f};
    result.gridX = c;
    result.gridY = r;
    return result;
  }

  if (wall.type == WallType::Destructible)
  {
    wall.health -= damage;

    if (wall.health <= 0)
    {
      // 记录摧毁信息
      result.destroyed = true;
      result.attribute = wall.attribute;
      result.position = {c * m_tileSize + m_tileSize / 2.f, r * m_tileSize + m_tileSize / 2.f};
      result.gridX = c;
      result.gridY = r;

      // 清除当前墙格
      wall.type = WallType::None;
    }
    else
    {
      // 受伤但未摧毁
      result.destroyed = false;
      result.attribute = WallAttribute::None;
      result.position = {c * m_tileSize + m_tileSize / 2.f, r * m_tileSize + m_tileSize / 2.f};
      result.gridX = c;
      result.gridY = r;
    }

    return result;
  }

  // 非墙体
  return result;
}

WallDestroyResult Maze::applyWallDamage(int row, int col, float damage, bool forceDestroy)
{
  WallDestroyResult result;

  if (row < 0 || row >= m_rows || col < 0 || col >= m_cols)
    return result;

  Wall &wall = m_walls[row][col];

  if (wall.type == WallType::Destructible)
  {
    if (forceDestroy)
    {
      // 强制摧毁（用于同步已确定摧毁的墙）
      result.destroyed = true;
      result.attribute = wall.attribute;
      result.position = {col * m_tileSize + m_tileSize / 2.f, row * m_tileSize + m_tileSize / 2.f};
      result.gridX = col;
      result.gridY = row;
      wall.type = WallType::None;
    }
    else
    {
      wall.health -= damage;
      if (wall.health <= 0)
      {
        result.destroyed = true;
        result.attribute = wall.attribute;
        result.position = {col * m_tileSize + m_tileSize / 2.f, row * m_tileSize + m_tileSize / 2.f};
        result.gridX = col;
        result.gridY = row;
        wall.type = WallType::None;
      }
      else
      {
        result.destroyed = false;
        result.attribute = WallAttribute::None;
        result.position = {col * m_tileSize + m_tileSize / 2.f, row * m_tileSize + m_tileSize / 2.f};
        result.gridX = col;
        result.gridY = row;
      }
    }
  }

  return result;
}

bool Maze::isAtExit(sf::Vector2f position, float radius) const
{
  float dx = position.x - m_exitPosition.x;
  float dy = position.y - m_exitPosition.y;
  float dist = std::sqrt(dx * dx + dy * dy);
  return dist < radius + m_tileSize / 2.f;
}

bool Maze::isWalkable(int row, int col) const
{
  if (row < 0 || row >= m_rows || col < 0 || col >= m_cols)
    return false;
  WallType type = m_walls[row][col].type;
  return type == WallType::None || type == WallType::Exit;
}

bool Maze::canPlaceWall(sf::Vector2f worldPos) const
{
  int c = static_cast<int>(worldPos.x / m_tileSize);
  int r = static_cast<int>(worldPos.y / m_tileSize);

  // 检查边界
  if (r < 0 || r >= m_rows || c < 0 || c >= m_cols)
    return false;

  const Wall &wall = m_walls[r][c];

  // 只能在空地上放置
  if (wall.type != WallType::None)
    return false;

  // 不能放在起点
  sf::Vector2f cellCenter = {c * m_tileSize + m_tileSize / 2.f, r * m_tileSize + m_tileSize / 2.f};
  float distToStart = std::hypot(cellCenter.x - m_startPosition.x, cellCenter.y - m_startPosition.y);
  if (distToStart < m_tileSize)
    return false;

  // 不能放在出口
  float distToExit = std::hypot(cellCenter.x - m_exitPosition.x, cellCenter.y - m_exitPosition.y);
  if (distToExit < m_tileSize)
    return false;

  return true;
}

bool Maze::placeWall(sf::Vector2f worldPos)
{
  if (!canPlaceWall(worldPos))
    return false;

  int c = static_cast<int>(worldPos.x / m_tileSize);
  int r = static_cast<int>(worldPos.y / m_tileSize);

  Wall &wall = m_walls[r][c];

  // 设置为可破坏的棕色墙
  wall.type = WallType::Destructible;
  wall.attribute = WallAttribute::None;
  wall.health = 100.f;
  wall.maxHealth = 100.f;

  // 设置形状
  float x = c * m_tileSize;
  float y = r * m_tileSize;
  wall.shape.setSize({m_tileSize - 2.f, m_tileSize - 2.f});
  wall.shape.setCornerRadius(WALL_CORNER_RADIUS);
  wall.shape.setPosition({x + 1.f, y + 1.f});
  wall.shape.setFillColor(m_destructibleColor);
  wall.shape.setOutlineColor(sf::Color(100, 60, 20));
  wall.shape.setOutlineThickness(1.f);

  // 重新计算圆角
  calculateRoundedCorners();

  return true;
}

GridPos Maze::worldToGrid(sf::Vector2f pos) const
{
  return {static_cast<int>(pos.x / m_tileSize), static_cast<int>(pos.y / m_tileSize)};
}

sf::Vector2f Maze::gridToWorld(GridPos grid) const
{
  return {grid.x * m_tileSize + m_tileSize / 2.f, grid.y * m_tileSize + m_tileSize / 2.f};
}

std::vector<sf::Vector2f> Maze::findPath(sf::Vector2f start, sf::Vector2f target) const
{
  GridPos startGrid = worldToGrid(start);
  GridPos targetGrid = worldToGrid(target);

  // 如果起点或终点不可通行，返回空路径
  if (!isWalkable(startGrid.y, startGrid.x) || !isWalkable(targetGrid.y, targetGrid.x))
  {
    return {};
  }

  // A* 算法
  auto heuristic = [](GridPos a, GridPos b) -> float
  {
    return static_cast<float>(std::abs(a.x - b.x) + std::abs(a.y - b.y));
  };

  struct Node
  {
    GridPos pos;
    float gCost; // 从起点到当前节点的代价
    float fCost; // gCost + heuristic

    bool operator>(const Node &other) const { return fCost > other.fCost; }
  };

  std::priority_queue<Node, std::vector<Node>, std::greater<Node>> openSet;
  std::unordered_map<GridPos, GridPos, GridPosHash> cameFrom;
  std::unordered_map<GridPos, float, GridPosHash> gScore;

  openSet.push({startGrid, 0.f, heuristic(startGrid, targetGrid)});
  gScore[startGrid] = 0.f;

  // 四个方向
  const int dx[] = {0, 1, 0, -1};
  const int dy[] = {-1, 0, 1, 0};

  while (!openSet.empty())
  {
    Node current = openSet.top();
    openSet.pop();

    if (current.pos == targetGrid)
    {
      // 重建路径
      std::vector<sf::Vector2f> path;
      GridPos curr = targetGrid;
      while (curr != startGrid)
      {
        path.push_back(gridToWorld(curr));
        curr = cameFrom[curr];
      }
      std::reverse(path.begin(), path.end());
      return path;
    }

    // 检查是否已经有更好的路径到达这个节点
    if (gScore.count(current.pos) && current.gCost > gScore[current.pos])
    {
      continue;
    }

    for (int i = 0; i < 4; ++i)
    {
      GridPos neighbor = {current.pos.x + dx[i], current.pos.y + dy[i]};

      if (!isWalkable(neighbor.y, neighbor.x))
        continue;

      float tentativeG = current.gCost + 1.f;

      if (!gScore.count(neighbor) || tentativeG < gScore[neighbor])
      {
        cameFrom[neighbor] = current.pos;
        gScore[neighbor] = tentativeG;
        float fCost = tentativeG + heuristic(neighbor, targetGrid);
        openSet.push({neighbor, tentativeG, fCost});
      }
    }
  }

  // 没有找到路径
  return {};
}

bool Maze::isDestructibleWall(int row, int col) const
{
  if (row < 0 || row >= m_rows || col < 0 || col >= m_cols)
    return false;
  return m_walls[row][col].type == WallType::Destructible;
}

Maze::PathResult Maze::findPathThroughDestructible(sf::Vector2f start, sf::Vector2f target, float destructibleCost) const
{
  PathResult result;

  GridPos startGrid = worldToGrid(start);
  GridPos targetGrid = worldToGrid(target);

  // 检查起点和终点是否有效（起点必须可通行，终点可以是空地或可破坏墙）
  if (!isWalkable(startGrid.y, startGrid.x))
  {
    return result;
  }

  // 终点如果是不可破坏墙则无法到达
  if (targetGrid.y >= 0 && targetGrid.y < m_rows && targetGrid.x >= 0 && targetGrid.x < m_cols)
  {
    if (m_walls[targetGrid.y][targetGrid.x].type == WallType::Solid)
    {
      return result;
    }
  }

  // A* 算法（将可破坏墙视为高代价但可通行）
  auto heuristic = [](GridPos a, GridPos b) -> float
  {
    return static_cast<float>(std::abs(a.x - b.x) + std::abs(a.y - b.y));
  };

  struct Node
  {
    GridPos pos;
    float gCost;
    float fCost;

    bool operator>(const Node &other) const { return fCost > other.fCost; }
  };

  std::priority_queue<Node, std::vector<Node>, std::greater<Node>> openSet;
  std::unordered_map<GridPos, GridPos, GridPosHash> cameFrom;
  std::unordered_map<GridPos, float, GridPosHash> gScore;

  openSet.push({startGrid, 0.f, heuristic(startGrid, targetGrid)});
  gScore[startGrid] = 0.f;

  const int dx[] = {0, 1, 0, -1};
  const int dy[] = {-1, 0, 1, 0};

  while (!openSet.empty())
  {
    Node current = openSet.top();
    openSet.pop();

    if (current.pos == targetGrid)
    {
      // 重建路径并查找第一个可破坏墙
      std::vector<sf::Vector2f> path;
      GridPos curr = targetGrid;

      // 先收集所有路径点（从终点到起点）
      std::vector<GridPos> gridPath;
      while (curr != startGrid)
      {
        gridPath.push_back(curr);
        curr = cameFrom[curr];
      }

      // 反转得到从起点到终点的顺序
      std::reverse(gridPath.begin(), gridPath.end());

      // 转换为世界坐标并查找第一个可破坏墙
      for (const auto &gridPos : gridPath)
      {
        path.push_back(gridToWorld(gridPos));

        // 检查是否是可破坏墙（且还没找到第一个）
        if (!result.hasDestructibleWall && isDestructibleWall(gridPos.y, gridPos.x))
        {
          result.hasDestructibleWall = true;
          result.firstDestructibleWallPos = gridToWorld(gridPos);
          result.firstDestructibleWallGrid = gridPos;
        }
      }

      result.path = path;
      return result;
    }

    if (gScore.count(current.pos) && current.gCost > gScore[current.pos])
    {
      continue;
    }

    for (int i = 0; i < 4; ++i)
    {
      GridPos neighbor = {current.pos.x + dx[i], current.pos.y + dy[i]};

      // 边界检查
      if (neighbor.y < 0 || neighbor.y >= m_rows || neighbor.x < 0 || neighbor.x >= m_cols)
        continue;

      WallType neighborType = m_walls[neighbor.y][neighbor.x].type;

      // 不可破坏墙不能通过
      if (neighborType == WallType::Solid)
        continue;

      // 计算移动代价：空地/出口=1，可破坏墙=destructibleCost
      float moveCost = 1.f;
      if (neighborType == WallType::Destructible)
      {
        moveCost = destructibleCost;
      }

      float tentativeG = current.gCost + moveCost;

      if (!gScore.count(neighbor) || tentativeG < gScore[neighbor])
      {
        cameFrom[neighbor] = current.pos;
        gScore[neighbor] = tentativeG;
        float fCost = tentativeG + heuristic(neighbor, targetGrid);
        openSet.push({neighbor, tentativeG, fCost});
      }
    }
  }

  return result; // 空路径
}

int Maze::checkLineOfSight(sf::Vector2f start, sf::Vector2f end) const
{
  // 使用 Bresenham 线段算法检查视线
  // 返回值：0 = 无阻挡, 1 = 有可拆墙阻挡, 2 = 有不可拆墙阻挡

  GridPos startGrid = worldToGrid(start);
  GridPos endGrid = worldToGrid(end);

  int x0 = startGrid.x, y0 = startGrid.y;
  int x1 = endGrid.x, y1 = endGrid.y;

  int dx = std::abs(x1 - x0);
  int dy = std::abs(y1 - y0);
  int sx = x0 < x1 ? 1 : -1;
  int sy = y0 < y1 ? 1 : -1;
  int err = dx - dy;

  int result = 0; // 无阻挡

  while (true)
  {
    // 检查当前格子
    if (y0 >= 0 && y0 < m_rows && x0 >= 0 && x0 < m_cols)
    {
      WallType type = m_walls[y0][x0].type;
      if (type == WallType::Solid)
      {
        return 2; // 不可拆墙阻挡
      }
      else if (type == WallType::Destructible)
      {
        result = 1; // 可拆墙阻挡（继续检查是否有不可拆墙）
      }
    }

    if (x0 == x1 && y0 == y1)
      break;

    int e2 = 2 * err;
    if (e2 > -dy)
    {
      err -= dy;
      x0 += sx;
    }
    if (e2 < dx)
    {
      err += dx;
      y0 += sy;
    }
  }

  return result;
}

int Maze::checkBulletPath(sf::Vector2f start, sf::Vector2f target) const
{
  // 使用射线步进法更精确地检测子弹路径
  // 每次步进一小段距离，检查是否碰到墙壁

  sf::Vector2f direction = target - start;
  float distance = std::sqrt(direction.x * direction.x + direction.y * direction.y);

  if (distance < 1.f)
    return 0; // 起点和终点太近

  // 归一化方向
  direction /= distance;

  // 步进大小（小于格子大小的一半，确保不会跳过墙壁）
  const float stepSize = m_tileSize * 0.05f;
  int steps = static_cast<int>(distance / stepSize) + 1;

  int result = 0; // 默认无阻挡

  for (int i = 1; i <= steps; ++i)
  {
    float t = std::min(static_cast<float>(i) * stepSize, distance);
    sf::Vector2f checkPos = start + direction * t;

    GridPos grid = worldToGrid(checkPos);

    // 边界检查
    if (grid.y < 0 || grid.y >= m_rows || grid.x < 0 || grid.x >= m_cols)
      continue;

    WallType type = m_walls[grid.y][grid.x].type;

    if (type == WallType::Solid)
    {
      return 2; // 会先命中不可破坏墙
    }
    else if (type == WallType::Destructible)
    {
      if (result == 0)
      {
        result = 1; // 记录会命中可破坏墙，但继续检查后面是否有不可破坏墙
      }
      // 一旦碰到可破坏墙，子弹就会停止，所以后面的墙不会被击中
      return result;
    }

    // 检查是否已经到达目标附近
    float remainDist = std::sqrt((target.x - checkPos.x) * (target.x - checkPos.x) +
                                 (target.y - checkPos.y) * (target.y - checkPos.y));
    if (remainDist < stepSize)
      break;
  }

  return result;
}

sf::Vector2f Maze::getFirstBlockedPosition(sf::Vector2f start, sf::Vector2f end) const
{
  // 找到视线上第一个被阻挡的位置
  GridPos startGrid = worldToGrid(start);
  GridPos endGrid = worldToGrid(end);

  int x0 = startGrid.x, y0 = startGrid.y;
  int x1 = endGrid.x, y1 = endGrid.y;

  int dx = std::abs(x1 - x0);
  int dy = std::abs(y1 - y0);
  int sx = x0 < x1 ? 1 : -1;
  int sy = y0 < y1 ? 1 : -1;
  int err = dx - dy;

  while (true)
  {
    // 检查当前格子
    if (y0 >= 0 && y0 < m_rows && x0 >= 0 && x0 < m_cols)
    {
      WallType type = m_walls[y0][x0].type;
      if (type == WallType::Solid || type == WallType::Destructible)
      {
        return gridToWorld({x0, y0});
      }
    }

    if (x0 == x1 && y0 == y1)
      break;

    int e2 = 2 * err;
    if (e2 > -dy)
    {
      err -= dy;
      x0 += sx;
    }
    if (e2 < dx)
    {
      err += dx;
      y0 += sy;
    }
  }

  return end; // 没有阻挡，返回目标位置
}

bool Maze::isWall(int row, int col) const
{
  if (row < 0 || row >= m_rows || col < 0 || col >= m_cols)
    return true; // 边界外视为墙

  WallType type = m_walls[row][col].type;
  return type == WallType::Solid || type == WallType::Destructible;
}

void Maze::calculateRoundedCorners()
{
  // 对于每个墙体，检查其四个角是否需要圆角
  // 规则：如果某个角的两个相邻方向都没有墙，则该角需要圆角
  // 例如：左上角需要圆角的条件是：左边没有墙 AND 上边没有墙

  for (int r = 0; r < m_rows; ++r)
  {
    for (int c = 0; c < m_cols; ++c)
    {
      Wall &wall = m_walls[r][c];

      // 只处理墙体
      if (wall.type != WallType::Solid && wall.type != WallType::Destructible && wall.type != WallType::Exit)
        continue;

      // 检查四个方向的邻居
      bool hasTop = isWall(r - 1, c);
      bool hasBottom = isWall(r + 1, c);
      bool hasLeft = isWall(r, c - 1);
      bool hasRight = isWall(r, c + 1);

      // 还需要检查对角线邻居（用于更精确的判断）
      bool hasTopLeft = isWall(r - 1, c - 1);
      bool hasTopRight = isWall(r - 1, c + 1);
      bool hasBottomLeft = isWall(r + 1, c - 1);
      bool hasBottomRight = isWall(r + 1, c + 1);

      // 计算每个角是否需要圆角
      // 左上角：如果左边和上边都没有墙，则需要圆角
      bool roundTopLeft = !hasTop && !hasLeft;
      // 右上角：如果右边和上边都没有墙，则需要圆角
      bool roundTopRight = !hasTop && !hasRight;
      // 右下角：如果右边和下边都没有墙，则需要圆角
      bool roundBottomRight = !hasBottom && !hasRight;
      // 左下角：如果左边和下边都没有墙，则需要圆角
      bool roundBottomLeft = !hasBottom && !hasLeft;

      // 设置圆角
      wall.roundedCorners = {roundTopLeft, roundTopRight, roundBottomRight, roundBottomLeft};
      wall.shape.setRoundedCorners(roundTopLeft, roundTopRight, roundBottomRight, roundBottomLeft);
    }
  }
}
