
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Estrutura que representa uma imagem em tons de cinza. */
typedef struct {
    int width;
    int height;
    int maxval;
    unsigned char *data;
} Image;


/* Ignora espacos em branco e comentarios do arquivo PGM. */
static void skip_comments(FILE *fp) {
    int c;

    while ((c = fgetc(fp)) != EOF) {
        if (c == '#') {
            while ((c = fgetc(fp)) != EOF && c != '\n') {
                /* Ignora o restante do comentario. */
            }
        } else if (!isspace((unsigned char)c)) {
            ungetc(c, fp);
            return;
        }
    }
}


/* Le uma imagem PGM binaria (P5), com pixels de 8 bits. */
Image *read_pgm(const char *path) {
    FILE *fp = fopen(path, "rb");

    if (fp == NULL) {
        perror("Erro ao abrir a imagem");
        return NULL;
    }

    char magic[3] = {0};

    if (fscanf(fp, "%2s", magic) != 1 ||
        strcmp(magic, "P5") != 0) {
        fprintf(stderr, "Erro: a imagem deve estar no formato PGM P5.\n");
        fclose(fp);
        return NULL;
    }

    Image *img = malloc(sizeof(Image));

    if (img == NULL) {
        fprintf(stderr, "Erro ao alocar memoria para a imagem.\n");
        fclose(fp);
        return NULL;
    }

    /* Le a largura, a altura e o valor maximo dos pixels. */
    skip_comments(fp);
    int ok1 = fscanf(fp, "%d", &img->width);

    skip_comments(fp);
    int ok2 = fscanf(fp, "%d", &img->height);

    skip_comments(fp);
    int ok3 = fscanf(fp, "%d", &img->maxval);

    if (ok1 != 1 || ok2 != 1 || ok3 != 1 ||
        img->width <= 0 || img->height <= 0 ||
        img->maxval != 255) {
        fprintf(stderr,
                "Erro: cabecalho invalido ou imagem diferente de 8 bits.\n");
        free(img);
        fclose(fp);
        return NULL;
    }

    /*
     * Consome o separador entre o cabecalho e os pixels.
     * Trata tambem o final de linha Windows (\r\n).
     */
    int separador = fgetc(fp);

    if (separador == '\r') {
        int proximo = fgetc(fp);
        if (proximo != '\n' && proximo != EOF) {
            ungetc(proximo, fp);
        }
    } else if (separador == EOF ||
               !isspace((unsigned char)separador)) {
        fprintf(stderr, "Erro: separador invalido no arquivo PGM.\n");
        free(img);
        fclose(fp);
        return NULL;
    }

    size_t n = (size_t)img->width * img->height;

    img->data = malloc(n);

    if (img->data == NULL) {
        fprintf(stderr, "Erro ao alocar memoria para os pixels.\n");
        free(img);
        fclose(fp);
        return NULL;
    }

    /* Confere se todos os pixels foram lidos. */
    if (fread(img->data, 1, n, fp) != n) {
        fprintf(stderr, "Erro: nao foi possivel ler todos os pixels.\n");
        free(img->data);
        free(img);
        fclose(fp);
        return NULL;
    }

    fclose(fp);
    return img;
}


/* Salva uma imagem no formato PGM binario (P5). */
int write_pgm(const char *path, const Image *img) {
    FILE *fp = fopen(path, "wb");

    if (fp == NULL) {
        perror("Erro ao salvar a imagem");
        return -1;
    }

    /* Escreve o cabecalho do arquivo PGM. */
    fprintf(fp, "P5\n%d %d\n%d\n",
            img->width, img->height, img->maxval);

    size_t n = (size_t)img->width * img->height;

    /* Escreve os pixels da imagem. */
    int sucesso = fwrite(img->data, 1, n, fp) == n;

    if (fclose(fp) != 0) {
        sucesso = 0;
    }

    return sucesso ? 0 : -1;
}


/*
 * Convolucao espacial generica.
 *
 * src    = imagem original.
 * kernel = matriz de pesos armazenada em um vetor.
 * d      = dimensao do kernel quadrado.
 * offset = deslocamento aplicado para visualizacao.
 *
 * O kernel deve ter dimensao impar e positiva.
 * As bordas sem vizinhanca completa sao ignoradas.
 * A imagem resultante e menor que a imagem original.
 */
