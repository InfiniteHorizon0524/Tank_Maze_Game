#include "NetworkManager.hpp"
#include <iostream>
#include <cstring>

NetworkManager &NetworkManager::getInstance()
{
  static NetworkManager instance;
  return instance;
}

bool NetworkManager::connect(const std::string &host, unsigned short port)
{
  m_socket.setBlocking(true);
  auto address = sf::IpAddress::resolve(host);
  if (!address.has_value())
  {
    if (m_onError)
    {
      m_onError("Invalid IP address");
    }
    return false;
  }

  sf::Socket::Status status = m_socket.connect(address.value(), port, sf::seconds(5));
  m_socket.setBlocking(false);

  if (status != sf::Socket::Status::Done)
  {
    if (m_onError)
    {
      m_onError("Failed to connect to server");
    }
    return false;
  }

  m_connected = true;

  // 发送连接消息
  std::vector<uint8_t> data;
  data.push_back(static_cast<uint8_t>(NetMessageType::Connect));
  sendPacket(data);

  if (m_onConnected)
  {
    m_onConnected();
  }

  return true;
}

void NetworkManager::disconnect()
{
  if (m_connected)
  {
    std::vector<uint8_t> data;
    data.push_back(static_cast<uint8_t>(NetMessageType::Disconnect));
    sendPacket(data);
  }

  m_socket.disconnect();
  m_connected = false;
  m_roomCode.clear();
  m_receiveBuffer.clear();

  if (m_onDisconnected)
  {
    m_onDisconnected();
  }
}

void NetworkManager::createRoom(int mazeWidth, int mazeHeight, bool isDarkMode)
{
  if (!m_connected)
    return;

  std::vector<uint8_t> data;
  data.push_back(static_cast<uint8_t>(NetMessageType::CreateRoom));

  // 添加迷宫尺寸
  data.push_back(static_cast<uint8_t>(mazeWidth & 0xFF));
  data.push_back(static_cast<uint8_t>((mazeWidth >> 8) & 0xFF));
  data.push_back(static_cast<uint8_t>(mazeHeight & 0xFF));
  data.push_back(static_cast<uint8_t>((mazeHeight >> 8) & 0xFF));

  // 添加暗黑模式标志
  data.push_back(static_cast<uint8_t>(isDarkMode ? 1 : 0));

  sendPacket(data);
}

void NetworkManager::joinRoom(const std::string &roomCode)
{
  if (!m_connected)
    return;

  std::vector<uint8_t> data;
  data.push_back(static_cast<uint8_t>(NetMessageType::JoinRoom));
  data.push_back(static_cast<uint8_t>(roomCode.length()));
  for (char c : roomCode)
  {
    data.push_back(static_cast<uint8_t>(c));
  }

  sendPacket(data);
}

void NetworkManager::sendPosition(const PlayerState &state)
{
  if (!m_connected)
    return;

  std::vector<uint8_t> data;
  data.push_back(static_cast<uint8_t>(NetMessageType::PlayerUpdate));

  // 添加位置数据 (float -> bytes)
  auto addFloat = [&data](float value)
  {
    uint8_t *bytes = reinterpret_cast<uint8_t *>(&value);
    for (int i = 0; i < 4; i++)
      data.push_back(bytes[i]);
  };

  addFloat(state.x);
  addFloat(state.y);
  addFloat(state.rotation);
  addFloat(state.turretAngle);
  addFloat(state.health);
  data.push_back(state.reachedExit ? 1 : 0);
  data.push_back(state.isDead ? 1 : 0); // 添加死亡状态

  sendPacket(data);
}

void NetworkManager::sendShoot(float x, float y, float angle)
{
  if (!m_connected)
    return;

  std::vector<uint8_t> data;
  data.push_back(static_cast<uint8_t>(NetMessageType::PlayerShoot));

  auto addFloat = [&data](float value)
  {
    uint8_t *bytes = reinterpret_cast<uint8_t *>(&value);
    for (int i = 0; i < 4; i++)
      data.push_back(bytes[i]);
  };

  addFloat(x);
  addFloat(y);
  addFloat(angle);

  sendPacket(data);
}

