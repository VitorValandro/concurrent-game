#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <stdio.h>
#include "scenario.h"

extern int EXPLOSION_SIZE;
extern int HOSTAGE_WIDTH;
extern int HOSTAGE_HEIGHT;
extern int SCREEN_WIDTH;
extern int SCREEN_HEIGHT;
extern int BUILDING_HEIGHT;
extern int GROUND_HEIGHT;
extern int MARGIN_BETWEEN_HOSTAGES;

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
}

void drawHostages(SDL_Renderer* renderer, int capturedHostages, int rescuedHostages)
{
    SDL_Surface* image = IMG_Load("hostage_spritesheet.png");
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, image);

    // Desenha os reféns
    for (int i = 0; i < capturedHostages; i++)
    {
        SDL_Rect srcrect = {0, 0, 12, 20};
        SDL_Rect dstrect = {(HOSTAGE_WIDTH + MARGIN_BETWEEN_HOSTAGES) * i, SCREEN_HEIGHT - BUILDING_HEIGHT - GROUND_HEIGHT - HOSTAGE_HEIGHT, HOSTAGE_WIDTH, HOSTAGE_HEIGHT};
        SDL_RenderCopy(renderer, texture, &srcrect, &dstrect);
    }

    for (int i = 0; i < rescuedHostages; i++)
    {
        SDL_Rect srcrect = {0, 0, HOSTAGE_WIDTH, HOSTAGE_HEIGHT};
        SDL_Rect dstrect = {SCREEN_WIDTH - (HOSTAGE_WIDTH + MARGIN_BETWEEN_HOSTAGES) * (i + 1), SCREEN_HEIGHT - BUILDING_HEIGHT - GROUND_HEIGHT - HOSTAGE_HEIGHT, HOSTAGE_WIDTH, HOSTAGE_HEIGHT};
        SDL_RenderCopy(renderer, texture, &srcrect, &dstrect);
    }
}