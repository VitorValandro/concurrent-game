#include <SDL2/SDL.h>
#include <stdio.h>

#ifndef SCENARIO_H
#define SCENARIO_H

typedef struct
{
    SDL_Rect rect;
    SDL_Texture *texture;
} ScenarioElementInfo;

ScenarioElementInfo createScenarioElement(int x, int y, int w, int h);
void drawExplosion(SDL_Renderer* renderer, int x, int y);

void loadScenarioSpritesheet(SDL_Renderer* renderer, ScenarioElementInfo* scenarioElement, char* spritesheet);
void drawHostages(SDL_Renderer* renderer, int capturedHostages, int rescuedHostages);
void drawScenarioElement(SDL_Renderer* renderer, ScenarioElementInfo* scenarioElement);

#endif /* SCENARIO_H */