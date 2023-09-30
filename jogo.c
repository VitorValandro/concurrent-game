#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <stdio.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <semaphore.h>
#include "helicopter.h"
#include "cannon.h"
#include "scenario.h"

// Constantes
const int SCREEN_WIDTH = 1100;
const int SCREEN_HEIGHT = 700;
const int GROUND_HEIGHT = 100;
const int BUILDING_WIDTH = 175;
const int BUILDING_HEIGHT = 300;
const int BRIDGE_WIDTH = 150;
const int BRIDGE_HEIGHT = GROUND_HEIGHT;
const int CANNON_WIDTH = 100;
const int CANNON_HEIGHT = 50;
const int HELICOPTER_WIDTH = 150;
const int HELICOPTER_HEIGHT = 75;
const int MISSILE_WIDTH = 5;
const int MISSILE_HEIGHT = 15;
const int CANNON_SPEED = 2;
const int HELICOPTER_SPEED = 3;
const int MISSILE_SPEED = 5;
const int COOLDOWN_TIME = 500;
const int MAX_MISSILES = 10;
const int AMMUNITION = MAX_MISSILES;
const int NUM_HOSTAGES = 10;
const int HOSTAGES_WIDTH = 7;
const int HOSTAGES_HEIGHT = 14;
const int RELOAD_TIME_FOR_EACH_MISSILE = 10; // milisegundos

pthread_mutex_t bridgeMutex = PTHREAD_MUTEX_INITIALIZER;

int currentHostages = NUM_HOSTAGES;
int rescuedHostages = 0;

ScenarioElementInfo groundInfo;
ScenarioElementInfo bridgeInfo;
ScenarioElementInfo leftBuilding;
ScenarioElementInfo rightBuilding;

// Função pra renderizar os objetos
// Isso não pode ser concorrente porque a tela que o usuário vê é uma zona de exclusão mútua
void render(SDL_Renderer *renderer, CannonInfo *cannon1Info, CannonInfo *cannon2Info, HelicopterInfo *helicopterInfo)
{
    // Limpa a tela
    SDL_SetRenderDrawColor(renderer, 188, 170, 164, 255);
    SDL_RenderClear(renderer);

    // Desenha a plataforma da esquerda na tela
    SDL_SetRenderDrawColor(renderer, 223, 154, 87, 0); // Green color
    SDL_RenderFillRect(renderer, &leftBuilding.rect);

    // Desenha a plataforma da direita na tela
    SDL_SetRenderDrawColor(renderer, 223, 154, 87, 0);
    SDL_RenderFillRect(renderer, &rightBuilding.rect);

    // Desenha o chão na tela
    SDL_SetRenderDrawColor(renderer, 220, 204, 187, 0);
    SDL_RenderFillRect(renderer, &groundInfo.rect);

    // Desenha a ponte na tela
    SDL_SetRenderDrawColor(renderer, 94, 91, 82, 0);
    SDL_RenderFillRect(renderer, &bridgeInfo.rect);

    drawCannon(cannon1Info, renderer);
    drawCannon(cannon2Info, renderer);

    drawHelicopter(helicopterInfo, renderer);

    // Desenha os mísseis do canhão 1
    for (int i = 0; i < cannon1Info->numActiveMissiles; i++)
    {
        if (cannon1Info->missiles[i].active)
        {
            SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
            SDL_RenderFillRect(renderer, &cannon1Info->missiles[i].rect);
        }
    }

    // Desenha os mísseis do canhão 2
    for (int i = 0; i < cannon2Info->numActiveMissiles; i++)
    {
        if (cannon2Info->missiles[i].active)
        {
            SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
            SDL_RenderFillRect(renderer, &cannon2Info->missiles[i].rect);
        }
    }

    // Desenha os reféns
    for (int i = 0; i < currentHostages; i++)
    {
        SDL_SetRenderDrawColor(renderer, 252, 106, 3, 0);
        SDL_Rect hostageRect;
        hostageRect.w = HOSTAGES_WIDTH;
        hostageRect.h = HOSTAGES_HEIGHT;
        hostageRect.x = (HOSTAGES_WIDTH + 9) * (i + 1);
        hostageRect.y = SCREEN_HEIGHT - BUILDING_HEIGHT - GROUND_HEIGHT - HOSTAGES_HEIGHT;
        SDL_RenderFillRect(renderer, &hostageRect);
    }

    for (int i = 0; i < rescuedHostages; i++)
    {
        SDL_SetRenderDrawColor(renderer, 252, 106, 3, 0);
        SDL_Rect hostageRect;
        hostageRect.w = HOSTAGES_WIDTH;
        hostageRect.h = HOSTAGES_HEIGHT;
        hostageRect.x = SCREEN_WIDTH - (HOSTAGES_WIDTH + 9) * (i + 1);
        hostageRect.y = SCREEN_HEIGHT - BUILDING_HEIGHT - GROUND_HEIGHT - HOSTAGES_HEIGHT;
        SDL_RenderFillRect(renderer, &hostageRect);
    }

    // Atualiza a tela
    SDL_RenderPresent(renderer);
}

