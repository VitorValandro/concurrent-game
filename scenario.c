#include <SDL2/SDL.h>
#include <stdio.h>
#include "scenario.h"

// Função pra criar um objeto do cenário
ScenarioElementInfo createScenarioElement(int x, int y, int w, int h)
{
    ScenarioElementInfo rectInfo;
    rectInfo.rect.x = x;
    rectInfo.rect.y = y;
    rectInfo.rect.w = w;
    rectInfo.rect.h = h;
    return rectInfo;
}