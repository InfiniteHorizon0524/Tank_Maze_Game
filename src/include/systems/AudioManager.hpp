#pragma once

#include <SFML/Audio.hpp>
#include <string>
#include <unordered_map>
#include <memory>
#include <vector>

// 背景音乐类型
enum class BGMType
{
  Menu,   // 主菜单
  Start,  // 游戏开始（只播放一次）
  Middle, // 游戏中段（循环播放）
  Climax  // 高潮（看到终点）
};

// 音效类型
enum class SFXType
{
  Shoot,         // 发射子弹
  BulletHitWall, // 子弹击中墙体
  BulletHitTank, // 子弹击中坦克/NPC
  Explode,       // 爆炸（坦克死亡/红色墙爆炸）
  CollectCoins,  // 收集金币（黄色墙被打掉）
  Bingo,         // 蓝色治疗墙被打掉
  WallBroken,    // 棕色墙被打爆
  MenuSelect,    // 菜单选项切换
  MenuConfirm    // 菜单确认
};

class AudioManager
{
public:
  static AudioManager &getInstance();

  // 初始化（加载所有音频资源）
  bool init(const std::string &assetPath = "music_assets/");

  // 背景音乐控制
  void playBGM(BGMType type);
  void stopBGM();
  void setBGMVolume(float volume); // 0-100
  BGMType getCurrentBGM() const { return m_currentBGM; }

  // 音效播放（带位置，用于距离衰减）
  void playSFX(SFXType type, sf::Vector2f soundPos, sf::Vector2f listenerPos);

  // 音效播放（无位置，全局音效）
  void playSFXGlobal(SFXType type);

  // 循环音效控制（用于坦克移动等）
  void playLoopSFX(SFXType type);
  void stopLoopSFX(SFXType type);
  bool isLoopSFXPlaying(SFXType type) const;

  // 设置音效音量
  void setSFXVolume(float volume); // 0-100

  // 设置听音范围（超出此范围的音效不播放）
  void setListeningRange(float range) { m_listeningRange = range; }
  float getListeningRange() const { return m_listeningRange; }

  // 更新（清理已播放完的音效）
  void update();

  // 停止所有音效（重启游戏时调用）
  void stopAllSFX();

  // 暂停/恢复所有音频
  void pauseAll();
  void resumeAll();

private:
  AudioManager() = default;
  ~AudioManager() = default;
  AudioManager(const AudioManager &) = delete;
  AudioManager &operator=(const AudioManager &) = delete;

  // 根据距离计算音量（0-100）
  float calculateVolume(sf::Vector2f soundPos, sf::Vector2f listenerPos) const;

  // 背景音乐
  sf::Music m_bgmMenu;
  sf::Music m_bgmStart;
  sf::Music m_bgmMiddle;
  sf::Music m_bgmClimax;
  sf::Music *m_currentBGMPlayer = nullptr;
  BGMType m_currentBGM = BGMType::Menu;
  float m_bgmVolume = 50.f;

  // 音效缓冲区
  std::unordered_map<SFXType, sf::SoundBuffer> m_sfxBuffers;

  // 活跃的音效实例（用于同时播放多个相同音效）
  std::vector<std::unique_ptr<sf::Sound>> m_activeSounds;

  float m_sfxVolume = 70.f;
  float m_listeningRange = 800.f; // 默认听音范围（像素）

  // 循环音效实例
  std::unordered_map<SFXType, std::unique_ptr<sf::Sound>> m_loopSounds;

  bool m_initialized = false;
};
