#include <SDL2/SDL.h>
#include <stdio.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

// Constantes
const int SCREEN_WIDTH = 800;
const int SCREEN_HEIGHT = 600;
const int CANNON_WIDTH = 100;
const int CANNON_HEIGHT = 50;
const int HELICOPTER_WIDTH = 120;
const int HELICOPTER_HEIGHT = 50;
const int MISSILE_WIDTH = 5;
const int MISSILE_HEIGHT = 15;
const int CANNON_SPEED = 3;
const int HELICOPTER_SPEED = 3;
const int MISSILE_SPEED = 5;
const int COOLDOWN_TIME = 2000;
const int MAX_MISSILES = 10;

// Mutex para controlar o acesso à renderização da tela
pthread_mutex_t renderMutex = PTHREAD_MUTEX_INITIALIZER;
// Mutex para controlar as posições dos canhões
pthread_mutex_t cannonMutex = PTHREAD_MUTEX_INITIALIZER;

// Guarda as informações dos mísseis
typedef struct {
    SDL_Rect rect;
    int speed;
    bool active;
    double angle;
    pthread_t thread;
} MissileInfo;

// Guarda as informações dos objetos
typedef struct {
    SDL_Rect rect;
    int speed;
    Uint32 lastShotTime;
    MissileInfo *missiles;
    int numMissiles;
} CannonInfo;

typedef struct {
    SDL_Rect rect;
    int speed;
} HelicopterInfo;

// Função pra criar um objeto
CannonInfo createCannon(int x, int y, int w, int h, int speed) {
    CannonInfo cannonInfo;
    cannonInfo.rect.x = x;
    cannonInfo.rect.y = y;
    cannonInfo.rect.w = w;
    cannonInfo.rect.h = h;
    cannonInfo.speed = speed;
    cannonInfo.lastShotTime = SDL_GetTicks();
    cannonInfo.numMissiles = 0;
    cannonInfo.missiles = (MissileInfo *)malloc(sizeof(MissileInfo) * MAX_MISSILES);

    printf("Number of missiles: %d\n", cannonInfo.numMissiles);
    return cannonInfo;
}

// Função pra criar um objeto
HelicopterInfo createHelicopter(int x, int y, int w, int h, int speed) {
    HelicopterInfo helicopterInfo;
    helicopterInfo.rect.x = x;
    helicopterInfo.rect.y = y;
    helicopterInfo.rect.w = w;
    helicopterInfo.rect.h = h;
    helicopterInfo.speed = speed;
    return helicopterInfo;
}

// Função concorrente para mover os mísseis
void* moveMissiles(void* arg) {
    MissileInfo* missileInfo = (MissileInfo*)arg;

    while (1) {
        Uint32 currentTime = SDL_GetTicks();

        if (missileInfo->active) {
            // Atualiza as posições lógicas do míssil se estiver ativo
            missileInfo->rect.x += (int)(missileInfo->speed * cos(missileInfo->angle));
            missileInfo->rect.y -= (int)(missileInfo->speed * sin(missileInfo->angle));

            // Desativa o míssil se ele sair da tela
            if (missileInfo->rect.x < 0 || missileInfo->rect.x > SCREEN_WIDTH || missileInfo->rect.y < 0 || missileInfo->rect.y > SCREEN_HEIGHT) {
                missileInfo->active = false;
            }
        }

        SDL_Delay(10);
    }

    return NULL;
}

// Função pra criar um míssil
void createMissile(CannonInfo* cannon) {
    if (cannon->numMissiles >= MAX_MISSILES) {
        return;
    }

    // Initialize the new missile
    MissileInfo *missile = &cannon->missiles[cannon->numMissiles];
    missile->rect.w = MISSILE_WIDTH;
    missile->rect.h = MISSILE_HEIGHT;
    missile->rect.x = cannon->rect.x + (CANNON_WIDTH - MISSILE_WIDTH) / 2; 
    missile->rect.y = cannon->rect.y;
    missile->speed = MISSILE_SPEED;
    missile->active = true;
    missile->angle = ((rand() % 160) * M_PI / 180.0);

    pthread_t newThread;
    pthread_create(&newThread, NULL, moveMissiles, &cannon->missiles[cannon->numMissiles]);
    missile->thread = newThread;

    cannon->numMissiles++;
    printf("Created new missile: %d\nthread: %lu", cannon->numMissiles, missile->thread);
}

// Função concorrente para mover a posição lógica dos canhões
void* moveCannon(void* arg) {
    CannonInfo* cannonInfo = (CannonInfo*)arg;

    while (1) {
        // Atualiza a posição do canhão
        cannonInfo->rect.x += cannonInfo->speed;

        Uint32 currentTime = SDL_GetTicks();

        if (cannonInfo->numMissiles < MAX_MISSILES && (currentTime - cannonInfo->lastShotTime >= COOLDOWN_TIME)) {
            createMissile(cannonInfo);
            cannonInfo->lastShotTime = currentTime;
        }

        // Se o canhão alcançar o limite da tela, reseta
        if (cannonInfo->rect.x > SCREEN_WIDTH) {
            cannonInfo->rect.x = -CANNON_WIDTH;
        } else if (cannonInfo->rect.x + CANNON_WIDTH < 0) {
            cannonInfo->rect.x = SCREEN_WIDTH;
        }

        // Espera 10ms pra controlar a velocidade
        SDL_Delay(10);
    }

    return NULL;
}

