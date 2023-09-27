#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdbool.h>
#include <pthread.h>

#ifndef HELICOPTER_H
#define HELICOPTER_H

// Guarda as informações dos mísseis
typedef struct
{
    SDL_Rect rect;
    int speed;
    bool active;
    double angle;
    pthread_t thread;
} MissileInfo;

typedef struct
{
    SDL_Rect rect;
    int speed;
    SDL_Rect **fixed_collision_rects;
    MissileInfo **missile_collision_rects;
    int num_missile_collision_rects;
    bool transportingHostage;
} HelicopterInfo;

HelicopterInfo createHelicopter(int x, int y, int w, int h, int speed, SDL_Rect **collisionRectArray);
void addHelicopterCollisionMissile(HelicopterInfo *helicopter, MissileInfo *missile);
void *moveMissiles(void *arg);
void checkMissileCollisions(SDL_Rect helicopterRect, MissileInfo *missiles[], int missiles_length);
void checkHelicopterCollisions(SDL_Rect helicopterRect, SDL_Rect *rects[], int rects_length);
void *moveHelicopter(void *arg);

#endif /* HELICOPTER_H */