void NetworkManager::sendReachExit()
{
  if (!m_connected)
    return;

  std::vector<uint8_t> data;
  data.push_back(static_cast<uint8_t>(NetMessageType::ReachExit));
  sendPacket(data);
}

void NetworkManager::sendGameResult(bool localWin)
{
  if (!m_connected)
    return;

  std::vector<uint8_t> data;
  data.push_back(static_cast<uint8_t>(NetMessageType::GameResult));
  data.push_back(localWin ? 1 : 0); // 我赢了
  sendPacket(data);
}

void NetworkManager::sendRestartRequest()
{
  if (!m_connected)
    return;

  std::vector<uint8_t> data;
  data.push_back(static_cast<uint8_t>(NetMessageType::RestartRequest));
  sendPacket(data);
}

void NetworkManager::sendClimaxStart()
{
  if (!m_connected)
    return;

  std::vector<uint8_t> data;
  data.push_back(static_cast<uint8_t>(NetMessageType::ClimaxStart));
  sendPacket(data);
}

void NetworkManager::sendWallPlace(float x, float y)
{
  if (!m_connected)
    return;

  std::vector<uint8_t> data;
  data.push_back(static_cast<uint8_t>(NetMessageType::WallPlace));

  auto addFloat = [&data](float value)
  {
    uint8_t *bytes = reinterpret_cast<uint8_t *>(&value);
    for (int i = 0; i < 4; i++)
      data.push_back(bytes[i]);
  };

  addFloat(x);
  addFloat(y);

  sendPacket(data);
}

void NetworkManager::sendWallDamage(int row, int col, float damage, bool destroyed, int attribute, int destroyerId)
{
  if (!m_connected)
    return;

  std::vector<uint8_t> data;
  data.push_back(static_cast<uint8_t>(NetMessageType::WallDamage));

  auto addInt = [&data](int value)
  {
    uint8_t *bytes = reinterpret_cast<uint8_t *>(&value);
    for (int i = 0; i < 4; i++)
      data.push_back(bytes[i]);
  };

  auto addFloat = [&data](float value)
  {
    uint8_t *bytes = reinterpret_cast<uint8_t *>(&value);
    for (int i = 0; i < 4; i++)
      data.push_back(bytes[i]);
  };

  addInt(row);
  addInt(col);
  addFloat(damage);
  data.push_back(destroyed ? 1 : 0);
  addInt(attribute);
  addInt(destroyerId);

  sendPacket(data);
}

void NetworkManager::sendRescueStart()
{
  if (!m_connected)
    return;

  std::vector<uint8_t> data;
  data.push_back(static_cast<uint8_t>(NetMessageType::RescueStart));
  sendPacket(data);
}

void NetworkManager::sendRescueProgress(float progress)
{
  if (!m_connected)
    return;

  std::vector<uint8_t> data;
  data.push_back(static_cast<uint8_t>(NetMessageType::RescueProgress));

  uint8_t *bytes = reinterpret_cast<uint8_t *>(&progress);
  for (int i = 0; i < 4; i++)
    data.push_back(bytes[i]);

  sendPacket(data);
}

void NetworkManager::sendRescueComplete()
{
  if (!m_connected)
    return;

  std::vector<uint8_t> data;
  data.push_back(static_cast<uint8_t>(NetMessageType::RescueComplete));
  sendPacket(data);
}

void NetworkManager::sendRescueCancel()
{
  if (!m_connected)
    return;

  std::vector<uint8_t> data;
  data.push_back(static_cast<uint8_t>(NetMessageType::RescueCancel));
  sendPacket(data);
}

void NetworkManager::sendPlayerReady(bool isReady)
{
  if (!m_connected)
    return;

  std::vector<uint8_t> data;
  data.push_back(static_cast<uint8_t>(NetMessageType::PlayerReady));
  data.push_back(isReady ? 1 : 0);
  sendPacket(data);
}

