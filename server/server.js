const net = require('net');

// 消息类型（与 C++ 客户端一致）
const MessageType = {
  Connect: 1,
  ConnectAck: 2,
  Disconnect: 3,
  CreateRoom: 4,
  JoinRoom: 5,
  RoomCreated: 6,
  RoomJoined: 7,
  RoomError: 8,
  GameStart: 9,
  PlayerUpdate: 10,
  PlayerShoot: 11,
  MazeData: 12,
  RequestMaze: 13,
  ReachExit: 14,
  GameWin: 15,
  GameResult: 16,
  RestartRequest: 17,
  // NPC同步
  NpcActivate: 18,
  NpcUpdate: 19,
  NpcShoot: 20,
  NpcDamage: 21,
  // 墙壁放置
  WallPlace: 22,
  // 音乐同步
  ClimaxStart: 23,
  // 玩家离开
  PlayerLeft: 24,
  // 救援同步
  RescueStart: 25,
  RescueProgress: 26,
  RescueComplete: 27,
  RescueCancel: 28,
  // 房间大厅
  PlayerReady: 29,
  HostStartGame: 30,
  RoomInfo: 31,
  // 墙壁伤害同步
  WallDamage: 32
};

// 房间管理
const rooms = new Map();

// 生成随机房间码（纯4位数字）
function generateRoomCode() {
  const code = Math.floor(1000 + Math.random() * 9000).toString();
  return code;
}

// 获取客户端IP地址
function getClientIP(socket) {
  const addr = socket.remoteAddress || '';
  // 去掉 IPv6 前缀
  return addr.replace(/^::ffff:/, '');
}

// 发送房间信息给所有玩家
function sendRoomInfo(room) {
  const host = room.players.find(p => p.isHost);
  const guest = room.players.find(p => !p.isHost);

  const hostIP = host ? getClientIP(host.socket) : '';
  const guestIP = guest ? getClientIP(guest.socket) : '';
  const guestReady = guest ? guest.ready : false;

  // 构建消息: RoomInfo + hostIPLen + hostIP + guestIPLen + guestIP + guestReady + isDarkMode
  const hostIPBuf = Buffer.from(hostIP);
  const guestIPBuf = Buffer.from(guestIP);
  const response = Buffer.alloc(1 + 1 + hostIPBuf.length + 1 + guestIPBuf.length + 1 + 1);

  let offset = 0;
  response[offset++] = MessageType.RoomInfo;
  response[offset++] = hostIPBuf.length;
  hostIPBuf.copy(response, offset);
  offset += hostIPBuf.length;
  response[offset++] = guestIPBuf.length;
  guestIPBuf.copy(response, offset);
  offset += guestIPBuf.length;
  response[offset++] = guestReady ? 1 : 0;
  response[offset++] = room.isDarkMode ? 1 : 0;

  for (const player of room.players) {
    sendMessage(player.socket, response);
  }
}

// 发送消息给客户端
function sendMessage(socket, data) {
  const len = data.length;
  const packet = Buffer.alloc(2 + len);
  packet.writeUInt16LE(len, 0);
  data.copy(packet, 2);
  socket.write(packet);
}

// 广播给房间内其他玩家
function broadcastToRoom(room, senderSocket, data) {
  for (const player of room.players) {
    if (player.socket !== senderSocket) {
      sendMessage(player.socket, data);
    }
  }
}