int main(int argc, char *argv[])
{
    // Inicializa o SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        printf("Problema ao inicializar SDL. Erro: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Init(SDL_INIT_VIDEO);

    // Cria uma janela SDL
    SDL_Window *window = SDL_CreateWindow("Jogo Concorrente", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
    if (window == NULL)
    {
        printf("Não foi possível abrir a janela do SDL. Erro: %s\n", SDL_GetError());
        return 1;
    }

    // Cria um renderizador SDL
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (renderer == NULL)
    {
        printf("Renderizador do SDL não pôde ser criado. Erro: %s\n", SDL_GetError());
        return 1;
    }

    // Cria os elementos do cenário
    groundInfo = createScenarioElement(0, SCREEN_HEIGHT - GROUND_HEIGHT, SCREEN_WIDTH, GROUND_HEIGHT);
    bridgeInfo = createScenarioElement(BUILDING_WIDTH, SCREEN_HEIGHT - BRIDGE_HEIGHT, BRIDGE_WIDTH, BRIDGE_HEIGHT);
    leftBuilding = createScenarioElement(0, SCREEN_HEIGHT - BUILDING_HEIGHT - GROUND_HEIGHT, BUILDING_WIDTH, BUILDING_HEIGHT);
    rightBuilding = createScenarioElement(SCREEN_WIDTH - BUILDING_WIDTH, SCREEN_HEIGHT - BUILDING_HEIGHT - GROUND_HEIGHT, BUILDING_WIDTH, BUILDING_HEIGHT);

    // Cria os canhões
    CannonInfo cannon1Info = createCannon(BUILDING_WIDTH + BRIDGE_WIDTH + CANNON_WIDTH, SCREEN_HEIGHT - BRIDGE_HEIGHT - CANNON_HEIGHT, CANNON_WIDTH, CANNON_HEIGHT);
    CannonInfo cannon2Info = createCannon(BUILDING_WIDTH + BRIDGE_WIDTH + CANNON_WIDTH * 4, SCREEN_HEIGHT - BRIDGE_HEIGHT - CANNON_HEIGHT, CANNON_WIDTH, CANNON_HEIGHT);
    
    loadCannonSprite(&cannon1Info, renderer);
    loadCannonSprite(&cannon2Info, renderer);

    // Inicializa o array de colisões para o helicóptero
    SDL_Rect **rectArray = (SDL_Rect **)malloc(sizeof(SDL_Rect *) * 5);

    rectArray[0] = &cannon1Info.rect;
    rectArray[1] = &cannon2Info.rect;
    rectArray[2] = &groundInfo.rect;
    rectArray[3] = &leftBuilding.rect;
    rectArray[4] = &rightBuilding.rect;

    HelicopterInfo helicopterInfo = createHelicopter(400, 300, HELICOPTER_WIDTH, HELICOPTER_HEIGHT, HELICOPTER_SPEED, rectArray);
    loadHelicopterSprite(&helicopterInfo, renderer);
   
    MoveCannonThreadParams paramsCannon1;
    paramsCannon1.helicopterInfo = &helicopterInfo;
    paramsCannon1.cannonInfo = &cannon1Info;

    MoveCannonThreadParams paramsCannon2;
    paramsCannon2.helicopterInfo = &helicopterInfo;
    paramsCannon2.cannonInfo = &cannon2Info;

    // Inicializa as threads
    pthread_t thread_cannon1, thread_cannon2, thread_helicopter, thread_reload_cannon1, thread_reload_cannon2;
    pthread_create(&thread_cannon1, NULL, moveCannon, &paramsCannon1);                    // thread do canhão 1
    pthread_create(&thread_cannon2, NULL, moveCannon, &paramsCannon2);                    // thread do canhão 2
    pthread_create(&thread_helicopter, NULL, moveHelicopter, &helicopterInfo);            // thread do helicóptero
    pthread_create(&thread_reload_cannon1, NULL, reloadCannonAmmunition, &paramsCannon1); // thread do depósito do canhão 1
    pthread_create(&thread_reload_cannon2, NULL, reloadCannonAmmunition, &paramsCannon2); // thread do depósito do canhão 2

    srand(time(NULL)); // Seed pra gerar números aleatórios usados no cálculo do ângulo do míssil

    // Game loop
    int quit = 0;
    SDL_Event e;

    while (!quit)
    {
        // Escuta o evento pra fechar a tela do jogo
        while (SDL_PollEvent(&e) != 0)
        {
            if (e.type == SDL_QUIT)
            {
                quit = 1;
            }
        }

        // Chama a função que renderiza na tela
        render(renderer, &cannon1Info, &cannon2Info, &helicopterInfo);
    }

    // Destrói as threads e a janela do SDL
    pthread_cancel(thread_cannon1);
    pthread_cancel(thread_cannon2);
    pthread_cancel(thread_helicopter);
    pthread_cancel(thread_reload_cannon1);
    pthread_cancel(thread_reload_cannon2);

    sem_destroy(&cannon1Info.ammunition_semaphore_empty);
    sem_destroy(&cannon2Info.ammunition_semaphore_empty);
    sem_destroy(&cannon1Info.ammunition_semaphore_full);
    sem_destroy(&cannon2Info.ammunition_semaphore_full);

    free(cannon1Info.missiles);
    free(cannon2Info.missiles);
    free(helicopterInfo.fixed_collision_rects);
    free(helicopterInfo.missile_collision_rects);

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