void NetworkManager::sendHostStartGame()
{
  if (!m_connected)
    return;

  std::vector<uint8_t> data;
  data.push_back(static_cast<uint8_t>(NetMessageType::HostStartGame));
  sendPacket(data);
}

void NetworkManager::sendNpcActivate(int npcId, int team, int activatorId)
{
  if (!m_connected)
    return;

  std::vector<uint8_t> data;
  data.push_back(static_cast<uint8_t>(NetMessageType::NpcActivate));
  data.push_back(static_cast<uint8_t>(npcId));
  data.push_back(static_cast<uint8_t>(team));
  data.push_back(static_cast<uint8_t>(activatorId + 128)); // +128 以支持负数
  sendPacket(data);
}

void NetworkManager::sendNpcUpdate(const NpcState &state)
{
  if (!m_connected)
    return;

  std::vector<uint8_t> data;
  data.push_back(static_cast<uint8_t>(NetMessageType::NpcUpdate));
  data.push_back(static_cast<uint8_t>(state.id));

  // 位置
  auto pushFloat = [&data](float f)
  {
    uint32_t bits;
    std::memcpy(&bits, &f, sizeof(float));
    data.push_back(static_cast<uint8_t>(bits & 0xFF));
    data.push_back(static_cast<uint8_t>((bits >> 8) & 0xFF));
    data.push_back(static_cast<uint8_t>((bits >> 16) & 0xFF));
    data.push_back(static_cast<uint8_t>((bits >> 24) & 0xFF));
  };

  pushFloat(state.x);
  pushFloat(state.y);
  pushFloat(state.rotation);
  pushFloat(state.turretAngle);
  pushFloat(state.health);
  data.push_back(static_cast<uint8_t>(state.team));
  data.push_back(state.activated ? 1 : 0);
  sendPacket(data);
}

void NetworkManager::sendNpcShoot(int npcId, float x, float y, float angle)
{
  if (!m_connected)
    return;

  std::vector<uint8_t> data;
  data.push_back(static_cast<uint8_t>(NetMessageType::NpcShoot));
  data.push_back(static_cast<uint8_t>(npcId));

  auto pushFloat = [&data](float f)
  {
    uint32_t bits;
    std::memcpy(&bits, &f, sizeof(float));
    data.push_back(static_cast<uint8_t>(bits & 0xFF));
    data.push_back(static_cast<uint8_t>((bits >> 8) & 0xFF));
    data.push_back(static_cast<uint8_t>((bits >> 16) & 0xFF));
    data.push_back(static_cast<uint8_t>((bits >> 24) & 0xFF));
  };

  pushFloat(x);
  pushFloat(y);
  pushFloat(angle);
  sendPacket(data);
}

void NetworkManager::sendNpcDamage(int npcId, float damage)
{
  if (!m_connected)
    return;

  std::vector<uint8_t> data;
  data.push_back(static_cast<uint8_t>(NetMessageType::NpcDamage));
  data.push_back(static_cast<uint8_t>(npcId));

  uint32_t bits;
  std::memcpy(&bits, &damage, sizeof(float));
  data.push_back(static_cast<uint8_t>(bits & 0xFF));
  data.push_back(static_cast<uint8_t>((bits >> 8) & 0xFF));
  data.push_back(static_cast<uint8_t>((bits >> 16) & 0xFF));
  data.push_back(static_cast<uint8_t>((bits >> 24) & 0xFF));
  sendPacket(data);
}

void NetworkManager::sendMazeData(const std::vector<std::string> &mazeData, bool isEscapeMode, bool isDarkMode)
{
  if (!m_connected)
    return;

  std::vector<uint8_t> data;
  data.push_back(static_cast<uint8_t>(NetMessageType::MazeData));

  // 游戏模式标志: bit 0 = isEscapeMode, bit 1 = isDarkMode
  uint8_t modeFlags = (isEscapeMode ? 1 : 0) | (isDarkMode ? 2 : 0);
  data.push_back(modeFlags);

  // 迷宫行数
  uint16_t rows = static_cast<uint16_t>(mazeData.size());
  data.push_back(static_cast<uint8_t>(rows & 0xFF));
  data.push_back(static_cast<uint8_t>((rows >> 8) & 0xFF));

  // 每行数据
  for (const auto &row : mazeData)
  {
    uint16_t len = static_cast<uint16_t>(row.length());
    data.push_back(static_cast<uint8_t>(len & 0xFF));
    data.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
    for (char c : row)
    {
      data.push_back(static_cast<uint8_t>(c));
    }
  }

  sendPacket(data);
}

