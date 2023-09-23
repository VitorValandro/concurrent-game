#include <SDL2/SDL.h>
#include <stdio.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <semaphore.h>

// Constantes
const int SCREEN_WIDTH = 900;
const int SCREEN_HEIGHT = 600;
const int LEFT_BUILDING_WIDTH = 150;
const int BRIDGE_WIDTH = 100;
const int BRIDGE_HEIGHT = 100;
const int CANNON_WIDTH = 50;
const int CANNON_HEIGHT = 25;
const int HELICOPTER_WIDTH = 90;
const int HELICOPTER_HEIGHT = 30;
const int MISSILE_WIDTH = 5;
const int MISSILE_HEIGHT = 15;
const int CANNON_SPEED = 2;
const int HELICOPTER_SPEED = 3;
const int MISSILE_SPEED = 5;
const int COOLDOWN_TIME = 2000;
const int MAX_MISSILES = 5;
const int AMMUNITION = MAX_MISSILES;

// Mutex para controlar o acesso às posições dos mísseis
pthread_mutex_t missileMutex = PTHREAD_MUTEX_INITIALIZER;

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
    int numActiveMissiles;
    int ammunition;

    sem_t ammunition_semaphore_empty;
    sem_t ammunition_semaphore;
    pthread_mutex_t reloadingLock;
} CannonInfo;

typedef struct {
    SDL_Rect rect;
    int speed;
    SDL_Rect** fixed_collision_rects;
    MissileInfo** missile_collision_rects;
    int num_missile_collision_rects;
} HelicopterInfo;

typedef struct {
    HelicopterInfo* helicopterInfo;
    CannonInfo* cannonInfo;
} MoveCannonThreadParams;

typedef struct {
    SDL_Rect rect;
} BridgeInfo;

// Função pra criar um objeto
BridgeInfo createBridge(int x, int y, int w, int h) {
    BridgeInfo bridgeInfo;
    bridgeInfo.rect.x = x;
    bridgeInfo.rect.y = y;
    bridgeInfo.rect.w = w;
    bridgeInfo.rect.h = h;
    return bridgeInfo;
}

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

// Função pra criar um objeto
HelicopterInfo createHelicopter(int x, int y, int w, int h, int speed, SDL_Rect** collisionRectArray) {
    HelicopterInfo helicopterInfo;
    helicopterInfo.rect.x = x;
    helicopterInfo.rect.y = y;
    helicopterInfo.rect.w = w;
    helicopterInfo.rect.h = h;
    helicopterInfo.speed = speed;
    helicopterInfo.fixed_collision_rects = collisionRectArray;
    helicopterInfo.missile_collision_rects = (MissileInfo **)malloc(sizeof(MissileInfo *) * 20);
    helicopterInfo.num_missile_collision_rects = 0;
    return helicopterInfo;
}

void addHelicopterCollisionMissile(HelicopterInfo* helicopter, MissileInfo* missile) {
    helicopter->missile_collision_rects[helicopter->num_missile_collision_rects] = missile;
    helicopter->num_missile_collision_rects++;
}

