#include <SDL2/SDL.h>
#include <stdio.h>
#include <pthread.h>
#include <stdbool.h>

// Mutex para controlar o acesso à renderização da tela
pthread_mutex_t renderMutex = PTHREAD_MUTEX_INITIALIZER;

// Struct para segurar as informações dos objetos
typedef struct {
    SDL_Rect rect;
    int speed;
} RectangleInfo;

// Função para mover os canhões
void* moveCannon(void* arg) {
    RectangleInfo* rectInfo = (RectangleInfo*)arg;

    while (1) {
        // Atualiza a posição do canhão
        rectInfo->rect.x += rectInfo->speed;

        // Se o canhão chega no fim da tela, reseta pra posição inicial
        if (rectInfo->rect.x > 800) {
            rectInfo->rect.x = -rectInfo->rect.w;
        } else if (rectInfo->rect.x + rectInfo->rect.w < 0) {
            rectInfo->rect.x = 800;
        }

        // espera 10ms pra controlar a velocidade
        SDL_Delay(10);
    }

    return NULL;
}

// Função para mover o helicóptero controlado pelo usuário
void* moveHelicopter(void* arg) {
    RectangleInfo* helicopterRect = (RectangleInfo*)arg;

    while (1) {
        const Uint8* keystates = SDL_GetKeyboardState(NULL);

        // Checa o estado do teclado para ver se as arrow keys estão pressionadas
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

        // espera 10 ms pra controlar a velocidade
        SDL_Delay(10);
    }

    return NULL;
}

// Função pra renderizar os objetos
// Isso não pode ser concorrente porque pode causar race conditions
void render(SDL_Renderer* renderer, RectangleInfo* cannon1Info, RectangleInfo* cannon2Info, RectangleInfo* helicopterInfo) {
    // Limpa a tela
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    // Bloqueia o mutex porque atualização da tela tem exclusão mútua
    pthread_mutex_lock(&renderMutex);

    // Desenha o canhão 1
    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255); // Green color
    SDL_RenderFillRect(renderer, &cannon1Info->rect);

    // Desenha o canhão 2
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255); // Red color
    SDL_RenderFillRect(renderer, &cannon2Info->rect);

    // Desenha o helicóptero
    SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255); // Blue color
    SDL_RenderFillRect(renderer, &helicopterInfo->rect);

    // Desbloqueia o mutex depois de renderizar a tela
    pthread_mutex_unlock(&renderMutex);

    // Atualiza a tela com as novas informações
    SDL_RenderPresent(renderer);
}

// Função para criar um retangulo
RectangleInfo createRectangle(int x, int y, int w, int h, int speed) {
    RectangleInfo rectInfo;
    rectInfo.rect.x = x;
    rectInfo.rect.y = y;
    rectInfo.rect.w = w;
    rectInfo.rect.h = h;
    rectInfo.speed = speed;
    return rectInfo;
}

int main(int argc, char* argv[]) {
    // Inicializa a biblioteca SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("Problema ao inicializar SDL. Erro: %s\n", SDL_GetError());
        return 1;
    }

    // Cria uma janela SDL
    SDL_Window* window = SDL_CreateWindow("Jogo Concorrente", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 800, 600, SDL_WINDOW_SHOWN);
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
    pthread_t thread1, thread2, thread3;
    RectangleInfo cannon1Info = createRectangle(0, 350, 100, 50, 3);
    RectangleInfo cannon2Info = createRectangle(600, 350, 100, 50, -3);
    RectangleInfo helicopterInfo = createRectangle(400, 300, 50, 50, 3); // Rect_3 starts at the center

    pthread_create(&thread1, NULL, moveCannon, &cannon1Info);
    pthread_create(&thread2, NULL, moveCannon, &cannon2Info);
    pthread_create(&thread3, NULL, moveHelicopter, &helicopterInfo);

    // Loop do jogo
    int quit = 0;
    SDL_Event e;

    while (!quit) {
        // Checa se o usuário clicou no botão de fechar a tela
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                quit = 1;
            }
        }

        // Chama a função pra renderizar
        render(renderer, &cannon1Info, &cannon2Info, &helicopterInfo);
    }

    // Destrói as threads e a tela do SDL
    pthread_cancel(thread1);
    pthread_cancel(thread2);
    pthread_cancel(thread3);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