// 处理消息
function handleMessage(socket, data) {
  if (data.length < 1) return;

  const msgType = data[0];

  switch (msgType) {
    case MessageType.Connect: {
      console.log('Client connected');
      // 发送连接确认
      const response = Buffer.alloc(1);
      response[0] = MessageType.ConnectAck;
      sendMessage(socket, response);
      break;
    }

    case MessageType.Disconnect: {
      console.log('Client requested disconnect');
      // 触发清理逻辑（与close事件相同）
      socket.end(); // 这会触发close事件，进而调用cleanupPlayer
      break;
    }

    case MessageType.CreateRoom: {
      // 读取迷宫尺寸
      const mazeWidth = data.readUInt16LE(1);
      const mazeHeight = data.readUInt16LE(3);
      // 读取暗黑模式标志（如果存在）
      const isDarkMode = data.length > 5 ? data[5] !== 0 : false;

      // 创建房间
      let roomCode;
      do {
        roomCode = generateRoomCode();
      } while (rooms.has(roomCode));

      const room = {
        code: roomCode,
        mazeWidth: mazeWidth,
        mazeHeight: mazeHeight,
        mazeData: null,  // 存储迷宫数据
        players: [{ socket: socket, reachedExit: false, isHost: true, ready: true }],
        started: false,
        isEscapeMode: false,  // 游戏模式（从迷宫数据中读取）
        isDarkMode: isDarkMode  // 暗黑模式
      };

      rooms.set(roomCode, room);
      socket.roomCode = roomCode;
      socket.isHost = true;

      console.log(`Room created: ${roomCode} (${mazeWidth}x${mazeHeight}) darkMode=${isDarkMode}`);

      // 发送房间创建成功
      const response = Buffer.alloc(2 + roomCode.length);
      response[0] = MessageType.RoomCreated;
      response[1] = roomCode.length;
      response.write(roomCode, 2);
      sendMessage(socket, response);

      // 发送房间信息
      sendRoomInfo(room);
      break;
    }

    case MessageType.MazeData: {
      // 房主发送迷宫数据
      const roomCode = socket.roomCode;
      if (!roomCode) break;

      const room = rooms.get(roomCode);
      if (!room) break;

      // 只有房主可以发送迷宫数据
      if (!socket.isHost) break;

      // 存储迷宫数据（包含游戏模式）
      room.mazeData = data;
      // 解析游戏模式（第2个字节）
      if (data.length >= 2) {
        room.isEscapeMode = (data[1] !== 0);
      }
      console.log(`Received maze data for room ${roomCode}, size: ${data.length} bytes, isEscapeMode: ${room.isEscapeMode}`);

      // 如果有第二个玩家在房间，发送迷宫数据给他（但不开始游戏）
      if (room.players.length === 2) {
        const guest = room.players.find(p => !p.isHost);
        if (guest) {
          sendMessage(guest.socket, data);
          console.log(`Sent maze data to guest in room ${roomCode}`);
        }
      }
      break;
    }

    case MessageType.JoinRoom: {
      const codeLen = data[1];
      const roomCode = data.slice(2, 2 + codeLen).toString();

      const room = rooms.get(roomCode);
      if (!room) {
        // 房间不存在
        const error = "Room not found";
        const response = Buffer.alloc(2 + error.length);
        response[0] = MessageType.RoomError;
        response[1] = error.length;
        response.write(error, 2);
        sendMessage(socket, response);
        break;
      }

      if (room.players.length >= 2) {
        const error = "Room is full";
        const response = Buffer.alloc(2 + error.length);
        response[0] = MessageType.RoomError;
        response[1] = error.length;
        response.write(error, 2);
        sendMessage(socket, response);
        break;
      }

      room.players.push({ socket: socket, reachedExit: false, isHost: false, ready: false });
      socket.roomCode = roomCode;
      socket.isHost = false;

      console.log(`Player joined room: ${roomCode}`);

      // 发送加入成功
      const response = Buffer.alloc(2 + roomCode.length);
      response[0] = MessageType.RoomJoined;
      response[1] = roomCode.length;
      response.write(roomCode, 2);
      sendMessage(socket, response);

      // 发送迷宫数据给新玩家（如果已有）
      if (room.mazeData) {
        sendMessage(socket, room.mazeData);
        console.log(`Sent existing maze data to guest in room ${roomCode}`);
      } else {
        // 如果没有迷宫数据，请求房主发送
        const host = room.players.find(p => p.isHost);
        if (host) {
          const requestMaze = Buffer.alloc(1);
          requestMaze[0] = MessageType.RequestMaze;
          sendMessage(host.socket, requestMaze);
          console.log(`Requested maze data from host in room ${roomCode}`);
        }
      }

      // 发送房间信息给所有玩家
      sendRoomInfo(room);
      break;
    }

    case MessageType.PlayerReady: {
      const roomCode = socket.roomCode;
      if (!roomCode) break;

      const room = rooms.get(roomCode);
      if (!room) break;

      const player = room.players.find(p => p.socket === socket);
      if (player) {
        player.ready = (data[1] !== 0);
        console.log(`Player ready status in room ${roomCode}: ${player.ready}`);

        // 广播准备状态给其他玩家
        broadcastToRoom(room, socket, data);

        // 发送更新后的房间信息
        sendRoomInfo(room);
      }
      break;
    }

    case MessageType.HostStartGame: {
      const roomCode = socket.roomCode;
      if (!roomCode) break;

      const room = rooms.get(roomCode);
      if (!room) break;

      // 只有房主可以开始游戏
      if (!socket.isHost) {
        console.log(`Non-host tried to start game in room ${roomCode}`);
        break;
      }

      // 检查是否满足开始条件：有2个玩家且对方已准备
      const guest = room.players.find(p => !p.isHost);
      if (!guest || !guest.ready) {
        console.log(`Cannot start game in room ${roomCode}: guest not ready`);
        break;
      }

      if (room.players.length < 2) {
        console.log(`Cannot start game in room ${roomCode}: not enough players`);
        break;
      }

      room.started = true;

      // 游戏开始后重置所有玩家的 ready 状态
      // 这样返回房间时需要重新准备
      for (const player of room.players) {
        player.ready = false;
      }

      // 发送游戏开始消息给两个玩家
      const gameStartMsg = Buffer.alloc(1);
      gameStartMsg[0] = MessageType.GameStart;
      for (const player of room.players) {
        sendMessage(player.socket, gameStartMsg);
      }
      console.log(`Game started in room: ${roomCode}`);
      break;
    }

    case MessageType.PlayerUpdate: {
      const roomCode = socket.roomCode;
      if (!roomCode) break;

      const room = rooms.get(roomCode);
      if (!room || !room.started) break;

      // 转发给其他玩家
      broadcastToRoom(room, socket, data);
      break;
    }

    case MessageType.PlayerShoot: {
      const roomCode = socket.roomCode;
      if (!roomCode) break;

      const room = rooms.get(roomCode);
      if (!room || !room.started) break;

      // 转发给其他玩家
      broadcastToRoom(room, socket, data);
      break;
    }

    case MessageType.ReachExit: {
      const roomCode = socket.roomCode;
      if (!roomCode) break;

      const room = rooms.get(roomCode);
      if (!room || !room.started) break;

      // 标记玩家到达终点
      const player = room.players.find(p => p.socket === socket);
      if (player) {
        player.reachedExit = true;
        console.log(`Player reached exit in room ${roomCode}`);

        // 检查是否所有玩家都到达终点
        const allReached = room.players.every(p => p.reachedExit);
        if (allReached) {
          // 发送游戏胜利消息
          const winMsg = Buffer.alloc(1);
          winMsg[0] = MessageType.GameWin;
          for (const p of room.players) {
            sendMessage(p.socket, winMsg);
          }
          console.log(`All players reached exit in room ${roomCode}! Game won!`);
        }
      }

      // 转发给其他玩家
      broadcastToRoom(room, socket, data);
      break;
    }

    case MessageType.GameResult: {
      const roomCode = socket.roomCode;
      if (!roomCode) break;

      const room = rooms.get(roomCode);
      if (!room || !room.started) break;

      // 转发游戏结果给其他玩家
      broadcastToRoom(room, socket, data);
      console.log(`Game result sent in room ${roomCode}`);
      break;
    }

    case MessageType.RestartRequest: {
      const roomCode = socket.roomCode;
      if (!roomCode) break;

      const room = rooms.get(roomCode);
      if (!room) break;

      // 重置房间状态，准备下一轮
      room.started = false;

      // 找到发送请求的玩家
      const sender = room.players.find(p => p.socket === socket);

      // 重置 reachedExit 状态，并根据发送者身份设置 ready 状态
      for (const player of room.players) {
        player.reachedExit = false;

        if (player.socket === socket) {
          // 发送者返回房间：房主自动 ready，非房主保持 not ready
          player.ready = player.isHost;
        }
        // 其他玩家的 ready 状态保持不变（可能已经先返回并准备好了）
      }

      // 转发重新开始请求给其他玩家
      broadcastToRoom(room, socket, data);

      // 发送更新后的房间信息
      sendRoomInfo(room);
      console.log(`Restart request in room ${roomCode} from ${sender?.isHost ? 'host' : 'guest'}`);
      break;
    }

    // 救援同步消息
    case MessageType.RescueStart:
    case MessageType.RescueProgress:
    case MessageType.RescueComplete:
    case MessageType.RescueCancel: {
      const roomCode = socket.roomCode;
      if (!roomCode) break;

      const room = rooms.get(roomCode);
      if (!room || !room.started) break;

      // 转发给其他玩家
      broadcastToRoom(room, socket, data);
      break;
    }

    // NPC同步消息 - 直接转发给房间内其他玩家
    case MessageType.NpcActivate:
    case MessageType.NpcUpdate:
    case MessageType.NpcShoot:
    case MessageType.NpcDamage:
    case MessageType.ClimaxStart:
    case MessageType.WallPlace:
    case MessageType.WallDamage: {
      const roomCode = socket.roomCode;
      if (!roomCode) break;

      const room = rooms.get(roomCode);
      if (!room || !room.started) break;

      // 转发给其他玩家
      broadcastToRoom(room, socket, data);
      break;
    }
  }
}

