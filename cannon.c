#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <stdio.h>
#include <stdbool.h>
#include <pthread.h>
#include <semaphore.h>
#include "cannon.h"
#include "helicopter.h"

extern int CANNON_SPEED;
extern int AMMUNITION;
extern int BUILDING_WIDTH;
extern int CANNON_WIDTH;
extern int MIN_COOLDOWN_TIME;
extern int MAX_COOLDOWN_TIME;
extern int SCREEN_WIDTH;
extern int BRIDGE_WIDTH;
extern int MISSILE_WIDTH;
extern int MISSILE_HEIGHT;
extern int MISSILE_SPEED;
extern int RELOAD_TIME_FOR_EACH_MISSILE;

extern pthread_mutex_t bridgeMutex;

// Função pra criar um canhão
CannonInfo createCannon(int x, int y, int w, int h, int initialAmmunition)
{
    CannonInfo cannonInfo;
    cannonInfo.rect.x = x;
    cannonInfo.rect.y = y;
    cannonInfo.rect.w = w;
    cannonInfo.rect.h = h;
    cannonInfo.speed = CANNON_SPEED;
    cannonInfo.lastShotTime = SDL_GetTicks();
    cannonInfo.numActiveMissiles = 0;
    cannonInfo.missiles = (MissileInfo *)malloc(sizeof(MissileInfo) * AMMUNITION);
    cannonInfo.ammunition = initialAmmunition;

    sem_t sem_empty, ammo_sem;
    sem_init(&sem_empty, 0, 0);
    sem_init(&ammo_sem, 0, 1);

    cannonInfo.ammunition_semaphore_empty = sem_empty;
    cannonInfo.ammunition_semaphore_full = ammo_sem;

    pthread_mutex_t lock;
    cannonInfo.reloadingLock = lock;

    return cannonInfo;
}

// Função concorrente para mover a posição lógica dos canhões
void *moveCannon(void *arg)
{
    MoveCannonThreadParams *params = (MoveCannonThreadParams *)arg;
    CannonInfo *cannonInfo = params->cannonInfo;
    HelicopterInfo *helicopterInfo = params->helicopterInfo;

    while (1)
    {
        // Verifica se o canhão está em cima da ponte
        if (cannonInfo->rect.x + CANNON_WIDTH > BUILDING_WIDTH && cannonInfo->rect.x < BUILDING_WIDTH + BRIDGE_WIDTH)
        {
            // se estiver, bloqueia a passagem na ponte para os demais canhões
            pthread_mutex_lock(&bridgeMutex);

            // enquanto estiver em cima da ponte, se desloca para sair dela enquanto outros canhões estão bloqueados
            while (
                cannonInfo->rect.x + CANNON_WIDTH > BUILDING_WIDTH &&
                cannonInfo->rect.x < BUILDING_WIDTH + BRIDGE_WIDTH)
            {
                // se estiver sem munição, se desloca em direção ao depósito (esquerda)
                if (cannonInfo->ammunition == 0)
                    cannonInfo->rect.x -= abs(cannonInfo->speed);
                else
                    cannonInfo->rect.x += abs(cannonInfo->speed);

                // Espera 10ms pra controlar a velocidade
                SDL_Delay(10);
            }

            // quando terminar a passagem pela ponte, libera o mutex da ponte
            pthread_mutex_unlock(&bridgeMutex);
        }

        if (cannonInfo->ammunition == 0)
        {
            // se está sem munição, desloca-se para o depósito
            if (cannonInfo->rect.x < BUILDING_WIDTH - CANNON_WIDTH)
            {
                // se está no depósito, libera o semáforo para a thread produtora de munições
                sem_post(&cannonInfo->ammunition_semaphore_empty);
                // espera até a munição ser recarregada
                sem_wait(&cannonInfo->ammunition_semaphore_full);
            }
            else
            {
                cannonInfo->rect.x -= abs(cannonInfo->speed);
            }
        }

        else
        {
            Uint32 currentTime = SDL_GetTicks();

            // gera um cooldown aleatório entre os limites
            int cooldown = rand() % (MAX_COOLDOWN_TIME + 1 - MIN_COOLDOWN_TIME) + MIN_COOLDOWN_TIME;

            // verifica se está na hora de disparar outro míssil
            if (currentTime - cannonInfo->lastShotTime >= cooldown)
            {
                createMissile(cannonInfo, helicopterInfo);
                cannonInfo->lastShotTime = currentTime;
            }

            // Atualiza a posição do canhão
            cannonInfo->rect.x += cannonInfo->speed;

            // Se o canhão alcançar os limites, inverte a direção
            if (cannonInfo->rect.x + CANNON_WIDTH > SCREEN_WIDTH - BUILDING_WIDTH)
                cannonInfo->speed = -CANNON_SPEED;
            else if (cannonInfo->rect.x <= BUILDING_WIDTH + BRIDGE_WIDTH)
                cannonInfo->speed = CANNON_SPEED;
        }

        // Espera 10ms pra controlar a velocidade
        SDL_Delay(10);
    }

    return NULL;
}

