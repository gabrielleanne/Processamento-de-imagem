#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <fftw3.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

/* ---------- Parâmetros ajustáveis ---------- */
static  double K1    = 0.5;   /* deslocamento (nao zera DC) */
static  double K2    = 1.5;   /* peso das altas frequencias */
static  double D0    = 30.0;  /* frequencia de corte (raio) */
static const int    ORDER = 2;     /* ordem do Butterworth */

/* ---------- Filtro HHP Butterworth ---------- */
static double hhp_butterworth(int u, int v, int M, int N, double D0, int n)
{
    double du = (u <= M/2) ? u : u - M;
    double dv = (v <= N/2) ? v : v - N;
    double D  = sqrt(du*du + dv*dv);

    if (D == 0.0) return 0.0;
    double ratio = D0 / D;
    return 1.0 / (1.0 + pow(ratio, 2.0 * n));
}

int main(int argc, char **argv)
{
    if (argc > 3) K1 = atof(argv[3]);
    if (argc > 4) K2 = atof(argv[4]);
    if (argc > 5) D0 = atof(argv[5]);

    const char *infile  = (argc > 1) ? argv[1] : "pet_original.jpg";
    const char *outfile = (argc > 2) ? argv[2] : "saida.png";
   

    int width, height, channels;
    unsigned char *img = stbi_load(infile, &width, &height, &channels, 1);
    if (!img) {
        fprintf(stderr, "Erro ao carregar %s\n", infile);
        return 1;
    }
    printf("Imagem: %d x %d (%d canais originais)\n", width, height, channels);

    int M = height, N = width;
    size_t NPix = (size_t)M * N;

    fftw_complex *F = fftw_alloc_complex(NPix);
    fftw_complex *G = fftw_alloc_complex(NPix);

    for (int y = 0; y < M; y++) {
        for (int x = 0; x < N; x++) {
            double val = img[y*N + x] / 255.0;
            F[y*N + x][0] = val;
            F[y*N + x][1] = 0.0;
        }
    }

    /* FFT direta */
    fftw_plan p_fwd = fftw_plan_dft_2d(M, N, F, F, FFTW_FORWARD, FFTW_ESTIMATE);
    fftw_execute(p_fwd);

    /* Aplica H(u,v) = k1 + k2 * HHP(u,v) */
    for (int u = 0; u < M; u++) {
        for (int v = 0; v < N; v++) {
            double Hhp = hhp_butterworth(u, v, M, N, D0, ORDER);
            double H   = K1 + K2 * Hhp;

            double re = F[u*N + v][0];
            double im = F[u*N + v][1];
            G[u*N + v][0] = H * re;
            G[u*N + v][1] = H * im;
        }
    }

    /* FFT inversa */
    fftw_plan p_inv = fftw_plan_dft_2d(M, N, G, G, FFTW_BACKWARD, FFTW_ESTIMATE);
    fftw_execute(p_inv);

    /* Normaliza para [0,255] */
    double minv = 1e30, maxv = -1e30;
    for (size_t i = 0; i < NPix; i++) {
        double v = G[i][0] / NPix;
        if (v < minv) minv = v;
        if (v > maxv) maxv = v;
    }
    double range = (maxv - minv) > 1e-12 ? (maxv - minv) : 1.0;

    unsigned char *out = malloc(NPix);
    for (size_t i = 0; i < NPix; i++) {
        double v = (G[i][0] / NPix - minv) / range;
        if (v < 0) v = 0;
        if (v > 1) v = 1;
        out[i] = (unsigned char)(v * 255.0 + 0.5);
    }

    stbi_write_png(outfile, N, M, 1, out, N);
    printf("Salvo: %s  (k1=%.2f, k2=%.2f, D0=%.1f, n=%d)\n",
           outfile, K1, K2, D0, ORDER);

    fftw_destroy_plan(p_fwd);
    fftw_destroy_plan(p_inv);
    fftw_free(F); fftw_free(G);
    stbi_image_free(img);
    free(out);
    return 0;
}