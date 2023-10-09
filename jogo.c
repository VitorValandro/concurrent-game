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
const int BUILDING_WIDTH = 200;
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
const int NUM_HOSTAGES = 10;
const int HOSTAGE_WIDTH = 15;
const int HOSTAGE_HEIGHT = 30;
const int MARGIN_BETWEEN_HOSTAGES = 5;
const int EXPLOSION_SIZE = 75;

int MIN_COOLDOWN_TIME = 1500;
int MAX_COOLDOWN_TIME = 4500;
int AMMUNITION = 10;
int RELOAD_TIME_FOR_EACH_MISSILE = 500; // milisegundos

pthread_mutex_t bridgeMutex = PTHREAD_MUTEX_INITIALIZER;

int currentHostages = NUM_HOSTAGES;
int rescuedHostages = 0;

bool destroyed = false;
bool gameover = false;

ScenarioElementInfo background;
ScenarioElementInfo groundInfo;
ScenarioElementInfo bridgeInfo;
ScenarioElementInfo leftBuilding;
ScenarioElementInfo rightBuilding;

// Função pra renderizar os objetos
// Isso não pode ser concorrente porque a tela que o usuário vê é uma zona de exclusão mútua
void render(SDL_Renderer *renderer, CannonInfo *cannon1Info, CannonInfo *cannon2Info, HelicopterInfo *helicopterInfo)
{
    // Limpa a tela
    SDL_RenderClear(renderer);
    
    drawScenarioElement(renderer, &background);
    drawScenarioElement(renderer, &leftBuilding);
    drawScenarioElement(renderer, &rightBuilding);
    drawScenarioElement(renderer, &groundInfo);
    drawScenarioElement(renderer, &bridgeInfo);

    drawCannon(cannon1Info, renderer);
    drawCannon(cannon2Info, renderer);

    // Desenha os mísseis do canhão 1
    for (int i = 0; i < cannon1Info->numActiveMissiles; i++)
    {
        if (cannon1Info->missiles[i].active)
        {
            SDL_SetRenderDrawColor(renderer, 255, 0, 0, 0);
            SDL_RenderFillRect(renderer, &cannon1Info->missiles[i].rect);
        }
    }

    // Desenha os mísseis do canhão 2
    for (int i = 0; i < cannon2Info->numActiveMissiles; i++)
    {
        if (cannon2Info->missiles[i].active)
        {
            SDL_SetRenderDrawColor(renderer, 255, 0, 0, 0);
            SDL_RenderFillRect(renderer, &cannon2Info->missiles[i].rect);
        }
    }

    drawHostages(renderer, currentHostages, rescuedHostages);

    if (destroyed) 
    {
        drawExplosion(
            renderer,
            helicopterInfo->rect.x + (helicopterInfo->rect.w / 2),
            helicopterInfo->rect.y + (helicopterInfo->rect.h / 2)
        );
        gameover = true;
    }
    else drawHelicopter(helicopterInfo, renderer);

    if (rescuedHostages == NUM_HOSTAGES)
    {
        gameover = true;
    }

    // Atualiza a tela
    SDL_RenderPresent(renderer);
}

int getDifficultyChoice() {
    int choice;

    printf("Escolha a dificuldade do jogo:\n");
    printf("1 - Fácil\n");
    printf("2 - Médio\n");
    printf("3 - Difícil\n");
    printf("Escolha digitando 1, 2 ou 3: ");

    while (1) {
        if (scanf("%d", &choice) != 1) {
            // Input is not a valid integer
            printf("Por favor, digite um número válido (1, 2 ou 3): ");
            while (getchar() != '\n'); // Clear input buffer
        } else if (choice < 1 || choice > 3) {
            // Input is out of range
            printf("Por favor, escolha 1, 2 ou 3: ");
        } else {
            // Valid input
            break;
        }
    }

    return choice;
}

