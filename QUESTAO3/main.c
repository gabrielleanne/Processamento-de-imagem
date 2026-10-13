#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================
 *  Filtro de Mediana para imagens PGM (P5/P2)
 *  Uso: main.exe imagem.pgm
 *  Saída: imagem_mediana.pgm
 * ============================================================ */

/* ---------- Estrutura de imagem PGM ---------- */
typedef struct {
    int w, h, maxval;
    unsigned char *pixels;
} PGM;

/* ---------- Leitura PGM (P5 binário ou P2 ASCII) ---------- */
static PGM *pgm_read(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) { perror("fopen"); return NULL; }

    char magic[3] = {0};
    if (fscanf(f, "%2s", magic) != 1) { fclose(f); return NULL; }
    if (strcmp(magic, "P5") != 0 && strcmp(magic, "P2") != 0) {
        fprintf(stderr, "Formato nao suportado (esperado P5 ou P2): %s\n", magic);
        fclose(f); return NULL;
    }

    int vals[3] = {0}, got = 0, c;
    while (got < 3) {
        c = fgetc(f);
        if (c == '#') { while ((c = fgetc(f)) != EOF && c != '\n'); }
        else if (c == EOF) { fclose(f); return NULL; }
        else if (c > ' ') {
            ungetc(c, f);
            if (fscanf(f, "%d", &vals[got]) != 1) { fclose(f); return NULL; }
            got++;
        }
    }
    if (strcmp(magic, "P5") == 0) fgetc(f);   /* consome 1 separador */

    PGM *img = malloc(sizeof(PGM));
    if (!img) { fclose(f); return NULL; }
    img->w = vals[0]; img->h = vals[1]; img->maxval = vals[2];

    size_t n = (size_t)img->w * img->h;
    img->pixels = malloc(n);
    if (!img->pixels) { free(img); fclose(f); return NULL; }

    if (strcmp(magic, "P5") == 0) {
        if (fread(img->pixels, 1, n, f) != n) {
            fprintf(stderr, "Erro lendo pixels P5\n");
            free(img->pixels); free(img); fclose(f); return NULL;
        }
    } else {
        for (size_t i = 0; i < n; i++) {
            int v; if (fscanf(f, "%d", &v) != 1) v = 0;
            img->pixels[i] = (unsigned char)v;
        }
    }
    fclose(f);
    return img;
}

/* ---------- Escrita PGM (P5 binário) ---------- */
static int pgm_write(const char *path, const PGM *img) {
    FILE *f = fopen(path, "wb");
    if (!f) { perror("fopen"); return 0; }
    fprintf(f, "P5\n%d %d\n%d\n", img->w, img->h, img->maxval);
    size_t n = (size_t)img->w * img->h;
    int ok = (fwrite(img->pixels, 1, n, f) == n);
    fclose(f);
    return ok;
}

/* ---------- Comparador para qsort ---------- */
static int cmp_u8(const void *a, const void *b) {
    unsigned char x = *(const unsigned char *)a;
    unsigned char y = *(const unsigned char *)b;
    return (x > y) - (x < y);
}

/* ---------- Filtro de mediana ----------
 *  window: lado ímpar (3, 5, 7, ...). Padrão: 3.
 *  Border: replicação (clamp) — mais fiel do que zerar, evita borda escura.
 */
static PGM *median_filter(const PGM *src, int window) {
    if (window < 3) window = 3;
    if ((window & 1) == 0) window++;   /* força ímpar */
    const int r = window / 2;
    const int W = src->w, H = src->h;

    PGM *dst = malloc(sizeof(PGM));
    dst->w = W; dst->h = H; dst->maxval = src->maxval;
    dst->pixels = malloc((size_t)W * H);
    if (!dst->pixels) { free(dst); return NULL; }

    unsigned char *buf = malloc((size_t)window * window);
    if (!buf) { free(dst->pixels); free(dst); return NULL; }

    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            int k = 0;
            for (int dy = -r; dy <= r; dy++) {
                int yy = y + dy;
                /* clamp (replicação de borda) */
                if (yy < 0) yy = 0;
                if (yy >= H) yy = H - 1;

                for (int dx = -r; dx <= r; dx++) {
                    int xx = x + dx;
                    if (xx < 0) xx = 0;
                    if (xx >= W) xx = W - 1;

                    buf[k++] = src->pixels[yy * W + xx];
                }
            }
            qsort(buf, (size_t)k, sizeof(unsigned char), cmp_u8);
            dst->pixels[y * W + x] = buf[k / 2];   /* mediana */
        }
    }

    free(buf);
    return dst;
}

/* ---------- Nome de saída: img.pgm -> img_mediana.pgm ---------- */
static void build_output_name(const char *in, char *out, size_t outsz) {
    const char *dot = strrchr(in, '.');
    if (dot) {
        size_t base = (size_t)(dot - in);
        snprintf(out, outsz, "%.*s_mediana%s", (int)base, in, dot);
    } else {
        snprintf(out, outsz, "%s_mediana.pgm", in);
    }
}

/* ---------- main ---------- */
int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <imagem.pgm> [tamanho_janela]\n", argv[0]);
        fprintf(stderr, "     tamanho_janela: impar (padrao 3)\n");
        return 1;
    }

    int window = 3;
    if (argc >= 3) {
        window = atoi(argv[2]);
        if (window < 3 || (window & 1) == 0) {
            fprintf(stderr, "Janela invalida (%d). Use impar >= 3. Usando 3.\n", window);
            window = 3;
        }
    }

    char outpath[1024];
    build_output_name(argv[1], outpath, sizeof(outpath));

    PGM *src = pgm_read(argv[1]);
    if (!src) { fprintf(stderr, "Falha lendo '%s'\n", argv[1]); return 1; }
    printf("Lida: %s (%dx%d, maxval=%d)\n", argv[1], src->w, src->h, src->maxval);

    printf("Aplicando filtro de mediana com janela %dx%d...\n", window, window);
    PGM *dst = median_filter(src, window);
    if (!dst) { fprintf(stderr, "Falha no filtro\n"); free(src->pixels); free(src); return 1; }

    if (!pgm_write(outpath, dst)) {
        fprintf(stderr, "Falha salvando '%s'\n", outpath);
        free(src->pixels); free(src);
        free(dst->pixels); free(dst);
        return 1;
    }
    printf("Salvo: %s\n", outpath);

    free(src->pixels); free(src);
    free(dst->pixels); free(dst);
    return 0;
}