// Função concorrente para mover os mísseis
void* moveMissiles(void* arg) {
    MissileInfo* missileInfo = (MissileInfo*)arg;

    while (1) {
        Uint32 currentTime = SDL_GetTicks();

        pthread_mutex_lock(&missileMutex);

        if (missileInfo->active) {
            // Atualiza as posições lógicas do míssil se estiver ativo
            missileInfo->rect.x += (int)(missileInfo->speed * cos(missileInfo->angle));
            missileInfo->rect.y -= (int)(missileInfo->speed * sin(missileInfo->angle));

            // Desativa o míssil se ele sair da tela
            if (missileInfo->rect.x < 0 || missileInfo->rect.x > SCREEN_WIDTH || missileInfo->rect.y < 0 || missileInfo->rect.y > SCREEN_HEIGHT) {
                missileInfo->active = false;
            }
        }

        pthread_mutex_unlock(&missileMutex);

        SDL_Delay(10);
    }

    return NULL;
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

// Função concorrente para mover a posição lógica dos canhões
void* moveCannon(void* arg) {
    MoveCannonThreadParams* params = (MoveCannonThreadParams*)arg;
    CannonInfo* cannonInfo = params->cannonInfo;
    HelicopterInfo* helicopterInfo = params->helicopterInfo;

    while (1) {
        if (cannonInfo->ammunition == 0)
        {
            if (cannonInfo->rect.x < LEFT_BUILDING_WIDTH - CANNON_WIDTH * 1.5)
            {
                sem_post(&cannonInfo->ammunition_semaphore_empty);
                sem_wait(&cannonInfo->ammunition_semaphore);
            }
            else {
                cannonInfo->rect.x -= abs(cannonInfo->speed);
            }
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
            else if (cannonInfo->rect.x < LEFT_BUILDING_WIDTH + BRIDGE_WIDTH)
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
     CannonInfo* cannonInfo = (CannonInfo*)arg;

    while(1) {
        sem_wait(&cannonInfo->ammunition_semaphore_empty);

        if (cannonInfo->ammunition == 0) {
            for (int i = 0; i < AMMUNITION; i++)
            {
                printf("Recarregando munição do canhão: %d\n", i);
                SDL_Delay(1000);
            }

            cannonInfo->ammunition = AMMUNITION;
        }

        sem_post(&cannonInfo->ammunition_semaphore);
    }
}

void checkMissileCollisions(SDL_Rect helicopterRect, MissileInfo* missiles[], int missiles_length) {
    // Bloqueia o mutex porque atualização da tela tem exclusão mútua
    pthread_mutex_lock(&missileMutex);
    for (int i = 0; i < missiles_length; i++)
    {
        if (&missiles[i]->active) {
            SDL_Rect *collisionRect = &missiles[i]->rect;
            if (SDL_HasIntersection(&helicopterRect, collisionRect)) {   
                printf("Collision detected with missile %d\n", i);
            }
        }
    }
    pthread_mutex_unlock(&missileMutex);
}

void checkHelicopterCollisions(SDL_Rect helicopterRect, SDL_Rect* rects[], int rects_length) {
    for (int i = 0; i < rects_length; i++)
    {
        SDL_Rect *collisionRect = rects[i];
            if (SDL_HasIntersection(&helicopterRect, collisionRect)) {
                printf("Collision detected with rect %d\n", i);
            }
    }
}


// Função concorrente para mover o helicóptero que é controlado pelo usuário
void* moveHelicopter(void* arg) {
    HelicopterInfo* helicopterInfo = (HelicopterInfo*)arg;

    while (1) {
        const Uint8* keystates = SDL_GetKeyboardState(NULL);

        // Checa o estado atual do teclado pra ver se está pressionado
        if (keystates[SDL_SCANCODE_LEFT]) {
            helicopterInfo->rect.x -= helicopterInfo->speed;
        }
        if (keystates[SDL_SCANCODE_RIGHT]) {
            helicopterInfo->rect.x += helicopterInfo->speed;
        }
        if (keystates[SDL_SCANCODE_UP]) {
            helicopterInfo->rect.y -= helicopterInfo->speed;
        }
        if (keystates[SDL_SCANCODE_DOWN]) {
            helicopterInfo->rect.y += helicopterInfo->speed;
        }

        checkHelicopterCollisions(
            helicopterInfo->rect,
            helicopterInfo->fixed_collision_rects,
            2
        );

        checkMissileCollisions(
            helicopterInfo->rect,
            helicopterInfo->missile_collision_rects,
            helicopterInfo->num_missile_collision_rects
        );

        // Espera 10ms pra controlar a velocidade
        SDL_Delay(10);
    }

    return NULL;
}

// Função pra renderizar os objetos
// Isso não pode ser concorrente porque a tela que o usuário vê é uma zona de exclusão mútua
void render(SDL_Renderer* renderer, BridgeInfo* bridgeInfo, CannonInfo* cannon1Info, CannonInfo* cannon2Info, HelicopterInfo* helicopterInfo) {
    // Limpa a tela
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    // Desenha o canhão 1 na tela
    SDL_SetRenderDrawColor(renderer, 0, 165, 42, 42); // Green color
    SDL_RenderFillRect(renderer, &bridgeInfo->rect);

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

    for (int i = 0; i < cannon1Info->numActiveMissiles; i++) {
        if (cannon1Info->missiles[i].active) {
            SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
            SDL_RenderFillRect(renderer, &cannon1Info->missiles[i].rect);
        }
        
        else {
            pthread_cancel(cannon1Info->missiles[i].thread);
            cannon1Inactive++;
            helicopterInfo->num_missile_collision_rects--;
        }
    }

    for (int i = 0; i < cannon2Info->numActiveMissiles; i++) {
        if (cannon2Info->missiles[i].active){
            SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
            SDL_RenderFillRect(renderer, &cannon2Info->missiles[i].rect);
        }
       
        else {
            pthread_cancel(cannon2Info->missiles[i].thread);
            cannon2Inactive++;
            helicopterInfo->num_missile_collision_rects--;
        }
    }

    cannon1Info->numActiveMissiles -= cannon1Inactive;
    cannon2Info->numActiveMissiles -= cannon2Inactive;

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

    BridgeInfo bridgeInfo = createBridge(LEFT_BUILDING_WIDTH, SCREEN_HEIGHT - BRIDGE_HEIGHT, BRIDGE_WIDTH, BRIDGE_HEIGHT);

    CannonInfo cannon1Info = createCannon(LEFT_BUILDING_WIDTH + BRIDGE_WIDTH, SCREEN_HEIGHT - BRIDGE_HEIGHT - CANNON_HEIGHT, CANNON_WIDTH, CANNON_HEIGHT);
    CannonInfo cannon2Info = createCannon(LEFT_BUILDING_WIDTH + BRIDGE_WIDTH + CANNON_WIDTH * 1.2, SCREEN_HEIGHT - BRIDGE_HEIGHT - CANNON_HEIGHT, CANNON_WIDTH, CANNON_HEIGHT);

    SDL_Rect **rectArray = (SDL_Rect **)malloc(sizeof(SDL_Rect *) * 2);
    
    rectArray[0] = &cannon1Info.rect;
    rectArray[1] = &cannon2Info.rect;
    HelicopterInfo helicopterInfo = createHelicopter(400, 300, HELICOPTER_WIDTH, HELICOPTER_HEIGHT, HELICOPTER_SPEED, rectArray);

    MoveCannonThreadParams paramsCannon1;
    paramsCannon1.helicopterInfo = &helicopterInfo;
    paramsCannon1.cannonInfo = &cannon1Info;

    MoveCannonThreadParams paramsCannon2;
    paramsCannon2.helicopterInfo = &helicopterInfo;
    paramsCannon2.cannonInfo = &cannon2Info;

    // Inicializa as threads
    pthread_t thread_cannon1, thread_cannon2, thread_helicopter, thread_reload_cannon1, thread_reload_cannon2;
    pthread_create(&thread_cannon1, NULL, moveCannon, &paramsCannon1);
    pthread_create(&thread_cannon2, NULL, moveCannon, &paramsCannon2);
    pthread_create(&thread_helicopter, NULL, moveHelicopter, &helicopterInfo);
    pthread_create(&thread_reload_cannon1, NULL, reloadCannonAmmunition, &cannon1Info);
    pthread_create(&thread_reload_cannon2, NULL, reloadCannonAmmunition, &cannon2Info);

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
        render(renderer, &bridgeInfo, &cannon1Info, &cannon2Info, &helicopterInfo);
    }

    // Destrói as threads e a janela do SDL
    pthread_cancel(thread_cannon1);
    pthread_cancel(thread_cannon2);
    pthread_cancel(thread_helicopter);
    pthread_cancel(thread_reload_cannon1);
    pthread_cancel(thread_reload_cannon2);

    sem_destroy(&cannon1Info.ammunition_semaphore_empty);
    sem_destroy(&cannon2Info.ammunition_semaphore_empty);
    sem_destroy(&cannon1Info.ammunition_semaphore);
    sem_destroy(&cannon2Info.ammunition_semaphore);

    free(cannon1Info.missiles);
    free(cannon2Info.missiles);
    free(helicopterInfo.fixed_collision_rects);
    free(helicopterInfo.missile_collision_rects);

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