void NetworkManager::update()
{
  if (!m_connected)
    return;

  receiveData();
}

void NetworkManager::sendPacket(const std::vector<uint8_t> &data)
{
  if (!m_connected)
    return;

  // 添加长度前缀 (2 bytes)
  std::vector<uint8_t> packet;
  uint16_t len = static_cast<uint16_t>(data.size());
  packet.push_back(static_cast<uint8_t>(len & 0xFF));
  packet.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
  packet.insert(packet.end(), data.begin(), data.end());

  m_socket.setBlocking(true);
  std::size_t sent = 0;
  [[maybe_unused]] auto status = m_socket.send(packet.data(), packet.size(), sent);
  m_socket.setBlocking(false);
}

void NetworkManager::receiveData()
{
  uint8_t buffer[1024];
  std::size_t received = 0;

  sf::Socket::Status status = m_socket.receive(buffer, sizeof(buffer), received);

  if (status == sf::Socket::Status::Done && received > 0)
  {
    m_receiveBuffer.insert(m_receiveBuffer.end(), buffer, buffer + received);

    // 处理完整的消息
    while (m_receiveBuffer.size() >= 2)
    {
      uint16_t len = m_receiveBuffer[0] | (m_receiveBuffer[1] << 8);

      if (m_receiveBuffer.size() >= 2 + len)
      {
        std::vector<uint8_t> message(m_receiveBuffer.begin() + 2,
                                     m_receiveBuffer.begin() + 2 + len);
        m_receiveBuffer.erase(m_receiveBuffer.begin(),
                              m_receiveBuffer.begin() + 2 + len);
        processMessage(message);
      }
      else
      {
        break; // 等待更多数据
      }
    }
  }
  else if (status == sf::Socket::Status::Disconnected)
  {
    m_connected = false;
    if (m_onDisconnected)
    {
      m_onDisconnected();
    }
  }
}