// thread para os depósitos produtores de munição
void *reloadCannonAmmunition(void *arg)
{
    MoveCannonThreadParams *params = (MoveCannonThreadParams *)arg;
    CannonInfo *cannonInfo = params->cannonInfo;
    HelicopterInfo *helicopterInfo = params->helicopterInfo;

    while (1)
    {
        // espera até sinalizar que a munição está vazia
        sem_wait(&cannonInfo->ammunition_semaphore_empty);

        if (cannonInfo->ammunition == 0)
        {
            for (int i = 0; i <= AMMUNITION; i++)
            {
                SDL_Delay(RELOAD_TIME_FOR_EACH_MISSILE);
                cannonInfo->ammunition += 1;
            }
        }

        // libera o array de threads dos mísseis ativos e de retângulos de colisão
        cannonInfo->numActiveMissiles = 0;
        helicopterInfo->num_missile_collision_rects = 0;

        // sinaliza que finalizou a produção da munição
        sem_post(&cannonInfo->ammunition_semaphore_full);
    }
}

// Função pra criar um míssil
void createMissile(CannonInfo *cannon, HelicopterInfo *helicopter)
{
    if (cannon->ammunition == 0)
    {
        return;
    }

    MissileInfo *missile = &cannon->missiles[cannon->numActiveMissiles];
    missile->rect.w = MISSILE_WIDTH;
    missile->rect.h = MISSILE_HEIGHT;
    missile->rect.x = cannon->rect.x + (CANNON_WIDTH - MISSILE_WIDTH) / 2;
    missile->rect.y = cannon->rect.y;
    missile->speed = MISSILE_SPEED;
    missile->active = true;
    missile->angle = ((rand() % 120) * M_PI / 180.0);

    // cria a thread desse míssil
    pthread_t newThread;
    pthread_create(&newThread, NULL, moveMissiles, &cannon->missiles[cannon->numActiveMissiles]);
    missile->thread = newThread;

    // adiciona o míssil no array de colisores do helicóptero
    addHelicopterCollisionMissile(helicopter, &cannon->missiles[cannon->numActiveMissiles]);

    cannon->numActiveMissiles++;
    cannon->ammunition--;
}

void loadCannonSprite(CannonInfo *cannon, SDL_Renderer* renderer) {
    SDL_Surface * image = IMG_Load("sprites/cannon_spritesheet.png");
    cannon->texture = SDL_CreateTextureFromSurface(renderer, image);
}

void drawCannon(CannonInfo *cannon, SDL_Renderer* renderer) {	
    Uint32 ticks = SDL_GetTicks();
    Uint32 ms = ticks / 200;
    
    SDL_Rect srcrect = {(ms % 3) * 50, 225 - ((cannon->ammunition * 9) / AMMUNITION) * 25, 50, 25 };
    SDL_RenderCopy(renderer, cannon->texture, &srcrect, &cannon->rect);
}