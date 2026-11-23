//imports
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>

//O NUMERO DA MATRIZ ALOCADA DE FORMA FIXA DEVE SER MUDADO NO DEFINE, O NOME DO ARQUIVO E O NUMERO DE THREADS TAMBEM DEVE SER ALTERADOS NO CÓDIGO PARA 
//NOVOS TESTES.

#define N 1024 //define a dimensão da matriz (muda de acordo com a que foi gerada pelo outro programa).

double A[N][N]; //define os espaços das matrizes
double B[N][N];
double C[N][N];

//função que lê a matriz do arquivo.txt
void carregar_matriz(const char *nome_arquivo, double matriz[N][N]) {
    FILE *arquivo = fopen(nome_arquivo, "r");
    if (arquivo == NULL) {
        printf("Erro: arquivo %s nao encontrado.\n", nome_arquivo);
        return;
    }
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            fscanf(arquivo, "%lf", &matriz[i][j]);
        }
    }
    fclose(arquivo);
}

//versão sequencial
void multiplicar_sequencial() {
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            C[i][j] = 0.0;
            for (int k = 0; k < N; k++) {
                C[i][j] += A[i][k] * B[k][j];
            }
        }
    }
}

//versão paralela
typedef struct {
    int id_thread;
    int linha_inicio;
    int linha_fim;
} InfoThread;
void* multiplicar_paralelizada(void* arg) {
    InfoThread *info = (InfoThread*) arg;
    for (int i = info->linha_inicio; i < info->linha_fim; i++) {
        for (int j = 0; j < N; j++) {
            C[i][j] = 0.0;
            for (int k = 0; k < N; k++) {
                C[i][j] += A[i][k] * B[k][j];
            }
        }
    }
    pthread_exit(NULL);
}

//salva matriz
void salvar_matriz_arquivo(const char *nome_arquivo, double matriz[N][N]) {
    FILE *arquivo = fopen(nome_arquivo, "w");
    if (arquivo == NULL) {
        printf("Erro ao criar o arquivo de resultado.\n");
        return;
    }
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            fprintf(arquivo, "%.2f ", matriz[i][j]);
        }
        fprintf(arquivo, "\n");
    }
    fclose(arquivo);
    printf("Matriz resultante salva com sucesso em %s!\n", nome_arquivo);
}

int main() {
    //tipagem
    struct timespec inicio, fim;
    double tempo_sequencial;
    double tempo_paralelo;

    //carregando as matrizes
    carregar_matriz("A_1024.txt", A);
    carregar_matriz("B_1024.txt", B);

    //calcula tempo sequencial
    clock_gettime(CLOCK_MONOTONIC, &inicio);
    multiplicar_sequencial();
    clock_gettime(CLOCK_MONOTONIC, &fim);
    salvar_matriz_arquivo("sequencial.txt", C);
    tempo_sequencial = (fim.tv_sec - inicio.tv_sec) + (fim.tv_nsec - inicio.tv_nsec) / 1e9; //segundos
    printf("Tempo sequencial (%dx%d): %f segundos\n", N, N, tempo_sequencial);

    //**PARALELA *//
    // **VERSÃO PARALELA DA MULTIPLICAÇÃO** //
    int num_threads = 8; //quantidade threads
    pthread_t threads[8]; //vetor guardando ate oito threads
    InfoThread info[8]; //id da thread para definir o quanto cada uma faz
    int linhas_por_thread = N / num_threads;
    // Inicia o cronômetro para a versão paralela
    clock_gettime(CLOCK_MONOTONIC, &inicio);
    for (int i = 0; i < num_threads; i++) {
        info[i].id_thread = i;
        info[i].linha_inicio = i * linhas_por_thread;
        info[i].linha_fim = (i == num_threads - 1) ? N : (i + 1) * linhas_por_thread;
        pthread_create(&threads[i], NULL, multiplicar_paralelizada, (void*)&info[i]);
    } //distribui a linha respectiva para cada thread e calcula
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL); //espera terminar
    }
    clock_gettime(CLOCK_MONOTONIC, &fim);
    salvar_matriz_arquivo("paralela.txt", C); //salva
    tempo_paralelo = (fim.tv_sec - inicio.tv_sec) + (fim.tv_nsec - inicio.tv_nsec) / 1e9; //segundos
    printf("Tempo paralelo (%d threads, %dx%d): %f segundos\n", num_threads, N, N, tempo_paralelo); //imprime

    return 0;
}
