#include "AudioManager.hpp"
#include <cmath>
#include <algorithm>
#include <iostream>

AudioManager &AudioManager::getInstance()
{
  static AudioManager instance;
  return instance;
}

bool AudioManager::init(const std::string &assetPath)
{
  if (m_initialized)
    return true;

  std::cout << "[Audio] Initializing audio system..." << std::endl;

  // 加载背景音乐
  if (!m_bgmMenu.openFromFile(assetPath + "menu.mp3"))
  {
    std::cerr << "[Audio] Failed to load menu.mp3" << std::endl;
    return false;
  }
  m_bgmMenu.setLooping(true);

  if (!m_bgmStart.openFromFile(assetPath + "start.mp3"))
  {
    std::cerr << "[Audio] Failed to load start.mp3" << std::endl;
    return false;
  }
  m_bgmStart.setLooping(false); // start只播放一次

  if (!m_bgmMiddle.openFromFile(assetPath + "middle.mp3"))
  {
    std::cerr << "[Audio] Failed to load middle.mp3" << std::endl;
    return false;
  }
  m_bgmMiddle.setLooping(true); // middle循环播放

  if (!m_bgmClimax.openFromFile(assetPath + "climax.mp3"))
  {
    std::cerr << "[Audio] Failed to load climax.mp3" << std::endl;
    return false;
  }
  m_bgmClimax.setLooping(true);

  // 加载音效
  sf::SoundBuffer buffer;

  if (!buffer.loadFromFile(assetPath + "shoot.mp3"))
  {
    std::cerr << "[Audio] Failed to load shoot.mp3" << std::endl;
    return false;
  }
  m_sfxBuffers[SFXType::Shoot] = buffer;

  if (!buffer.loadFromFile(assetPath + "BulletCollideWithWalls.mp3"))
  {
    std::cerr << "[Audio] Failed to load BulletCollideWithWalls.mp3" << std::endl;
    return false;
  }
  m_sfxBuffers[SFXType::BulletHitWall] = buffer;

  if (!buffer.loadFromFile(assetPath + "BulletCollideWithTanks.mp3"))
  {
    std::cerr << "[Audio] Failed to load BulletCollideWithTanks.mp3" << std::endl;
    return false;
  }
  m_sfxBuffers[SFXType::BulletHitTank] = buffer;

  if (!buffer.loadFromFile(assetPath + "explode.mp3"))
  {
    std::cerr << "[Audio] Failed to load explode.mp3" << std::endl;
    return false;
  }
  m_sfxBuffers[SFXType::Explode] = buffer;

  if (!buffer.loadFromFile(assetPath + "collectCoins.mp3"))
  {
    std::cerr << "[Audio] Failed to load collectCoins.mp3" << std::endl;
    return false;
  }
  m_sfxBuffers[SFXType::CollectCoins] = buffer;

  if (!buffer.loadFromFile(assetPath + "Bingo.mp3"))
  {
    std::cerr << "[Audio] Failed to load Bingo.mp3" << std::endl;
    return false;
  }
  m_sfxBuffers[SFXType::Bingo] = buffer;

  if (!buffer.loadFromFile(assetPath + "wallBroken.mp3"))
  {
    std::cerr << "[Audio] Failed to load wallBroken.mp3" << std::endl;
    return false;
  }
  m_sfxBuffers[SFXType::WallBroken] = buffer;

  if (!buffer.loadFromFile(assetPath + "chosen.mp3"))
  {
    std::cerr << "[Audio] Failed to load chosen.mp3" << std::endl;
    return false;
  }
  m_sfxBuffers[SFXType::MenuSelect] = buffer;

  if (!buffer.loadFromFile(assetPath + "confirm.mp3"))
  {
    std::cerr << "[Audio] Failed to load confirm.mp3" << std::endl;
    return false;
  }
  m_sfxBuffers[SFXType::MenuConfirm] = buffer;

  m_initialized = true;
  std::cout << "[Audio] Audio system initialized successfully" << std::endl;
  return true;
}

void AudioManager::playBGM(BGMType type)
{
  // 如果已经在播放相同的BGM，不做任何事
  if (m_currentBGMPlayer != nullptr && m_currentBGM == type)
  {
    if (m_currentBGMPlayer->getStatus() == sf::Sound::Status::Playing)
      return;
  }

  // 停止当前BGM
  stopBGM();

  // 选择新的BGM
  switch (type)
  {
  case BGMType::Menu:
    m_currentBGMPlayer = &m_bgmMenu;
    break;
  case BGMType::Start:
    m_currentBGMPlayer = &m_bgmStart;
    break;
  case BGMType::Middle:
    m_currentBGMPlayer = &m_bgmMiddle;
    break;
  case BGMType::Climax:
    m_currentBGMPlayer = &m_bgmClimax;
    break;
  }

  m_currentBGM = type;

  if (m_currentBGMPlayer)
  {
    m_currentBGMPlayer->setVolume(m_bgmVolume);
    m_currentBGMPlayer->play();
  }
}

