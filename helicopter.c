#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <stdio.h>
#include <pthread.h>
#include "helicopter.h"
#include "scenario.h"

extern int currentHostages;
extern int rescuedHostages;
extern int HELICOPTER_WIDTH;
extern int BUILDING_WIDTH;
extern int NUM_HOSTAGES;
extern int SCREEN_WIDTH;
extern int BUILDING_WIDTH;
extern int SCREEN_HEIGHT;
extern int AMMUNITION;
extern ScenarioElementInfo rightBuilding;
extern ScenarioElementInfo leftBuilding;
extern bool destroyed;

// Função pra criar um helicótero
HelicopterInfo createHelicopter(int x, int y, int w, int h, int speed, SDL_Rect **collisionRectArray)
{
    HelicopterInfo helicopterInfo;
    helicopterInfo.rect.x = x;
    helicopterInfo.rect.y = y;
    helicopterInfo.rect.w = w;
    helicopterInfo.rect.h = h;
    helicopterInfo.speed = speed;
    helicopterInfo.fixed_collision_rects = collisionRectArray;
    helicopterInfo.missile_collision_rects = (MissileInfo **)malloc(sizeof(MissileInfo *) * AMMUNITION);
    helicopterInfo.num_missile_collision_rects = 0;
    helicopterInfo.transportingHostage = false;
    helicopterInfo.currentMovement = 0;
    return helicopterInfo;
}

void addHelicopterCollisionMissile(HelicopterInfo *helicopter, MissileInfo *missile)
{
    helicopter->missile_collision_rects[helicopter->num_missile_collision_rects] = missile;
    helicopter->num_missile_collision_rects++;
}

void checkMissileCollisions(SDL_Rect helicopterRect, MissileInfo *missiles[], int missiles_length)
{
    for (int i = 0; i < missiles_length; i++)
    {
        if (&missiles[i]->active)
        {
            SDL_Rect *collisionRect = &missiles[i]->rect;
            if (SDL_HasIntersection(&helicopterRect, collisionRect))
            {
                destroyed = true;
            }
        }
    }
}

void checkHelicopterCollisions(SDL_Rect helicopterRect, SDL_Rect *rects[], int rects_length)
{
    if (
        helicopterRect.x < -(helicopterRect.w * 0.2) ||
        helicopterRect.x + helicopterRect.w > SCREEN_WIDTH + (helicopterRect.w * 0.2) ||
        helicopterRect.y < -(helicopterRect.h * 0.2) ||
        helicopterRect.y > SCREEN_HEIGHT + (helicopterRect.h * 0.2)
    ) {
        destroyed = true;
    }

    for (int i = 0; i < rects_length; i++)
    {
        SDL_Rect *collisionRect = rects[i];
        if (SDL_HasIntersection(&helicopterRect, collisionRect))
        {
            destroyed = true;
        }
    }
}

// Função concorrente para mover o helicóptero que é controlado pelo usuário
void *moveHelicopter(void *arg)
{
    HelicopterInfo *helicopterInfo = (HelicopterInfo *)arg;

    while (1)
    {
        helicopterInfo->currentMovement = 0;
        const Uint8 *keystates = SDL_GetKeyboardState(NULL);

        // Checa o estado atual do teclado pra ver se está pressionado
        if (keystates[SDL_SCANCODE_LEFT])
        {
            helicopterInfo->rect.x -= helicopterInfo->speed;
            helicopterInfo->currentMovement = 1;
        }
        if (keystates[SDL_SCANCODE_RIGHT])
        {
            helicopterInfo->rect.x += helicopterInfo->speed;
            helicopterInfo->currentMovement = 2;
        }
        if (keystates[SDL_SCANCODE_UP])
        {
            helicopterInfo->rect.y -= helicopterInfo->speed;
        }
        if (keystates[SDL_SCANCODE_DOWN])
        {
            helicopterInfo->rect.y += helicopterInfo->speed;
        }

        // checa colisão com canhões e objetos do cenário
        checkHelicopterCollisions(
            helicopterInfo->rect,
            helicopterInfo->fixed_collision_rects,
            5);

        // checa colisão com os mísseis
        checkMissileCollisions(
            helicopterInfo->rect,
            helicopterInfo->missile_collision_rects,
            helicopterInfo->num_missile_collision_rects);

        // se está no topo do prédio esquerdo e ainda há reféns, inicia o transporte do refém
        if (currentHostages > 0 && helicopterInfo->rect.x + HELICOPTER_WIDTH < BUILDING_WIDTH && !helicopterInfo->transportingHostage)
        {
            helicopterInfo->transportingHostage = true;
            currentHostages--;
        }

        // se está no topo do prédio à direita e está transportando um refém, finaliza o resgate
        if (rescuedHostages < NUM_HOSTAGES && helicopterInfo->rect.x > SCREEN_WIDTH - BUILDING_WIDTH && helicopterInfo->transportingHostage)
        {
            helicopterInfo->transportingHostage = false;
            rescuedHostages++;
        }

        // Espera 10ms pra controlar a velocidade
        SDL_Delay(10);
    }

    return NULL;
}

// Função concorrente para mover os mísseis
void *moveMissiles(void *arg)
{
    MissileInfo *missileInfo = (MissileInfo *)arg;

    while (1)
    {

        if (missileInfo->active)
        {
            // Atualiza as posições lógicas do míssil se estiver ativo
            missileInfo->rect.x += (int)(missileInfo->speed * cos(missileInfo->angle));
            missileInfo->rect.y -= (int)(missileInfo->speed * sin(missileInfo->angle));

            // Desativa o míssil se ele sair da tela
            if (
                missileInfo->rect.x < 0 ||
                missileInfo->rect.x > SCREEN_WIDTH ||
                missileInfo->rect.y < 0 ||
                missileInfo->rect.y > SCREEN_HEIGHT ||
                SDL_HasIntersection(&missileInfo->rect, &rightBuilding.rect) ||
                SDL_HasIntersection(&missileInfo->rect, &leftBuilding.rect))
            {
                missileInfo->active = false;

                // se o míssil não estiver mais ativo, destrói sua thread
                pthread_cancel(missileInfo->thread);
            }
        }

        SDL_Delay(10);
    }

    return NULL;
}

void loadHelicopterSprite(HelicopterInfo *helicopter, SDL_Renderer* renderer) {
    SDL_Surface * image = IMG_Load("sprites/helicopter_spritesheet.png");
    helicopter->texture = SDL_CreateTextureFromSurface(renderer, image);
}

void drawHelicopter(HelicopterInfo *helicopter, SDL_Renderer* renderer) {	
    Uint32 ticks = SDL_GetTicks();
    Uint32 ms = ticks / 200;
    
    int angleWhenMoving = 15;
    int angleDirection = 0;

    SDL_RendererFlip helicopterHorizontalDirection;
    if (helicopter->currentMovement == 1) {
        helicopterHorizontalDirection = SDL_FLIP_HORIZONTAL;
        angleDirection = 345;
    }
    else if (helicopter->currentMovement == 2) {
        helicopterHorizontalDirection = SDL_FLIP_NONE;
        angleDirection = 15;
    }

    SDL_Rect srcrect = {(ms % 4) * 100, helicopter->transportingHostage * 50, 100, 50};
    SDL_RenderCopyEx(renderer, helicopter->texture, &srcrect, &helicopter->rect, angleDirection, NULL, helicopterHorizontalDirection);
}