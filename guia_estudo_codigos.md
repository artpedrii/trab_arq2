# Guia de estudo: códigos do Trabalho Prático (AOC II, UEM)

Material de estudo para a apresentação/entrevista. Explica **o que cada código faz, como faz e por que foi feito assim**, exatamente como estão no zip (sem mudanças).

---

## 0. Visão geral

| Arquivo | Aplicação | O que tem |
|---|---|---|
| `multiplica_matriz.c` | Multiplicação de matrizes | Versão sequencial + paralela (divisão por linhas de C) |
| `processa_imagem.c` | Filtro de média 3×3 | Versão sequencial + paralela (divisão por linhas da imagem) |
| `contador.c` | Contadores paralelos | Sequencial + paralela A (mutex) + paralela B (contadores locais) |
| `gerar_matrizes.c`, `stb_image*.h` | (fornecidos) | Gera `A_n.txt`/`B_n.txt`; lê/grava PNG |

**Padrão comum aos três programas:**

1. Prepara os dados (fora da medição de tempo).
2. Mede a versão **sequencial** com `clock_gettime(CLOCK_MONOTONIC)`.
3. Mede a versão **paralela** (criação das threads + trabalho + `pthread_join`).
4. Imprime os tempos em segundos. Speedup e eficiência são calculados depois, no relatório.

**O que se muda à mão entre os testes** (está avisado nos comentários do topo de cada arquivo):

- `multiplica_matriz.c`: `#define N`, nomes `A_1024.txt`/`B_1024.txt`, `num_threads`.
- `processa_imagem.c`: nome do PNG e `num_threads`.
- `contador.c`: `numero_threads`.

> Fórmulas do relatório: **S = T_seq / T_par** e **E = S / p** (p = nº de threads).

---

## 1. Conceitos que aparecem nos três códigos

### 1.1 Medição de tempo

```c
struct timespec inicio, fim;
clock_gettime(CLOCK_MONOTONIC, &inicio);
/* ... trabalho ... */
clock_gettime(CLOCK_MONOTONIC, &fim);
tempo = (fim.tv_sec - inicio.tv_sec) + (fim.tv_nsec - inicio.tv_nsec) / 1e9;
```

- `CLOCK_MONOTONIC` é um relógio que **só avança**: não é afetado por ajuste de data/hora do sistema. Por isso é adequado para medir intervalos.
- `timespec` tem segundos (`tv_sec`) e nanossegundos (`tv_nsec`). Divide-se os ns por `1e9` para converter para segundos e soma-se as duas partes.
- É **parede (wall-clock)**, não tempo de CPU. Isso importa: em paralelo queremos saber quanto tempo "real" passou, e não a soma do tempo de todas as threads.

### 1.2 Ciclo de vida de uma thread (pthreads)

```c
pthread_create(&threads[i], NULL, funcao, (void*)&argumento);  // cria e já começa a executar
pthread_join(threads[i], NULL);                                // main espera a thread terminar
```

- `pthread_create` recebe: onde guardar o id da thread, atributos (`NULL` = padrão), a função (`void* f(void*)`) e **um único** argumento `void*`.
- Como só se passa um argumento, usa-se uma **struct** (`InfoThread`, `ThreadData`) ou um ponteiro para variável.
- `pthread_join` bloqueia até a thread acabar. Está **dentro** do intervalo cronometrado, como o enunciado pede.
- Padrão usado: **dois laços separados**, um só para criar todas e outro só para dar `join`. Se fizesse `create` + `join` no mesmo laço, as threads rodariam uma de cada vez (sem paralelismo).

### 1.3 Por que alguns códigos precisam de mutex e outros não?

| Situação | Precisa de sincronização? |
|---|---|
| Várias threads **só leem** o mesmo dado (A e B na multiplicação, imagem de entrada) | Não |
| Cada thread **escreve em posições distintas** (linhas diferentes de C ou da imagem de saída) | Não |
| Várias threads **escrevem na mesma variável** (contador compartilhado) | **Sim** (mutex) |