void AudioManager::stopBGM()
{
  if (m_currentBGMPlayer)
  {
    m_currentBGMPlayer->stop();
    m_currentBGMPlayer = nullptr;
  }
}

void AudioManager::setBGMVolume(float volume)
{
  m_bgmVolume = std::clamp(volume, 0.f, 100.f);
  if (m_currentBGMPlayer)
  {
    m_currentBGMPlayer->setVolume(m_bgmVolume);
  }
}

float AudioManager::calculateVolume(sf::Vector2f soundPos, sf::Vector2f listenerPos) const
{
  float dx = soundPos.x - listenerPos.x;
  float dy = soundPos.y - listenerPos.y;
  float distance = std::sqrt(dx * dx + dy * dy);

  if (distance >= m_listeningRange)
    return 0.f; // 超出范围，不播放

  // 线性衰减：距离越近音量越大
  float volumeRatio = 1.f - (distance / m_listeningRange);
  return m_sfxVolume * volumeRatio;
}

void AudioManager::playSFX(SFXType type, sf::Vector2f soundPos, sf::Vector2f listenerPos)
{
  float volume = calculateVolume(soundPos, listenerPos);

  if (volume <= 0.f)
    return; // 超出听音范围，不播放

  auto it = m_sfxBuffers.find(type);
  if (it == m_sfxBuffers.end())
    return;

  // 创建新的Sound实例
  auto sound = std::make_unique<sf::Sound>(it->second);
  sound->setVolume(volume);
  sound->play();

  m_activeSounds.push_back(std::move(sound));
}

void AudioManager::playSFXGlobal(SFXType type)
{
  auto it = m_sfxBuffers.find(type);
  if (it == m_sfxBuffers.end())
    return;

  auto sound = std::make_unique<sf::Sound>(it->second);
  sound->setVolume(m_sfxVolume);
  sound->play();

  m_activeSounds.push_back(std::move(sound));
}

void AudioManager::playLoopSFX(SFXType type)
{
  // 如果已经在播放，不重复创建
  auto it = m_loopSounds.find(type);
  if (it != m_loopSounds.end() && it->second->getStatus() == sf::Sound::Status::Playing)
    return;

  auto bufferIt = m_sfxBuffers.find(type);
  if (bufferIt == m_sfxBuffers.end())
    return;

  auto sound = std::make_unique<sf::Sound>(bufferIt->second);
  sound->setLooping(true);
  sound->setVolume(m_sfxVolume);
  sound->play();

  m_loopSounds[type] = std::move(sound);
}

void AudioManager::stopLoopSFX(SFXType type)
{
  auto it = m_loopSounds.find(type);
  if (it != m_loopSounds.end())
  {
    it->second->stop();
    m_loopSounds.erase(it);
  }
}

bool AudioManager::isLoopSFXPlaying(SFXType type) const
{
  auto it = m_loopSounds.find(type);
  if (it != m_loopSounds.end())
    return it->second->getStatus() == sf::Sound::Status::Playing;
  return false;
}

void AudioManager::setSFXVolume(float volume)
{
  m_sfxVolume = std::clamp(volume, 0.f, 100.f);
}

void AudioManager::update()
{
  // 检查 start BGM 是否播放完毕，自动切换到 middle
  if (m_currentBGM == BGMType::Start && m_currentBGMPlayer != nullptr)
  {
    if (m_currentBGMPlayer->getStatus() == sf::Sound::Status::Stopped)
    {
      playBGM(BGMType::Middle);
    }
  }

  // 清理已播放完的音效
  m_activeSounds.erase(
      std::remove_if(m_activeSounds.begin(), m_activeSounds.end(),
                     [](const std::unique_ptr<sf::Sound> &sound)
                     {
                       return sound->getStatus() == sf::Sound::Status::Stopped;
                     }),
      m_activeSounds.end());
}

void AudioManager::stopAllSFX()
{
  // 停止并清空所有活跃音效
  for (auto &sound : m_activeSounds)
  {
    sound->stop();
  }
  m_activeSounds.clear();

  // 停止所有循环音效
  for (auto &[type, sound] : m_loopSounds)
  {
    sound->stop();
  }
  m_loopSounds.clear();
}

void AudioManager::pauseAll()
{
  if (m_currentBGMPlayer)
  {
    m_currentBGMPlayer->pause();
  }

  for (auto &sound : m_activeSounds)
  {
    if (sound->getStatus() == sf::Sound::Status::Playing)
    {
      sound->pause();
    }
  }
}

void AudioManager::resumeAll()
{
  if (m_currentBGMPlayer)
  {
    m_currentBGMPlayer->play();
  }

  for (auto &sound : m_activeSounds)
  {
    if (sound->getStatus() == sf::Sound::Status::Paused)
    {
      sound->play();
    }
  }
}
