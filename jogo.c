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
const int COOLDOWN_TIME = 3000;

// Mutex para controlar o acesso à renderização da tela
pthread_mutex_t renderMutex = PTHREAD_MUTEX_INITIALIZER;
// Mutex para controlar as posições dos canhões
pthread_mutex_t cannonMutex = PTHREAD_MUTEX_INITIALIZER;

// Guarda as informações dos objetos
typedef struct {
    SDL_Rect rect;
    int speed;
} RectangleInfo;

// Guarda as informações dos mísseis
typedef struct {
    SDL_Rect rect;
    int speed;
    bool active;
    Uint32 lastShotTime;
    double angle;
    RectangleInfo* cannon;
} MissileInfo;

// Função concorrente para mover a posição lógica dos canhões
void* moveCannon(void* arg) {
    RectangleInfo* rectInfo = (RectangleInfo*)arg;

    while (1) {

        // Bloqueia o mutex do canhão pra evitar race condition com o míssil
        pthread_mutex_lock(&cannonMutex);
        // Atualiza a posição do canhão
        rectInfo->rect.x += rectInfo->speed;

        // Se o canhão alcançar o limite da tela, reseta
        if (rectInfo->rect.x > SCREEN_WIDTH) {
            rectInfo->rect.x = -CANNON_WIDTH;
        } else if (rectInfo->rect.x + CANNON_WIDTH < 0) {
            rectInfo->rect.x = SCREEN_WIDTH;
        }

        // Desbloqueia a mutex depois de usar as informações dos canhões
        pthread_mutex_unlock(&cannonMutex);

        // Espera 10ms pra controlar a velocidade
        SDL_Delay(10);
    }

    return NULL;
}

// Função concorrente para mover o helicóptero que é controlado pelo usuário
void* moveHelicopter(void* arg) {
    RectangleInfo* helicopterRect = (RectangleInfo*)arg;

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

// Função concorrente para mover os mísseis
void* moveMissiles(void* arg) {
    MissileInfo* missileInfo = (MissileInfo*)arg;

    while (1) {
        Uint32 currentTime = SDL_GetTicks();

        if (!missileInfo->active && (currentTime - missileInfo->lastShotTime >= COOLDOWN_TIME)) {
            // Gera um ângulo aleatório para lançar o míssil
            missileInfo->angle = ((rand() % 160) * M_PI / 180.0); // calcula o angulo em radianos

            // Bloqueia o mutex do canhão pra evitar race condition com o míssil
            pthread_mutex_lock(&cannonMutex);

            // Míssil parte do meio do topo do canhão
            missileInfo->rect.x = missileInfo->cannon->rect.x + (CANNON_WIDTH - MISSILE_WIDTH) / 2; 
            missileInfo->rect.y = missileInfo->cannon->rect.y;

            // Desbloqueia a mutex depois de usar as informações dos canhões
            pthread_mutex_unlock(&cannonMutex);

            missileInfo->active = true;
            missileInfo->lastShotTime = currentTime;
        }

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

// Função pra renderizar os objetos
// Isso não pode ser concorrente porque a tela que o usuário vê é uma zona de exclusão mútua
void render(SDL_Renderer* renderer, RectangleInfo* cannon1Info, RectangleInfo* cannon2Info, RectangleInfo* helicopterInfo, MissileInfo* missile1Info, MissileInfo* missile2Info) {
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

    // Desenha o míssil 1
    if (missile1Info->active) {
        SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255); // Yellow color
        SDL_RenderFillRect(renderer, &missile1Info->rect);
    }

    // Desenha o míssil 2
    if (missile2Info->active) {
        SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255); // Yellow color
        SDL_RenderFillRect(renderer, &missile2Info->rect);
    }

    // Atualiza a tela
    SDL_RenderPresent(renderer);
}

// Função pra criar um objeto
RectangleInfo createRectangle(int x, int y, int w, int h, int speed) {
    RectangleInfo rectInfo;
    rectInfo.rect.x = x;
    rectInfo.rect.y = y;
    rectInfo.rect.w = w;
    rectInfo.rect.h = h;
    rectInfo.speed = speed;
    return rectInfo;
}

// Função pra criar um míssil
MissileInfo createMissile(RectangleInfo* cannon) {
    MissileInfo missileInfo;
    missileInfo.rect.w = MISSILE_WIDTH;
    missileInfo.rect.h = MISSILE_HEIGHT;
    missileInfo.speed = MISSILE_SPEED;
    missileInfo.active = false;
    missileInfo.lastShotTime = 0;
    missileInfo.angle = 0.0;
    missileInfo.cannon = cannon;
    return missileInfo;
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
    RectangleInfo cannon1Info = createRectangle(0, 350, CANNON_WIDTH, CANNON_HEIGHT, CANNON_SPEED);
    RectangleInfo cannon2Info = createRectangle(600, 350, CANNON_WIDTH, CANNON_HEIGHT, -CANNON_SPEED);
    RectangleInfo helicopterInfo = createRectangle(400, 300, HELICOPTER_WIDTH, HELICOPTER_HEIGHT, HELICOPTER_SPEED);

    pthread_create(&thread1, NULL, moveCannon, &cannon1Info);
    pthread_create(&thread2, NULL, moveCannon, &cannon2Info);
    pthread_create(&thread3, NULL, moveHelicopter, &helicopterInfo);

    // Inicializa os mísseis
    srand(time(NULL)); // Seed pra gerar números aleatórios
    MissileInfo missile1Info = createMissile(&cannon1Info);
    MissileInfo missile2Info = createMissile(&cannon2Info);

    pthread_create(&thread4, NULL, moveMissiles, &missile1Info);
    pthread_create(&thread5, NULL, moveMissiles, &missile2Info);

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
        render(renderer, &cannon1Info, &cannon2Info, &helicopterInfo, &missile1Info, &missile2Info);
    }

    // Destrói as threads e a janela do SDL
    pthread_cancel(thread1);
    pthread_cancel(thread2);
    pthread_cancel(thread3);
    pthread_cancel(thread4);
    pthread_cancel(thread5);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