int main(int argc, char *argv[])
{
    int difficulty = getDifficultyChoice();

    AMMUNITION = AMMUNITION * (0.5 * difficulty + 0.5);
    RELOAD_TIME_FOR_EACH_MISSILE = RELOAD_TIME_FOR_EACH_MISSILE / difficulty;
    MIN_COOLDOWN_TIME = MIN_COOLDOWN_TIME / difficulty;
    MAX_COOLDOWN_TIME = MAX_COOLDOWN_TIME / difficulty;

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
    background = createScenarioElement(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    groundInfo = createScenarioElement(0, SCREEN_HEIGHT - GROUND_HEIGHT, SCREEN_WIDTH, GROUND_HEIGHT);
    bridgeInfo = createScenarioElement(BUILDING_WIDTH, SCREEN_HEIGHT - BRIDGE_HEIGHT, BRIDGE_WIDTH, BRIDGE_HEIGHT);
    leftBuilding = createScenarioElement(0, SCREEN_HEIGHT - BUILDING_HEIGHT - GROUND_HEIGHT, BUILDING_WIDTH, BUILDING_HEIGHT);
    rightBuilding = createScenarioElement(SCREEN_WIDTH - BUILDING_WIDTH, SCREEN_HEIGHT - BUILDING_HEIGHT - GROUND_HEIGHT, BUILDING_WIDTH, BUILDING_HEIGHT);

    loadScenarioSpritesheet(renderer, &background, "sprites/background_spritesheet.png");
    loadScenarioSpritesheet(renderer, &leftBuilding, "sprites/left_building_spritesheet.png");
    loadScenarioSpritesheet(renderer, &rightBuilding, "sprites/right_building_spritesheet.png");
    loadScenarioSpritesheet(renderer, &groundInfo, "sprites/ground_spritesheet.png");
    loadScenarioSpritesheet(renderer, &bridgeInfo, "sprites/bridge_spritesheet.png");

    // Cria os canhões
    CannonInfo cannon1Info = createCannon(BUILDING_WIDTH + BRIDGE_WIDTH + CANNON_WIDTH * 2, SCREEN_HEIGHT - BRIDGE_HEIGHT - CANNON_HEIGHT, CANNON_WIDTH, CANNON_HEIGHT, 0);
    CannonInfo cannon2Info = createCannon(BUILDING_WIDTH + BRIDGE_WIDTH + CANNON_WIDTH, SCREEN_HEIGHT - BRIDGE_HEIGHT - CANNON_HEIGHT, CANNON_WIDTH, CANNON_HEIGHT, 0);
    
    loadCannonSprite(&cannon1Info, renderer);
    loadCannonSprite(&cannon2Info, renderer);

    // Inicializa o array de colisões para o helicóptero
    SDL_Rect **rectArray = (SDL_Rect **)malloc(sizeof(SDL_Rect *) * 5);

    rectArray[0] = &cannon1Info.rect;
    rectArray[1] = &cannon2Info.rect;
    rectArray[2] = &groundInfo.rect;
    rectArray[3] = &leftBuilding.rect;
    rectArray[4] = &rightBuilding.rect;

    HelicopterInfo helicopterInfo = createHelicopter(SCREEN_WIDTH - BUILDING_WIDTH, SCREEN_HEIGHT - BUILDING_HEIGHT - GROUND_HEIGHT - HELICOPTER_HEIGHT * 1.5, HELICOPTER_WIDTH, HELICOPTER_HEIGHT, HELICOPTER_SPEED, rectArray);
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

        if (!gameover) {
            // Chama a função que renderiza o jogo na tela
            render(renderer, &cannon1Info, &cannon2Info, &helicopterInfo);
        }
        else 
        {
            if (rescuedHostages == NUM_HOSTAGES) printf("Parabéns! Você resgatou todos os reféns e venceu o jogo!");
            else printf("Você perdeu! Seu helicóptero foi destruído e ainda restavam reféns a serem resgatados.");
            quit = 1;
        };
    }
    
    free(cannon1Info.missiles);
    free(cannon2Info.missiles);
    free(helicopterInfo.fixed_collision_rects);
    free(helicopterInfo.missile_collision_rects);

    // Destrói as threads
    pthread_cancel(thread_cannon1);
    pthread_cancel(thread_cannon2);
    pthread_cancel(thread_helicopter);
    pthread_cancel(thread_reload_cannon1);
    pthread_cancel(thread_reload_cannon2);

    sem_destroy(&cannon1Info.ammunition_semaphore_empty);
    sem_destroy(&cannon2Info.ammunition_semaphore_empty);
    sem_destroy(&cannon1Info.ammunition_semaphore_full);
    sem_destroy(&cannon2Info.ammunition_semaphore_full);

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