Matrizes e imagem: a divisão por linhas já garante que ninguém escreve onde outro escreve. Contador A: todo mundo escreve na mesma variável, então precisa do mutex.

---

## 2. `multiplica_matriz.c`

### 2.1 Estruturas de dados

```c
#define N 1024
double A[N][N];
double B[N][N];
double C[N][N];
```

- Matrizes **globais e estáticas** (ficam na seção `.bss`, não na pilha). Isso evita estouro de pilha e deixa todas as threads enxergarem as mesmas matrizes sem precisar passá-las como argumento.
- Tipo `double`: cada matriz 1024×1024 ocupa 1024 × 1024 × 8 B = **8 MiB** (as três juntas: 24 MiB).
- Os valores gerados são inteiros de 0 a 99 (`rand() % 100`), lidos como `double` com `%lf`.

### 2.2 Funções

**`carregar_matriz(nome, matriz)`**: abre o `.txt`, lê N×N valores com `fscanf("%lf")`. Se o arquivo não existe, avisa e retorna. Está **fora** da medição.

**`multiplicar_sequencial()`**: o clássico de três laços:

```c
for i: for j: { C[i][j] = 0; for k: C[i][j] += A[i][k] * B[k][j]; }
```

Complexidade O(N³): para N=1024 são ~1,07 bilhão de multiplicações+somas.

**`multiplicar_paralelizada(void* arg)`**: é **o mesmo código**, mas o laço de fora (`i`) não vai de 0 a N: vai só de `linha_inicio` até `linha_fim` (intervalo recebido via struct):

```c
typedef struct { int id_thread; int linha_inicio; int linha_fim; } InfoThread;
```

(`id_thread` é guardado por organização, mas a função não o usa; só usa as linhas.)

**`salvar_matriz_arquivo`**: grava C com `%.2f` para depois comparar sequencial × paralela. Está **fora** do cronômetro.

### 2.3 Fluxo do `main`

1. Carrega `A_1024.txt` e `B_1024.txt`.
2. **Sequencial:** cronometra `multiplicar_sequencial()`, **depois** salva em `sequencial.txt` e imprime o tempo.
3. **Paralela:**
   - `linhas_por_thread = N / num_threads`.
   - Preenche `info[i]` e cria cada thread.
   - `join` em todas.
   - Para o cronômetro, **depois** salva em `paralela.txt`.

### 2.4 Divisão do trabalho (exemplo)

N = 1024, 8 threads → `linhas_por_thread = 128`:

| Thread | linha_inicio | linha_fim (exclusivo) |
|---|---|---|
| 0 | 0 | 128 |
| 1 | 128 | 256 |
| … | … | … |
| 7 | 896 | 1024 |

A última thread usa `(i == num_threads - 1) ? N : ...`. Isso faz com que **ela absorva qualquer resto** caso N não fosse divisível pelo nº de threads (com 256, 512 e 1024 e 2/4/8 threads a divisão é exata).

### 2.5 Por que não há condição de corrida?

- `A` e `B` são **somente lidas**.
- Cada thread escreve em **linhas exclusivas** de `C`.
- Cada `C[i][j]` é calculado por **uma única thread**, sempre somando na **mesma ordem de k**. Por isso o resultado paralelo é **idêntico** ao sequencial (e os valores são inteiros pequenos, exatamente representáveis em `double`).
- A verificação de igualdade é feita **comparando os arquivos** `sequencial.txt` e `paralela.txt` (o programa não compara internamente).
- Por reaproveitar a mesma matriz `C`, a versão paralela primeiro zera cada elemento (`C[i][j] = 0.0`) e depois acumula, então não "herda" lixo da execução sequencial.

### 2.6 Ligação com cache (para a discussão)