Image *convolve(const Image *src,
                const double *kernel,
                int d,
                int offset) {

    /* Valida os parametros recebidos. */
    if (src == NULL || src->data == NULL ||
        kernel == NULL || d <= 0 || d % 2 == 0) {
        fprintf(stderr, "Erro: parametros invalidos para convolucao.\n");
        return NULL;
    }

    if (src->width < d || src->height < d) {
        fprintf(stderr, "Erro: imagem menor que o kernel.\n");
        return NULL;
    }

    /* Raio: quantidade de pixels ignorados em cada borda. */
    int r = d / 2;

    Image *out = malloc(sizeof(Image));

    if (out == NULL) {
        fprintf(stderr, "Erro ao alocar a imagem de saida.\n");
        return NULL;
    }

    /*
     * Remove r pixels de cada lado.
     * Exemplo: imagem 100x80 e kernel 3x3 -> saida 98x78.
     */
    out->width = src->width - 2 * r;
    out->height = src->height - 2 * r;
    out->maxval = 255;

    size_t total = (size_t)out->width * out->height;
    out->data = malloc(total);

    if (out->data == NULL) {
        fprintf(stderr, "Erro ao alocar os pixels de saida.\n");
        free(out);
        return NULL;
    }

    /* Percorre os pixels que possuem vizinhanca completa. */
    for (int y = r; y < src->height - r; y++) {
        for (int x = r; x < src->width - r; x++) {

            double acc = 0.0;

            /* Percorre todas as posicoes do kernel. */
            for (int ky = 0; ky < d; ky++) {
                for (int kx = 0; kx < d; kx++) {

                    /* Coordenadas do pixel da imagem original. */
                    int iy = y + ky - r;
                    int ix = x + kx - r;

                    /*
                     * Inverte o kernel horizontal e verticalmente.
                     * Essa inversao distingue convolucao de correlacao.
                     */
                    int ky_inv = d - 1 - ky;
                    int kx_inv = d - 1 - kx;

                    double peso = kernel[ky_inv * d + kx_inv];
                    double pixel = src->data[iy * src->width + ix];

                    /* Multiplica pixel pelo peso e acumula o resultado. */
                    acc += pixel * peso;
                }
            }

            /*
             * Aplica o offset. Para o passa-alta, 128 ajuda a visualizar
             * respostas positivas e negativas em tons de cinza.
             */
            acc += offset;

            /* Limita o resultado ao intervalo valido de 8 bits. */
            if (acc < 0.0) {
                acc = 0.0;
            } else if (acc > 255.0) {
                acc = 255.0;
            }

            /*
             * Arredonda para o inteiro mais proximo.
             * As coordenadas sao deslocadas porque a saida e menor.
             */
            out->data[(y - r) * out->width + (x - r)] =
                (unsigned char)(acc + 0.5);
        }
    }

    return out;
}


/* Cria um kernel passa-baixa de media d x d. */
void criar_kernel_media(double *kernel, int d) {
    double peso = 1.0 / ((double)d * d);

    for (int i = 0; i < d * d; i++) {
        kernel[i] = peso;
    }
}


/* Libera a memoria ocupada por uma imagem. */
void liberar_imagem(Image *img) {
    if (img != NULL) {
        free(img->data);
        free(img);
    }
}


/* Executa a convolucao e salva o resultado. */
int processar_filtro(const Image *img,
                     const double *kernel,
                     int d,
                     int offset,
                     const char *nome_saida) {

    Image *resultado = convolve(img, kernel, d, offset);

    if (resultado == NULL) {
        return -1;
    }

    int status = write_pgm(nome_saida, resultado);

    if (status == 0) {
        printf("Kernel %dx%d: %s (%dx%d)\n",
               d, d, nome_saida,
               resultado->width, resultado->height);
    } else {
        fprintf(stderr, "Erro ao gravar %s.\n", nome_saida);
    }

    liberar_imagem(resultado);
    return status;
}


int main(int argc, char **argv) {

    /* Recebe o caminho da imagem pela linha de comando. */
    if (argc < 2) {
        fprintf(stderr, "Uso: %s imagem.pgm\n", argv[0]);
        return 1;
    }

    Image *img = read_pgm(argv[1]);

    if (img == NULL) {
        return 1;
    }

    printf("Imagem original: %dx%d\n\n",
           img->width, img->height);

    /* ---------- Kernels 3x3 ---------- */

    const int d3 = 3;

    /* Passa-baixa: media dos nove pixels. */
    double low3[9];
    criar_kernel_media(low3, d3);

    /* Passa-alta: Laplaciano 3x3. */
    double high3[9] = {
         0, -1,  0,
        -1,  4, -1,
         0, -1,  0
    };

    /* ---------- Kernels 7x7 ---------- */

    const int d7 = 7;

    /* Passa-baixa: media dos 49 pixels. */
    double low7[49];
    criar_kernel_media(low7, d7);

    /*
     * Passa-alta 7x7:
     * centro positivo e pesos negativos nos vizinhos em cruz.
     */
    double high7[49] = {0};

    high7[3 * d7 + 3] = 12;

    for (int k = 1; k <= 3; k++) {
        high7[(3 - k) * d7 + 3] = -1; /* Acima */
        high7[(3 + k) * d7 + 3] = -1; /* Abaixo */
        high7[3 * d7 + (3 - k)] = -1; /* Esquerda */
        high7[3 * d7 + (3 + k)] = -1; /* Direita */
    }

    /*
     * A soma dos pesos de high7 e zero:
     * 12 no centro e doze pesos iguais a -1.
     * Em regioes uniformes, a resposta do filtro e zero
     * antes da aplicacao do offset.
     */

    /* ---------- Aplicacao dos filtros ---------- */

    int erro = 0;

    if (processar_filtro(img, low3, d3, 0, "low_3x3.pgm") != 0)
        erro = 1;

    if (processar_filtro(img, high3, d3, 128, "high_3x3.pgm") != 0)
        erro = 1;

    if (processar_filtro(img, low7, d7, 0, "low_7x7.pgm") != 0)
        erro = 1;

    if (processar_filtro(img, high7, d7, 128, "high_7x7.pgm") != 0)
        erro = 1;

    /* Libera a imagem original. */
    liberar_imagem(img);

    return erro ? 1 : 0;
}
