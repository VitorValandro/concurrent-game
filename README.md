## Jogo concorrente

Só testei em ambiente linux, imagino que não vá funcionar no windows sem maiores ajustes.

É necessário instalar o compilador [GCC](https://gcc.gnu.org/) para conseguir compilar o jogo.
A única biblioteca usada é o [SDL2](https://www.libsdl.org/), para instalá-la use o seguinte comando:

```
sudo apt-get install libsdl2-dev
```

Depois, compile o arquivo `jogo.c` usando esse comando:

```
gcc -o jogo jogo.c -lSDL2
```

Agora é só executar o jogo:

```
./jogo
```