- `A[i][k]` é percorrida **ao longo da linha** (k varia) → acesso sequencial na memória → **boa localidade espacial**.
- `B[k][j]` é percorrida **ao longo da coluna** (k varia) → saltos de 8 KiB (1024 × 8 B) entre acessos → **muitas faltas de cache**. Esse é o principal custo do algoritmo ingênuo para matrizes grandes.
- Com 24 MiB de dados, para 1024×1024 as matrizes não cabem nas caches menores, então o tamanho de N influencia bastante a razão entre computação e acesso à memória.
- Em paralelo: `A` e `B` são lidas por todos os núcleos → as linhas de cache ficam em estado **Shared (S)** no MESI, sem invalidações. Cada núcleo escreve em linhas de `C` próprias → quase nenhum tráfego de coerência. Por isso esta aplicação tende a ter o **melhor escalonamento** dos três. O limite fica por conta de banda de memória, caches compartilhadas e nº de núcleos físicos.

---

## 3. `processa_imagem.c`

### 3.1 Bibliotecas

```c
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
```

Bibliotecas *header-only* (stb): o `#define ..._IMPLEMENTATION` antes do `include` faz o código das funções ser compilado **neste** arquivo. Servem só para ler/gravar PNG (`stbi_load`, `stbi_write_png`). Daí o aviso do cabeçalho sobre usar `-lm`.

### 3.2 Representação da imagem

`stbi_load(arquivo, &largura, &altura, &canais, 1)`:

- O último argumento `1` **força escala de cinza** (1 byte por pixel, 0–255), independente do PNG original.
- `canais` retorna o número de canais **originais** do arquivo (por isso o `printf` pode mostrar um valor diferente de 1).
- A imagem é um vetor 1D `unsigned char*` em ordem de linhas. O pixel (x, y) está em `imagem[y * largura + x]`.

### 3.3 O filtro de média 3×3

Para cada pixel interno, soma os 9 pixels da vizinhança (ele mesmo + 8 vizinhos) e divide por 9:

```c
for dy in -1..1: for dx in -1..1: soma += entrada[(y+dy)*largura + (x+dx)];
saida[y*largura + x] = (unsigned char)(soma / 9);   // divisão inteira (trunca)
```

- Lê de `entrada` e escreve em `saida` (**buffers diferentes**). Isso é essencial: se escrevesse na própria entrada, os pixels já filtrados contaminariam os cálculos dos vizinhos.
- Laços `y = 1 .. altura-2` e `x = 1 .. largura-2` → **bordas ficam de fora**, como pede o enunciado.

### 3.4 Como as bordas permanecem inalteradas

Antes de filtrar, a saída é **inicializada com uma cópia da entrada**. Como o filtro não escreve nas bordas, elas mantêm o valor original.

- Na sequencial, essa cópia está **dentro** de `processar_sequencial` (primeiro `for`), logo entra no tempo medido.
- Na paralela, a cópia é feita no `main` (linhas 97–99), **antes** de iniciar o cronômetro.

### 3.5 Divisão do trabalho entre threads

```c
typedef struct {
    unsigned char *entrada, *saida;
    int largura, linha_inicio, linha_fim;
} ThreadData;
```

Cada thread recebe os ponteiros dos buffers (compartilhados), a largura e seu intervalo de linhas. A divisão só considera as **linhas internas** (1 até altura-2):

```c
linhas_por_thread = (altura - 2) / num_threads;
linha_inicio = 1 + i * linhas_por_thread;
linha_fim    = (última thread) ? altura - 1 : linha_inicio + linhas_por_thread;
```

Exemplo com altura 256 (254 linhas internas):

| Threads | Divisão |
|---|---|
| 2 | T0: linhas 1–127 · T1: linhas 128–254 (127 cada) |
| 8 | T0–T6: 31 linhas cada · T7: 37 linhas (absorve o resto 254 − 8×31 = 6) |

`linha_fim` é exclusivo (`y < linha_fim`). A última thread vai até `altura - 1`, isto é, até a penúltima linha, preservando a última linha como borda.

`processar_linhas` roda o mesmo filtro, mas só nas linhas do seu intervalo; o `x` continua de 1 a `largura-2`.

### 3.5.1 Detalhe: vetor de threads

`pthread_t threads[8]` é fixo no máximo (8), e `ThreadData dados_threads[num_threads]` é um vetor de tamanho variável (VLA) com o nº escolhido.

### 3.6 Sincronização e cache

