#include "Game.hpp"

int main()
{
  Game game;

  if (!game.init())
  {
    return -1;
  }

  game.run();
  return 0;
}