void NetworkManager::processMessage(const std::vector<uint8_t> &data)
{
  if (data.empty())
    return;

  NetMessageType type = static_cast<NetMessageType>(data[0]);

  auto readFloat = [&data](size_t offset) -> float
  {
    if (offset + 4 > data.size())
      return 0.0f;
    float value;
    std::memcpy(&value, &data[offset], sizeof(float));
    return value;
  };

  switch (type)
  {
  case NetMessageType::RoomCreated:
  {
    if (data.size() > 2)
    {
      uint8_t codeLen = data[1];
      m_roomCode = std::string(data.begin() + 2, data.begin() + 2 + codeLen);
      if (m_onRoomCreated)
      {
        m_onRoomCreated(m_roomCode);
      }
    }
    break;
  }
  case NetMessageType::RoomJoined:
  {
    if (data.size() > 2)
    {
      uint8_t codeLen = data[1];
      m_roomCode = std::string(data.begin() + 2, data.begin() + 2 + codeLen);
      if (m_onRoomJoined)
      {
        m_onRoomJoined(m_roomCode);
      }
    }
    break;
  }
  case NetMessageType::RoomError:
  {
    if (data.size() > 2)
    {
      uint8_t errorLen = data[1];
      std::string error(data.begin() + 2, data.begin() + 2 + errorLen);
      if (m_onError)
      {
        m_onError(error);
      }
    }
    break;
  }
  case NetMessageType::GameStart:
  {
    // 新版本：GameStart 只是一个信号，迷宫数据已经通过 MazeData 传输
    if (m_onGameStart)
    {
      m_onGameStart();
    }
    break;
  }
  case NetMessageType::MazeData:
  {
    // 解析迷宫数据
    if (data.size() >= 4)
    {
      size_t offset = 1;

      // 读取游戏模式标志: bit 0 = isEscapeMode, bit 1 = isDarkMode
      uint8_t modeFlags = data[offset];
      bool isEscapeMode = (modeFlags & 1) != 0;
      bool isDarkMode = (modeFlags & 2) != 0;
      offset += 1;

      uint16_t rows = data[offset] | (data[offset + 1] << 8);
      offset += 2;

      std::vector<std::string> mazeData;
      for (uint16_t i = 0; i < rows && offset + 2 <= data.size(); i++)
      {
        uint16_t len = data[offset] | (data[offset + 1] << 8);
        offset += 2;
        if (offset + len <= data.size())
        {
          std::string row(data.begin() + offset, data.begin() + offset + len);
          mazeData.push_back(row);
          offset += len;
        }
      }

      // 先设置游戏模式，再回调 MazeData
      if (m_onGameModeReceived)
      {
        m_onGameModeReceived(isEscapeMode);
      }

      if (m_onMazeData)
      {
        m_onMazeData(mazeData, isDarkMode);
      }
    }
    break;
  }
  case NetMessageType::RequestMaze:
  {
    // 服务器请求房主发送迷宫数据
    if (m_onRequestMaze)
    {
      m_onRequestMaze();
    }
    break;
  }
  case NetMessageType::PlayerUpdate:
  {
    if (data.size() >= 22)
    {
      PlayerState state;
      state.x = readFloat(1);
      state.y = readFloat(5);
      state.rotation = readFloat(9);
      state.turretAngle = readFloat(13);
      state.health = readFloat(17);
      state.reachedExit = data[21] != 0;
      state.isDead = (data.size() >= 23) ? (data[22] != 0) : false; // 读取死亡状态
      if (m_onPlayerUpdate)
      {
        m_onPlayerUpdate(state);
      }
    }
    break;
  }
  case NetMessageType::PlayerShoot:
  {
    if (data.size() >= 13)
    {
      float x = readFloat(1);
      float y = readFloat(5);
      float angle = readFloat(9);
      if (m_onPlayerShoot)
      {
        m_onPlayerShoot(x, y, angle);
      }
    }
    break;
  }
  case NetMessageType::GameWin:
  {
    // 游戏胜利，两个玩家都到达终点
    // 可以在这里触发胜利回调
    break;
  }
  case NetMessageType::GameResult:
  {
    // 游戏结果 - 对方发来的是对方自己的结果（true=对方赢了, false=对方输了）
    if (data.size() >= 2)
    {
      bool otherPlayerResult = data[1] != 0;
      if (m_onGameResult)
      {
        // 直接传递对方的结果，由回调决定如何处理
        // Battle模式：对方输=我赢，所以回调中会取反
        // Escape模式：队友关系，结果相同
        m_onGameResult(otherPlayerResult);
      }
    }
    break;
  }
  case NetMessageType::RestartRequest:
  {
    // 对方请求重新开始
    if (m_onRestartRequest)
    {
      m_onRestartRequest();
    }
    break;
  }
  case NetMessageType::NpcActivate:
  {
    // NPC激活消息
    if (data.size() >= 4 && m_onNpcActivate)
    {
      int npcId = data[1];
      int team = data[2];
      int activatorId = static_cast<int>(data[3]) - 128; // 还原负数
      m_onNpcActivate(npcId, team, activatorId);
    }
    break;
  }
  case NetMessageType::NpcUpdate:
  {
    // NPC状态更新
    if (data.size() >= 24 && m_onNpcUpdate)
    {
      NpcState state;
      state.id = data[1];
      state.x = readFloat(2);
      state.y = readFloat(6);
      state.rotation = readFloat(10);
      state.turretAngle = readFloat(14);
      state.health = readFloat(18);
      state.team = data[22];
      state.activated = data[23] != 0;
      m_onNpcUpdate(state);
    }
    break;
  }
  case NetMessageType::NpcShoot:
  {
    // NPC射击
    if (data.size() >= 14 && m_onNpcShoot)
    {
      int npcId = data[1];
      float x = readFloat(2);
      float y = readFloat(6);
      float angle = readFloat(10);
      m_onNpcShoot(npcId, x, y, angle);
    }
    break;
  }
  case NetMessageType::NpcDamage:
  {
    // NPC受伤
    if (data.size() >= 6 && m_onNpcDamage)
    {
      int npcId = data[1];
      float damage = readFloat(2);
      m_onNpcDamage(npcId, damage);
    }
    break;
  }
  case NetMessageType::PlayerLeft:
  {
    // 对方玩家离开房间
    if (m_onPlayerLeft)
    {
      bool becameHost = (data.size() >= 2) ? (data[1] != 0) : false;
      m_onPlayerLeft(becameHost);
    }
    break;
  }
  case NetMessageType::ClimaxStart:
  {
    // 对方看到出口，开始播放高潮BGM
    if (m_onClimaxStart)
    {
      m_onClimaxStart();
    }
    break;
  }
  case NetMessageType::WallPlace:
  {
    // 对方放置墙壁
    if (data.size() >= 9 && m_onWallPlace)
    {
      float x, y;
      std::memcpy(&x, &data[1], sizeof(float));
      std::memcpy(&y, &data[5], sizeof(float));
      m_onWallPlace(x, y);
    }
    break;
  }
  case NetMessageType::WallDamage:
  {
    // 墙壁受到伤害同步
    if (data.size() >= 22 && m_onWallDamage)
    {
      int row, col, attribute, destroyerId;
      float damage;
      bool destroyed;
      std::memcpy(&row, &data[1], sizeof(int));
      std::memcpy(&col, &data[5], sizeof(int));
      std::memcpy(&damage, &data[9], sizeof(float));
      destroyed = (data[13] != 0);
      std::memcpy(&attribute, &data[14], sizeof(int));
      std::memcpy(&destroyerId, &data[18], sizeof(int));
      m_onWallDamage(row, col, damage, destroyed, attribute, destroyerId);
    }
    break;
  }
  case NetMessageType::RescueStart:
  {
    if (m_onRescueStart)
    {
      m_onRescueStart();
    }
    break;
  }
  case NetMessageType::RescueProgress:
  {
    if (data.size() >= 5 && m_onRescueProgress)
    {
      float progress;
      std::memcpy(&progress, &data[1], sizeof(float));
      m_onRescueProgress(progress);
    }
    break;
  }
  case NetMessageType::RescueComplete:
  {
    if (m_onRescueComplete)
    {
      m_onRescueComplete();
    }
    break;
  }
  case NetMessageType::RescueCancel:
  {
    if (m_onRescueCancel)
    {
      m_onRescueCancel();
    }
    break;
  }
  case NetMessageType::PlayerReady:
  {
    if (data.size() >= 2 && m_onPlayerReady)
    {
      bool isReady = (data[1] != 0);
      m_onPlayerReady(isReady);
    }
    break;
  }
  case NetMessageType::RoomInfo:
  {
    // 房间信息：hostIP长度(1) + hostIP + guestIP长度(1) + guestIP + guestReady(1) + isDarkMode(1)
    if (data.size() >= 3 && m_onRoomInfo)
    {
      size_t offset = 1;
      uint8_t hostIPLen = data[offset++];
      std::string hostIP(data.begin() + offset, data.begin() + offset + hostIPLen);
      offset += hostIPLen;

      uint8_t guestIPLen = data[offset++];
      std::string guestIP;
      if (guestIPLen > 0)
      {
        guestIP = std::string(data.begin() + offset, data.begin() + offset + guestIPLen);
        offset += guestIPLen;
      }

      bool guestReady = (data.size() > offset) ? (data[offset++] != 0) : false;
      bool isDarkMode = (data.size() > offset) ? (data[offset] != 0) : false;
      m_onRoomInfo(hostIP, guestIP, guestReady, isDarkMode);
    }
    break;
  }
  default:
    break;
  }
}