- **Não usa mutex**: cada thread escreve em linhas distintas de `saida`; `entrada` só é lida.
- Nas **fronteiras** entre blocos, uma thread lê linhas vizinhas que "pertencem" a outra (porque o filtro olha y−1 e y+1), mas só **lê** a entrada, nunca escreve nela. Logo, sem conflito.
- Localidade: acesso linha a linha → bom uso de cache (cada linha de cache de 64 B traz 64 pixels). Os 3 acessos por linha (y−1, y, y+1) reaproveitam dados recentemente carregados.
- Coerência: praticamente só há risco de *false sharing* na linha de cache que fica exatamente na fronteira entre dois blocos de escrita; é um efeito mínimo (no máximo 1 linha de cache por fronteira).
- Imagens pequenas: o custo fixo de criar/juntar threads pesa proporcionalmente mais, então o ganho cresce com a resolução.

### 3.7 Fluxo do `main`

1. Carrega o PNG em escala de cinza.
2. Aloca `imagem_saida`; cronometra `processar_sequencial`; salva `resultado_sequencial.png`.
3. Aloca `imagem_saida_paralela` e copia a entrada nela (mantém bordas).
4. Cria as threads, `join`, para o cronômetro; salva `resultado_paralelo.png`.
5. Libera tudo (`stbi_image_free`, `free`).

Verificação de corretude: comparar os dois PNGs de saída (devem ser idênticos, pois a conta de cada pixel é determinística e independente).

---

## 4. `contador.c`

Objetivo: **100 milhões de incrementos** (`OBJETIVO 100000000`), de três jeitos. Esta aplicação mostra o **custo da sincronização** e da **contenção**.

### 4.1 Sequencial

```c
void executar_sequencial() {
    long long contador = 0;
    for (long long i = 0; i < OBJETIVO; i++) contador++;
}
```

Um laço, uma variável local. `long long` (64 bits) porque 100 milhões cabe em `int`, mas o tipo largo é mais seguro para contagem grande. É a **referência** de tempo (T_seq).

### 4.2 Paralela A: contador compartilhado + mutex

```c
long long contador_compartilhado = 0;     // global, compartilhado
pthread_mutex_t cadeado;                  // o mutex

void* contar_com_mutex(void* arg) {
    long long n = *(long long*)arg;       // quantos incrementos esta thread faz
    for (long long i = 0; i < n; i++) {
        pthread_mutex_lock(&cadeado);
        contador_compartilhado++;         // seção crítica
        pthread_mutex_unlock(&cadeado);
    }
    return NULL;
}
```

- **Por que o mutex?** `contador_compartilhado++` **não é atômico**: são 3 passos (ler → somar 1 → gravar). Sem proteção, duas threads podem ler o mesmo valor e gravar o mesmo resultado → um incremento é perdido (*race condition* / atualização perdida).
- O mutex é inicializado com `pthread_mutex_init(&cadeado, NULL)` antes de criar as threads.
- **Cada incremento** faz lock + unlock. Ou seja, 100 milhões de ciclos de lock/unlock no total, **independente do nº de threads**.
- Todas as threads recebem o **mesmo ponteiro** `&incrementos_por_thread` (OBJETIVO / numero_threads). Não há problema, pois só é **lido**.
- Seção crítica minúscula (um `++`) + lock/unlock caro = o custo de sincronização domina o trabalho útil.

### 4.3 Paralela B: contadores locais

```c
void* contar_com_locais(void* arg) {
    long long n = *(long long*)arg;
    long long contador_local = 0;                 // variável privada da thread (pilha dela)
    for (long long i = 0; i < n; i++) contador_local++;
    long long *resultado = malloc(sizeof(long long));
    *resultado = contador_local;
    pthread_exit(resultado);                      // devolve o resultado via join
}
```

- Cada thread conta no **seu próprio** `contador_local`: sem compartilhamento, sem mutex.
- Para devolver o valor ao `main`, aloca com `malloc` (não pode devolver ponteiro para a pilha da própria thread, pois ela some quando termina) e retorna com `pthread_exit(resultado)`.
- No `main`, o `join` recebe esse ponteiro:

