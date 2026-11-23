//imports
#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include <stdlib.h>

//O NUMERO DE THREADS E NOME DO ARQUIVO DE IMAGEM A SER LIDO DEVEM SER ALTERADOS NO CÓDIGO. 
//UTILIZE A FLAG: -lm por conta das bibliotecas de leitura de imagem.

// Define a macro de implementação para a biblioteca de leitura de imagens
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// Define a macro de implementação para a biblioteca de salvamento de imagens
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h" 

//sequencial
void processar_sequencial(unsigned char *entrada, unsigned char *saida, int largura, int altura) {
    for (int i = 0; i < largura * altura; i++) {
        saida[i] = entrada[i];
    }
    for (int y = 1; y < altura - 1; y++) {
        for (int x = 1; x < largura - 1; x++) {
            int soma = 0;
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    int ny = y + dy;
                    int nx = x + dx;
                    soma += entrada[ny * largura + nx];
                }
            }
            saida[y * largura + x] = (unsigned char)(soma / 9);
        }
    }
}

//paralela
typedef struct {
    unsigned char *entrada;
    unsigned char *saida;
    int largura;
    int linha_inicio;
    int linha_fim;
} ThreadData;
void* processar_linhas(void *arg) {
    ThreadData *data = (ThreadData *)arg; 
    for (int y = data->linha_inicio; y < data->linha_fim; y++) {
        for (int x = 1; x < data->largura - 1; x++) {
            int soma = 0;
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    int ny = y + dy;
                    int nx = x + dx;
                    soma += data->entrada[ny * data->largura + nx];
                }
            }
            data->saida[y * data->largura + x] = (unsigned char)(soma / 9);
        }
    }
    pthread_exit(NULL);
}

int main () {
    //tipagem
    struct timespec inicio, fim;
    int largura, altura, canais;
    //carrega imagem 
    unsigned char *imagem = stbi_load("imagem1_256(1).png", &largura, &altura, &canais, 1);
    if (imagem == NULL) {
        printf("Erro ao carregar a imagem.\n");
        return 1;
    }
    printf("Imagem carregada: %dx%d pixels, %d canais.\n", largura, altura, canais);
    /*SEQUENCIAL*/
    unsigned char *imagem_saida = (unsigned char *)malloc(largura * altura); //aloca memoria para saida
    if (imagem_saida == NULL) {
        printf("Erro ao alocar memória.\n");
        stbi_image_free(imagem);
        return 1;
    }
    clock_gettime(CLOCK_MONOTONIC, &inicio);
    processar_sequencial(imagem, imagem_saida, largura, altura);
    clock_gettime(CLOCK_MONOTONIC, &fim); //marca tempo sequencial
    double tempo_gasto_sequencial = (fim.tv_sec - inicio.tv_sec) + (fim.tv_nsec - inicio.tv_nsec) / 1e9; //segundos
    printf("Tempo sequencial: %.6f segundos\n", tempo_gasto_sequencial); //imprime
    stbi_write_png("resultado_sequencial.png", largura, altura, 1, imagem_saida, largura); //salva imagem resultante

    /*PARALELA*/
    unsigned char *imagem_saida_paralela = (unsigned char *)malloc(largura * altura); //aloca memoria
    if (imagem_saida_paralela == NULL) {
        printf("Erro ao alocar memória para paralela.\n");
        free(imagem_saida);
        stbi_image_free(imagem);
        return 1;
    }
    for (int i = 0; i < largura * altura; i++) {
        imagem_saida_paralela[i] = imagem[i]; //mantem as bordas inalteradas
    }
    int num_threads = 2; //quantidade threads
    pthread_t threads[8]; //inicializa threads na quantidade maxima
    ThreadData dados_threads[num_threads];
    int linhas_por_thread = (altura - 2) / num_threads; //calcula linhas para thread
    clock_gettime(CLOCK_MONOTONIC, &inicio);
    for (int i = 0; i < num_threads; i++) {
        dados_threads[i].entrada = imagem;
        dados_threads[i].saida = imagem_saida_paralela;
        dados_threads[i].largura = largura;
        dados_threads[i].linha_inicio = 1 + (i * linhas_por_thread);
        if (i == num_threads - 1) {
            dados_threads[i].linha_fim = altura - 1;
        } else {
            dados_threads[i].linha_fim = dados_threads[i].linha_inicio + linhas_por_thread;
        }
        pthread_create(&threads[i], NULL, processar_linhas, (void *)&dados_threads[i]);
    }
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL); //espera acabar todas as threads
    }
    clock_gettime(CLOCK_MONOTONIC, &fim); //termina calculo de tempo
    double tempo_gasto_paralela = (fim.tv_sec - inicio.tv_sec) + (fim.tv_nsec - inicio.tv_nsec) / 1e9; //segundos
    printf("Tempo paralelo (%d threads): %.6f segundos\n", num_threads, tempo_gasto_paralela); //imprime
    stbi_write_png("resultado_paralelo.png", largura, altura, 1, imagem_saida_paralela, largura); //salva resultado

    stbi_image_free(imagem);
    free(imagem_saida);
    free(imagem_saida_paralela); //libera memoria

    return 0;
}