// Função concorrente para mover o helicóptero que é controlado pelo usuário
void* moveHelicopter(void* arg) {
    CannonInfo* helicopterRect = (CannonInfo*)arg;

    while (1) {
        const Uint8* keystates = SDL_GetKeyboardState(NULL);

        // Checa o estado atual do teclado pra ver se está pressionado
        if (keystates[SDL_SCANCODE_LEFT]) {
            helicopterRect->rect.x -= helicopterRect->speed;
        }
        if (keystates[SDL_SCANCODE_RIGHT]) {
            helicopterRect->rect.x += helicopterRect->speed;
        }
        if (keystates[SDL_SCANCODE_UP]) {
            helicopterRect->rect.y -= helicopterRect->speed;
        }
        if (keystates[SDL_SCANCODE_DOWN]) {
            helicopterRect->rect.y += helicopterRect->speed;
        }

        // Espera 10ms pra controlar a velocidade
        SDL_Delay(10);
    }

    return NULL;
}

// Função pra renderizar os objetos
// Isso não pode ser concorrente porque a tela que o usuário vê é uma zona de exclusão mútua
void render(SDL_Renderer* renderer, CannonInfo* cannon1Info, CannonInfo* cannon2Info, HelicopterInfo* helicopterInfo) {
    // Limpa a tela
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    // Desenha o canhão 1 na tela
    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255); // Green color
    SDL_RenderFillRect(renderer, &cannon1Info->rect);

    // Desenha o canhão 2 na tela
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255); // Red color
    SDL_RenderFillRect(renderer, &cannon2Info->rect);

    // Desenha o helicóptero
    SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255); // Blue color
    SDL_RenderFillRect(renderer, &helicopterInfo->rect);

    int cannon1Inactive = 0;
    int cannon2Inactive = 0;

    for (int i = 0; i < cannon1Info->numMissiles; i++)
    {
        if (cannon1Info->missiles[i].active)
        {
            SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
            SDL_RenderFillRect(renderer, &cannon1Info->missiles[i].rect);
        }
        
        else
        {
            pthread_cancel(cannon1Info->missiles[i].thread);
            cannon1Inactive++;
        }
    }

    for (int i = 0; i < cannon2Info->numMissiles; i++)
    {
        if (cannon2Info->missiles[i].active)
        {
            SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
            SDL_RenderFillRect(renderer, &cannon2Info->missiles[i].rect);
        }
       
        else 
        {
            pthread_cancel(cannon2Info->missiles[i].thread);
            cannon2Inactive++;
        }
    }

    cannon1Info->numMissiles -= cannon1Inactive;
    cannon2Info->numMissiles -= cannon2Inactive;

    // Atualiza a tela
    SDL_RenderPresent(renderer);
}

int main(int argc, char* argv[]) {
    // Inicializa o SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("Problema ao inicializar SDL. Erro: %s\n", SDL_GetError());
        return 1;
    }

    // Cria uma janela SDL
    SDL_Window* window = SDL_CreateWindow("Jogo Concorrente", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
    if (window == NULL) {
        printf("Não foi possível abrir a janela do SDL. Erro: %s\n", SDL_GetError());
        return 1;
    }

    // Cria um renderizador SDL
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (renderer == NULL) {
        printf("Renderizador do SDL não pôde ser criado. Erro: %s\n", SDL_GetError());
        return 1;
    }

    // Inicializa as threads
    pthread_t thread1, thread2, thread3, thread4, thread5;
    CannonInfo cannon1Info = createCannon(0, 350, CANNON_WIDTH, CANNON_HEIGHT, CANNON_SPEED);
    CannonInfo cannon2Info = createCannon(600, 350, CANNON_WIDTH, CANNON_HEIGHT, -CANNON_SPEED);
    HelicopterInfo helicopterInfo = createHelicopter(400, 300, HELICOPTER_WIDTH, HELICOPTER_HEIGHT, HELICOPTER_SPEED);

    pthread_create(&thread1, NULL, moveCannon, &cannon1Info);
    pthread_create(&thread2, NULL, moveCannon, &cannon2Info);
    pthread_create(&thread3, NULL, moveHelicopter, &helicopterInfo);

    srand(time(NULL)); // Seed pra gerar números aleatórios

    // Game loop
    int quit = 0;
    SDL_Event e;

    while (!quit) {
        // Escuta o evento pra fechar a tela do jogo
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                quit = 1;
            }
        }

        // Chama a função que renderiza na tela
        render(renderer, &cannon1Info, &cannon2Info, &helicopterInfo);
    }

    // Destrói as threads e a janela do SDL
    pthread_cancel(thread1);
    pthread_cancel(thread2);
    pthread_cancel(thread3);

    free(cannon1Info.missiles);
    free(cannon2Info.missiles);

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