```c
pthread_join(threads_b[i], (void**)&resultado_thread);
contador_total_paraleloB += *resultado_thread;   // soma parcial
free(resultado_thread);                          // libera o malloc
```

- A **soma final** é feita pela thread principal, sequencialmente, só com `numero_threads` parcelas (2, 4 ou 8): custo desprezível.
- O programa imprime `contador_total_paraleloB`, que deve dar **100.000.000** (confirma a corretude).
- O tempo B inclui criar, executar, `join`, somar e dar `free` (tudo dentro do cronômetro).

### 4.4 Divisão

`incrementos_por_thread = OBJETIVO / numero_threads` → 100M é divisível por 2, 4 e 8, então a divisão é exata (50M / 25M / 12,5M por thread). É exatamente a tabela do enunciado (4 threads → 25 milhões cada).

### 4.5 O que explicar sobre desempenho e MESI

**Versão A (mutex):**

- Todas as threads disputam **o mesmo mutex** e escrevem na **mesma variável** → **contenção**. Só uma thread por vez executa a seção crítica, então o trabalho é essencialmente **serializado**. Mais threads = mais disputa, não mais velocidade.
- Quando o mutex está ocupado, a thread pode ser **bloqueada** pelo SO (chamada ao kernel, troca de contexto) e depois acordada: muito mais caro que o `++` em si.
- **Coerência de cache (MESI):** a linha de cache que contém `contador_compartilhado` (e o mutex) precisa ser escrita por vários núcleos. Para escrever, o núcleo precisa ter a linha em **M (Modified)** ou **E (Exclusive)**. Quando outro núcleo quer escrever, a linha é **invalidada (I)** no primeiro e transferida ao segundo. Essa linha fica "pingando" de núcleo em núcleo (*cache line bouncing*), e cada incremento paga essa transferência.
- Resultado esperado: speedup baixo, podendo ser **< 1** (paralelo mais lento que o sequencial), e eficiência muito baixa.

**Versão B (locais):**

- Cada thread só mexe na **sua** variável (na pilha dela, provavelmente até em registrador) → sem tráfego de coerência, sem mutex.
- Trabalho 100% independente → escalonamento próximo do ideal, limitado pelo nº de **núcleos físicos** (8 threads em CPU com menos de 8 núcleos físicos, ou com SMT, não dá o dobro) e pelo custo de criar/juntar threads.

**Importante: medido × teórico.** O programa só **mede tempo**. O MESI é a **explicação teórica** do porquê de A ser lento; ele não foi observado diretamente (isso exigiria contadores de hardware, como `perf`). No relatório/entrevista, separe: *"medi que A foi X vezes mais lenta; a interpretação via MESI/contenção é conceitual"*.

---

## 5. Perguntas prováveis na entrevista (e respostas curtas)

**1. Por que dois laços (criar / join) e não um?**
Para que todas as threads existam e rodem ao mesmo tempo. Com `create` e `join` no mesmo laço, cada thread terminaria antes da próxima começar.

**2. O que está dentro do tempo medido na versão paralela?**
Criação das threads, preenchimento das structs, execução, `pthread_join` (e, no contador B, a soma e os `free`). Não entram: leitura dos arquivos, geração das matrizes, compilação, nem o salvamento dos resultados.

**3. Por que usar `CLOCK_MONOTONIC`?**
Mede tempo real decorrido e não sofre ajustes do relógio do sistema. É o tempo de parede, que é o que interessa para comparar sequencial × paralelo.

**4. Como você garantiu que as threads não calculam a mesma coisa?**
Dividindo por **faixas de linhas disjuntas** (`linha_inicio` / `linha_fim`) nas matrizes e na imagem, e dividindo os incrementos igualmente nos contadores. A última thread absorve o resto da divisão.

**5. Precisa de mutex na multiplicação de matrizes ou na imagem? Por quê?**
Não: leituras compartilhadas são seguras e as escritas são em posições exclusivas de cada thread.

