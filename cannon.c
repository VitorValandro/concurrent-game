#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdbool.h>
#include <pthread.h>
#include <semaphore.h>
#include "cannon.h"
#include "helicopter.h"

extern int CANNON_SPEED;
extern int MAX_MISSILES;
extern int AMMUNITION;
extern int BUILDING_WIDTH;
extern int CANNON_WIDTH;
extern int COOLDOWN_TIME;
extern int SCREEN_WIDTH;
extern int BRIDGE_WIDTH;
extern int MISSILE_WIDTH;
extern int MISSILE_HEIGHT;
extern int MISSILE_SPEED;

extern pthread_mutex_t cannonMutex;

// Função pra criar um objeto
CannonInfo createCannon(int x, int y, int w, int h) {
    CannonInfo cannonInfo;
    cannonInfo.rect.x = x;
    cannonInfo.rect.y = y;
    cannonInfo.rect.w = w;
    cannonInfo.rect.h = h;
    cannonInfo.speed = CANNON_SPEED;
    cannonInfo.lastShotTime = SDL_GetTicks();
    cannonInfo.numActiveMissiles = 0;
    cannonInfo.missiles = (MissileInfo *)malloc(sizeof(MissileInfo) * MAX_MISSILES);
    cannonInfo.ammunition = AMMUNITION;

    sem_t sem_empty, ammo_sem;
    sem_init(&sem_empty, 0, 0);
    sem_init(&ammo_sem, 0, 1);

    cannonInfo.ammunition_semaphore_empty = sem_empty;
    cannonInfo.ammunition_semaphore = ammo_sem;

    pthread_mutex_t lock;
    cannonInfo.reloadingLock = lock;

    return cannonInfo;
}

// Função concorrente para mover a posição lógica dos canhões
void* moveCannon(void* arg) {
    MoveCannonThreadParams* params = (MoveCannonThreadParams*)arg;
    CannonInfo* cannonInfo = params->cannonInfo;
    HelicopterInfo* helicopterInfo = params->helicopterInfo;

    while (1) {
        if (cannonInfo->ammunition == 0)
        {
            pthread_mutex_lock(&cannonMutex);
            if (cannonInfo->rect.x < BUILDING_WIDTH - CANNON_WIDTH * 1.5)
            {
                sem_post(&cannonInfo->ammunition_semaphore_empty);
                sem_wait(&cannonInfo->ammunition_semaphore);
            }
            else {
                cannonInfo->rect.x -= abs(cannonInfo->speed);
            }
            pthread_mutex_unlock(&cannonMutex);
        }

        else {
            Uint32 currentTime = SDL_GetTicks();
            if (currentTime - cannonInfo->lastShotTime >= COOLDOWN_TIME) {
                createMissile(cannonInfo, helicopterInfo);
                cannonInfo->lastShotTime = currentTime;
            }

            // Atualiza a posição do canhão
            cannonInfo->rect.x += cannonInfo->speed;

            // Se o canhão alcançar o limite da tela, volta
            if (cannonInfo->rect.x + CANNON_WIDTH > SCREEN_WIDTH) {
                cannonInfo->speed = -CANNON_SPEED;
            }
            else if (cannonInfo->rect.x < BUILDING_WIDTH + BRIDGE_WIDTH)
            {
                cannonInfo->speed = CANNON_SPEED;
            }
        }  

        // Espera 10ms pra controlar a velocidade
        SDL_Delay(10);
    }

    return NULL;
}

void* reloadCannonAmmunition(void* arg) {
    MoveCannonThreadParams* params = (MoveCannonThreadParams*)arg;
    CannonInfo* cannonInfo = params->cannonInfo;
    HelicopterInfo* helicopterInfo = params->helicopterInfo;

    while(1) {
        sem_wait(&cannonInfo->ammunition_semaphore_empty);

        if (cannonInfo->ammunition == 0) {
            for (int i = 0; i < AMMUNITION; i++)
            {
                printf("Recarregando munição do canhão: %d\n", i);
                SDL_Delay(10);
            }

            cannonInfo->ammunition = AMMUNITION;
        }

        cannonInfo->numActiveMissiles = 0;
        helicopterInfo->num_missile_collision_rects = 0;

        sem_post(&cannonInfo->ammunition_semaphore);
    }
}

// Função pra criar um míssil
void createMissile(CannonInfo* cannon, HelicopterInfo* helicopter) {
    if (cannon->ammunition == 0) {
        return;
    }

    // Initialize the new missile
    MissileInfo *missile = &cannon->missiles[cannon->numActiveMissiles];
    missile->rect.w = MISSILE_WIDTH;
    missile->rect.h = MISSILE_HEIGHT;
    missile->rect.x = cannon->rect.x + (CANNON_WIDTH - MISSILE_WIDTH) / 2; 
    missile->rect.y = cannon->rect.y;
    missile->speed = MISSILE_SPEED;
    missile->active = true;
    missile->angle = ((rand() % 120) * M_PI / 180.0);

    pthread_t newThread;
    pthread_create(&newThread, NULL, moveMissiles, &cannon->missiles[cannon->numActiveMissiles]);
    missile->thread = newThread;

    addHelicopterCollisionMissile(helicopter, &cannon->missiles[cannon->numActiveMissiles]);

    cannon->numActiveMissiles++;
    cannon->ammunition--;
    printf("Created new missile: %d\n", cannon->ammunition);
}
