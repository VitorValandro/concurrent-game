#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <stdio.h>
#include "scenario.h"

extern int EXPLOSION_SIZE;

// Função pra criar um objeto do cenário
ScenarioElementInfo createScenarioElement(int x, int y, int w, int h)
{
    ScenarioElementInfo rectInfo;
    rectInfo.rect.x = x;
    rectInfo.rect.y = y;
    rectInfo.rect.w = w;
    rectInfo.rect.h = h;
    return rectInfo;
}

void drawExplosion(SDL_Renderer* renderer, int x, int y)
{
    SDL_Surface* image = IMG_Load("explosion_spritesheet.png");
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, image);

    for (int i = 0; i < 4; i++)
    {
        SDL_Rect srcrect = {i * 32, 0, 32, 32};
        SDL_Rect dstrect = { x, y, EXPLOSION_SIZE, EXPLOSION_SIZE};
        SDL_RenderCopy(renderer, texture, &srcrect, &dstrect);
        SDL_RenderPresent(renderer);
        SDL_Delay(100);
    }

    return;
}