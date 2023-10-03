#include <SDL2/SDL.h>
#include <stdio.h>

#ifndef SCENARIO_H
#define SCENARIO_H

typedef struct
{
    SDL_Rect rect;
} ScenarioElementInfo;

ScenarioElementInfo createScenarioElement(int x, int y, int w, int h);
void drawExplosion(SDL_Renderer* renderer, int x, int y);

#endif /* SCENARIO_H */