// 创建服务器
const server = net.createServer((socket) => {
  console.log(`Client connected from ${socket.remoteAddress}`);

  let buffer = Buffer.alloc(0);
  let isCleanedUp = false; // 防止重复清理

  // 清理玩家从房间中移除的函数
  function cleanupPlayer() {
    if (isCleanedUp) return;
    isCleanedUp = true;

    console.log('Cleaning up player...');

    if (socket.roomCode) {
      const room = rooms.get(socket.roomCode);
      if (room) {
        const wasHost = socket.isHost;
        room.players = room.players.filter(p => p.socket !== socket);

        if (room.players.length === 0) {
          rooms.delete(socket.roomCode);
          console.log(`Room ${socket.roomCode} deleted`);
        } else {
          // 重置房间状态，让剩余玩家回到房间大厅
          room.started = false;
          for (const player of room.players) {
            player.reachedExit = false;
            // 新房主自动准备就绪
            if (wasHost) {
              player.ready = true;
            }
          }

          // 如果离开的是房主，让剩余玩家成为新房主
          if (wasHost && room.players.length > 0) {
            room.players[0].isHost = true;
            room.players[0].socket.isHost = true;
            room.players[0].ready = true;  // 房主默认准备
            console.log(`New host assigned in room ${socket.roomCode}`);
          }

          // 通知剩余玩家对方已离开，并告知是否成为新房主
          for (const player of room.players) {
            try {
              const playerLeftMsg = Buffer.alloc(2);
              playerLeftMsg[0] = MessageType.PlayerLeft;
              playerLeftMsg[1] = player.isHost ? 1 : 0;
              sendMessage(player.socket, playerLeftMsg);
            } catch (e) {
              console.error('Error sending PlayerLeft message:', e.message);
            }
          }

          // 发送更新后的房间信息
          sendRoomInfo(room);
          console.log(`Notified remaining players in room ${socket.roomCode}`);
        }
      }
    }
  }

  socket.on('data', (data) => {
    buffer = Buffer.concat([buffer, data]);

    // 处理所有完整的消息
    while (buffer.length >= 2) {
      const msgLen = buffer.readUInt16LE(0);
      if (buffer.length < 2 + msgLen) break;

      const msgData = buffer.slice(2, 2 + msgLen);
      buffer = buffer.slice(2 + msgLen);

      try {
        handleMessage(socket, msgData);
      } catch (e) {
        console.error('Error handling message:', e);
      }
    }
  });

  socket.on('close', () => {
    console.log('Client disconnected (close event)');
    cleanupPlayer();
  });

  socket.on('error', (err) => {
    console.error('Socket error:', err.message);
    cleanupPlayer();
  });
});

const PORT = 9999;
server.listen(PORT, () => {
  console.log(`Tank Maze Server running on port ${PORT}`);
  console.log('Waiting for players...');
});
