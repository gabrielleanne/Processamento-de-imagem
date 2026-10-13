#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------- Estrutura de imagem PGM ---------- */
typedef struct {
    int w, h, maxval;
    unsigned char *pixels;   /* 1 byte por pixel (assumimos maxval <= 255) */
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

    /* Pula comentários (#) e lê w, h, maxval */
    int vals[3] = {0}, got = 0;
    int c;
    while (got < 3) {
        c = fgetc(f);
        if (c == '#') {                 /* consome até fim da linha */
            while ((c = fgetc(f)) != EOF && c != '\n');
        } else if (c == EOF) {
            fclose(f); return NULL;
        } else if (c > ' ') {           /* início de um número */
            ungetc(c, f);
            if (fscanf(f, "%d", &vals[got]) != 1) { fclose(f); return NULL; }
            got++;
        }
    }
    /* Após ler maxval, consome UMA quebra de linha / espaço do P5 */
    if (strcmp(magic, "P5") == 0) fgetc(f);

    PGM *img = malloc(sizeof(PGM));
    if (!img) { fclose(f); return NULL; }
    img->w = vals[0];
    img->h = vals[1];
    img->maxval = vals[2];

    size_t n = (size_t)img->w * img->h;
    img->pixels = malloc(n);
    if (!img->pixels) { free(img); fclose(f); return NULL; }

    if (strcmp(magic, "P5") == 0) {
        if (fread(img->pixels, 1, n, f) != n) {
            fprintf(stderr, "Erro lendo pixels P5\n");
            free(img->pixels); free(img); fclose(f); return NULL;
        }
    } else { /* P2: valores ASCII */
        for (size_t i = 0; i < n; i++) {
            int v;
            if (fscanf(f, "%d", &v) != 1) { v = 0; }
            img->pixels[i] = (unsigned char)v;
        }
    }

    fclose(f);
    return img;
}

/* ---------- Escrita PGM (sempre P5 binário) ---------- */
static int pgm_write(const char *path, const PGM *img) {
    FILE *f = fopen(path, "wb");
    if (!f) { perror("fopen"); return 0; }
    fprintf(f, "P5\n%d %d\n%d\n", img->w, img->h, img->maxval);
    size_t n = (size_t)img->w * img->h;
    if (fwrite(img->pixels, 1, n, f) != n) { fclose(f); return 0; }
    fclose(f);
    return 1;
}

/* ---------- Equalização de histograma ---------- */
static void histogram_equalization(PGM *img) {
    const int L = 256;
    size_t N = (size_t)img->w * img->h;

    /* 1) histograma */
    unsigned long hist[256] = {0};
    for (size_t i = 0; i < N; i++) hist[img->pixels[i]]++;

    /* 2) CDF */
    unsigned long cdf[256];
    cdf[0] = hist[0];
    for (int k = 1; k < L; k++) cdf[k] = cdf[k-1] + hist[k];

    /* 3) cdf_min = primeiro valor não-nulo */
    unsigned long cdf_min = 0;
    for (int k = 0; k < L; k++) if (cdf[k]) { cdf_min = cdf[k]; break; }

    /* 4) mapa de transformação */
    unsigned char map[256];
    double denom = (double)(N - cdf_min);
    if (denom <= 0.0) {
        for (int k = 0; k < L; k++) map[k] = (unsigned char)k;  /* imagem uniforme */
    } else {
        for (int k = 0; k < L; k++) {
            double v = ((double)(cdf[k] - cdf_min) / denom) * (L - 1);
            if (v < 0) v = 0;
            if (v > L - 1) v = L - 1;
            map[k] = (unsigned char)(v + 0.5);
        }
    }

    /* 5) aplica */
    for (size_t i = 0; i < N; i++) img->pixels[i] = map[img->pixels[i]];
}

/* ---------- (opcional) salva histograma como imagem PGM 256x200 ---------- */
static void save_hist_pgm(const PGM *img, const char *path) {
    unsigned long hist[256] = {0};
    size_t N = (size_t)img->w * img->h;
    for (size_t i = 0; i < N; i++) hist[img->pixels[i]]++;

    unsigned long mx = 1;
    for (int k = 0; k < 256; k++) if (hist[k] > mx) mx = hist[k];

    const int HW = 256, HH = 200;
    unsigned char *buf = malloc((size_t)HW * HH);
    memset(buf, 255, (size_t)HW * HH);

    for (int x = 0; x < HW; x++) {
        int bar = (int)((double)hist[x] / (double)mx * (HH - 1));
        for (int y = 0; y < bar; y++)
            buf[(HH - 1 - y) * HW + x] = 0;
    }

    FILE *f = fopen(path, "wb");
    if (!f) { free(buf); return; }
    fprintf(f, "P5\n%d %d\n255\n", HW, HH);
    fwrite(buf, 1, (size_t)HW * HH, f);
    fclose(f);
    free(buf);
}

/* ---------- Gera "saida" a partir de "entrada.pgm" ---------- */
/* entrada: /pasta/img.pgm  ->  saida: /pasta/img_equalizada.pgm */
static void build_output_name(const char *in, char *out, size_t outsz) {
    const char *dot = strrchr(in, '.');
    if (dot && (strcasecmp(dot, ".pgm") == 0 || strcasecmp(dot, ".ppm") == 0)) {
        size_t base = (size_t)(dot - in);
        snprintf(out, outsz, "%.*s_equalizada%s", (int)base, in, dot);
    } else {
        snprintf(out, outsz, "%s_equalizada.pgm", in);
    }
}

/* ---------- main ---------- */
int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <imagem.pgm>\n", argv[0]);
        return 1;
    }

    const char *inpath = argv[1];
    char outpath[1024];
    build_output_name(inpath, outpath, sizeof(outpath));

    PGM *img = pgm_read(inpath);
    if (!img) {
        fprintf(stderr, "Falha ao ler '%s'\n", inpath);
        return 1;
    }
    printf("Lida: %s (%dx%d, maxval=%d)\n", inpath, img->w, img->h, img->maxval);

    /* histogramas antes/depois (opcionais, mas ótimos pra relatório) */
    save_hist_pgm(img, "hist_original.pgm");
    histogram_equalization(img);
    save_hist_pgm(img, "hist_equalizado.pgm");

    if (!pgm_write(outpath, img)) {
        fprintf(stderr, "Falha ao salvar '%s'\n", outpath);
        free(img->pixels); free(img);
        return 1;
    }
    printf("Salvo: %s\n", outpath);

    free(img->pixels);
    free(img);
    return 0;
}