**6. E no contador?**
Na versão A sim, porque `contador++` não é atômico (ler-modificar-escrever). Na B não, porque cada thread tem seu contador privado.

**7. Por que a versão A é tão pior que a B?**
Serialização na seção crítica + custo de lock/unlock por incremento + contenção + linha de cache invalidada/transferida entre núcleos (MESI). A B elimina o compartilhamento.

**8. Como você sabe que o resultado paralelo está correto?**
- Matrizes: `diff` entre `sequencial.txt` e `paralela.txt`.
- Imagem: comparação dos PNGs de saída.
- Contador B: imprime o total (100.000.000). Contador A: o valor final é protegido pelo mutex (corretude garantida pela exclusão mútua).

**9. Por que o speedup não é igual ao número de threads?**
Custo de criar/juntar threads, partes não paralelizáveis, memória/cache compartilhada, nº de núcleos físicos (vs. lógicos), outros processos no sistema, e contenção (no caso A).

**10. O que muda ao aumentar o tamanho da entrada?**
O trabalho útil cresce e o custo fixo das threads pesa menos, então o speedup tende a melhorar. Em matrizes grandes, a memória/cache passa a ser o gargalo.

**11. O que é *false sharing*?**
Quando threads escrevem em variáveis **diferentes** que caem na **mesma linha de cache**, o que causa invalidações como se compartilhassem a variável. Aqui, no contador B as variáveis locais ficam em pilhas diferentes, então não ocorre; em C e na imagem só pode ocorrer nas fronteiras entre blocos, com efeito mínimo.

**12. Por que `pthread_exit(resultado)` com `malloc` no contador B?**
Porque a variável local some quando a thread termina. O `malloc` fica no heap e sobrevive; o `main` recebe o ponteiro no `join` e libera com `free`.

### Pontos de detalhe que podem aparecer (saiba o comportamento)

- **Contador sequencial:** `contador` é local e nunca é usado depois. Dependendo do nível de otimização da compilação (`-O0`, `-O2`...), o compilador pode ou não eliminar o laço. Saiba qual flag você usou e como isso afeta o T_seq do contador.
- **Contador A:** o programa não imprime o valor final de `contador_compartilhado` (o B imprime o total). A garantia de corretude vem do mutex.
- **Imagem:** a cópia inicial da saída está dentro do tempo na sequencial e fora dele na paralela (é o que o código faz).
- **Imagem:** `canais` informa o nº de canais **originais** do PNG; a imagem carregada tem sempre 1 canal por causa do último argumento `1`.
- **Matrizes:** `id_thread` existe na struct mas não é usado; o que define o trabalho é o intervalo de linhas.
- **Parâmetros fixos:** nº de threads, tamanho e nomes de arquivo são alterados no código e recompilados para cada configuração (por isso os comentários de aviso no topo).

---

## 6. Cola rápida (revisão de 2 minutos)

| | Multiplicação | Imagem | Contadores |
|---|---|---|---|
| **Divisão** | Linhas de C | Linhas internas (1 a altura-2) | Nº de incrementos por thread |
| **Dados compartilhados** | A, B (leitura) | entrada (leitura) | A: contador (escrita) · B: nenhum |
| **Escrita** | Linhas exclusivas de C | Linhas exclusivas da saída | A: mesma variável · B: variável local |
| **Mutex?** | Não | Não | **A: sim** · B: não |
| **Gargalo provável** | Memória/cache (acesso a B por coluna) | Memória + custo fixo (imagens pequenas) | A: contenção/coerência · B: nº de núcleos |
| **Resto da divisão** | Última thread pega | Última thread pega | Exato (100M divisível por 2, 4, 8) |
| **Escalabilidade esperada** | Boa | Boa para imagens grandes | A: péssima · B: quase ideal |

**MESI em uma frase:** *Modified* (só eu tenho, alterada), *Exclusive* (só eu tenho, igual à memória), *Shared* (vários têm, só leitura), *Invalid* (cópia inválida). Escrever em dado compartilhado invalida as cópias dos outros núcleos, e é isso que torna o contador com mutex lento.
