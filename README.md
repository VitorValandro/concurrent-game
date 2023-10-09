## Jogo concorrente

Só testei em ambiente linux, imagino que não vá funcionar no windows sem maiores ajustes.

É necessário instalar o compilador [GCC](https://gcc.gnu.org/) para conseguir compilar o jogo.
As biblioteca usadas são o [SDL2](https://www.libsdl.org/) e o SDL_Image, para instalá-las use os seguintes comandos:

```
sudo apt-get install libsdl2-dev
sudo apt-get install libsdl-image1.2-dev
```

Depois, compile os arquivos usando esse comando:

```
gcc *.c `sdl2-config --libs` -lSDL2 -lSDL2_image -lm -o jogo
```

Agora é só executar o jogo:

```
./jogo
```
