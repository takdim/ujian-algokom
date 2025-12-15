#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <omp.h>

/* Simple PPM image read/write (compatible fallback) */
typedef struct {
    unsigned char* data;
    int width;
    int height;
    int channels; // 1 for grayscale, 3 for RGB, 4 for RGBA
} Image;

/*
 * Baca file PNG menggunakan ImageMagick
 * KEEP COLOR - tidak dikonversi ke grayscale
 */
Image* read_png_as_rgb(const char* filename) {
    Image* img = malloc(sizeof(Image));
    char ppm_temp[] = "/tmp/temp_image.ppm";
    char command[512];
    
    // Gunakan ImageMagick convert untuk baca PNG (KEEP RGB)
    snprintf(command, sizeof(command),
             "convert \"%s\" -depth 8 %s 2>/dev/null",
             filename, ppm_temp);
    
    printf("Membaca PNG: %s (KEEP COLOR)\n", filename);
    int ret = system(command);
    
    if (ret != 0) {
        printf("❌ Error: Gagal membaca file PNG.\n");
        printf("   Pastikan:\n");
        printf("   1. File '%s' ada\n", filename);
        printf("   2. ImageMagick terinstall: brew install imagemagick\n");
        free(img);
        return NULL;
    }
    
    // Baca PPM temporary file
    FILE* f = fopen(ppm_temp, "rb");
    if (!f) {
        printf("❌ Error: Gagal membuka temporary PPM file\n");
        free(img);
        return NULL;
    }
    
    char magic[3];
    fscanf(f, "%2s", magic);
    
    if (magic[0] != 'P' || magic[1] != '6') {
        printf("❌ Error: Invalid PPM format\n");
        fclose(f);
        free(img);
        return NULL;
    }
    
    // Skip whitespace
    int c;
    while ((c = fgetc(f)) != EOF && (c == ' ' || c == '\n' || c == '\t' || c == '#')) {
        if (c == '#') {
            while ((c = fgetc(f)) != EOF && c != '\n');
        }
    }
    ungetc(c, f);
    
    // Baca width, height, maxval
    int maxval;
    fscanf(f, "%d %d %d", &img->width, &img->height, &maxval);
    fgetc(f); // skip whitespace
    
    img->channels = 3; // RGB - KEEP COLOR
    img->data = malloc(img->width * img->height * 3 * sizeof(unsigned char));
    
    // Read RGB data langsung (tanpa konversi grayscale)
    fread(img->data, 1, img->width * img->height * 3, f);
    
    fclose(f);
    
    // Cleanup temporary file
    snprintf(command, sizeof(command), "rm -f %s", ppm_temp);
    system(command);
    
    printf("✅ Berhasil membaca PNG: %dx%d (RGB Color)\n", img->width, img->height);
    return img;
}

/*
 * Tulis hasil ke file PPM (RGB Color)
 */
void write_ppm(const char* filename, unsigned char** img, int height, int width) {
    FILE* f = fopen(filename, "wb");
    fprintf(f, "P6\n%d %d\n255\n", width, height);
    
    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            // img[i][j] adalah pointer ke RGB data (3 bytes)
            unsigned char* rgb = (unsigned char*)(img[i] + j * 3);
            fputc(rgb[0], f); // R
            fputc(rgb[1], f); // G
            fputc(rgb[2], f); // B
        }
    }
    fclose(f);
    printf("✅ Hasil disimpan ke: %s\n", filename);
}

/*
 * Fungsi interpolasi bilinear untuk satu titik
 */
static inline double bilinear_interpolate(
    double x, double y,
    double Q11, double Q21, double Q12, double Q22)
{
    double fx1 = Q11 + (Q21 - Q11) * x;
    double fx2 = Q12 + (Q22 - Q12) * x;
    return fx1 + (fx2 - fx1) * y;
}

/*
 * VERSI SERIAL - Resize citra RGB menggunakan interpolasi bilinear
 */
unsigned char** bilinear_resize_serial(
    unsigned char** src, int src_h, int src_w,
    int new_h, int new_w)
{
    unsigned char** dst = malloc(new_h * sizeof(unsigned char*));
    for (int i = 0; i < new_h; i++)
        dst[i] = malloc(new_w * 3 * sizeof(unsigned char)); // 3 for RGB

    double x_ratio = (double)(src_w - 1) / (double)(new_w - 1);
    double y_ratio = (double)(src_h - 1) / (double)(new_h - 1);

    // Loop SERIAL - tidak ada paralelisasi
    for (int i = 0; i < new_h; i++) {
        for (int j = 0; j < new_w; j++) {
            double src_x = j * x_ratio;
            double src_y = i * y_ratio;

            int x1 = (int)src_x;
            int y1 = (int)src_y;
            
            // Boundary check
            if (x1 >= src_w - 1) x1 = src_w - 2;
            if (y1 >= src_h - 1) y1 = src_h - 2;
            if (x1 < 0) x1 = 0;
            if (y1 < 0) y1 = 0;
            
            int x2 = x1 + 1;
            int y2 = y1 + 1;

            double dx = src_x - x1;
            double dy = src_y - y1;

            // Process untuk setiap channel RGB
            for (int c = 0; c < 3; c++) {
                double Q11 = src[y1][x1 * 3 + c];
                double Q21 = src[y1][x2 * 3 + c];
                double Q12 = src[y2][x1 * 3 + c];
                double Q22 = src[y2][x2 * 3 + c];

                double val = bilinear_interpolate(dx, dy, Q11, Q21, Q12, Q22);

                if (val < 0) val = 0;
                if (val > 255) val = 255;

                dst[i][j * 3 + c] = (unsigned char)(val + 0.5);
            }
        }
    }
    return dst;
}

/*
 * VERSI PARALEL - Resize citra RGB menggunakan OpenMP
 */
unsigned char** bilinear_resize_parallel(
    unsigned char** src, int src_h, int src_w,
    int new_h, int new_w, int num_threads)
{
    unsigned char** dst = malloc(new_h * sizeof(unsigned char*));
    for (int i = 0; i < new_h; i++)
        dst[i] = malloc(new_w * 3 * sizeof(unsigned char)); // 3 for RGB

    double x_ratio = (double)(src_w - 1) / (double)(new_w - 1);
    double y_ratio = (double)(src_h - 1) / (double)(new_h - 1);

    // Set jumlah thread untuk OpenMP
    omp_set_num_threads(num_threads);

    // Loop PARALEL menggunakan OpenMP
    #pragma omp parallel for collapse(2) schedule(dynamic)
    for (int i = 0; i < new_h; i++) {
        for (int j = 0; j < new_w; j++) {
            double src_x = j * x_ratio;
            double src_y = i * y_ratio;

            int x1 = (int)src_x;
            int y1 = (int)src_y;
            
            // Boundary check
            if (x1 >= src_w - 1) x1 = src_w - 2;
            if (y1 >= src_h - 1) y1 = src_h - 2;
            if (x1 < 0) x1 = 0;
            if (y1 < 0) y1 = 0;
            
            int x2 = x1 + 1;
            int y2 = y1 + 1;

            double dx = src_x - x1;
            double dy = src_y - y1;

            // Process untuk setiap channel RGB
            for (int c = 0; c < 3; c++) {
                double Q11 = src[y1][x1 * 3 + c];
                double Q21 = src[y1][x2 * 3 + c];
                double Q12 = src[y2][x1 * 3 + c];
                double Q22 = src[y2][x2 * 3 + c];

                double val = bilinear_interpolate(dx, dy, Q11, Q21, Q12, Q22);

                if (val < 0) val = 0;
                if (val > 255) val = 255;

                dst[i][j * 3 + c] = (unsigned char)(val + 0.5);
            }
        }
    }
    return dst;
}

/*
 * Konversi flat image ke 2D array untuk processing (RGB - 3 bytes per pixel)
 */
unsigned char** image_to_2d(unsigned char* data, int height, int width) {
    unsigned char** img = malloc(height * sizeof(unsigned char*));
    for (int i = 0; i < height; i++) {
        img[i] = malloc(width * 3 * sizeof(unsigned char)); // 3 for RGB
        memcpy(img[i], &data[i * width * 3], width * 3);
    }
    return img;
}

/*
 * Fungsi untuk membebaskan memori gambar
 */
void free_image_2d(unsigned char** img, int height) {
    for (int i = 0; i < height; i++)
        free(img[i]);
    free(img);
}

void free_image_struct(Image* img) {
    if (img) {
        if (img->data) free(img->data);
        free(img);
    }
}

/*
 * Fungsi untuk memverifikasi hasil (membandingkan dua gambar RGB)
 */
int verify_results(unsigned char** img1, unsigned char** img2, int h, int w) {
    for (int i = 0; i < h; i++) {
        for (int j = 0; j < w; j++) {
            // Compare RGB values
            if (img1[i][j * 3] != img2[i][j * 3] ||
                img1[i][j * 3 + 1] != img2[i][j * 3 + 1] ||
                img1[i][j * 3 + 2] != img2[i][j * 3 + 2]) {
                return 0;
            }
        }
    }
    return 1;
}

int main(int argc, char* argv[])
{
    printf("=================================================================\n");
    printf("  INTERPOLASI BILINEAR: SERIAL vs PARALEL (OpenMP)\n");
    printf("  Input: Real PNG Image (RGB COLOR)\n");
    printf("=================================================================\n\n");

    // Default filename
    const char* input_file = "gantrycrane.png";
    
    // Jika ada argument, gunakan sebagai filename
    if (argc > 1) {
        input_file = argv[1];
    }

    // Baca PNG image
    Image* src_img = read_png_as_rgb(input_file);
    if (!src_img) {
        printf("\n❌ Gagal membaca gambar. Pastikan:\n");
        printf("   1. File '%s' ada di directory saat ini\n", input_file);
        printf("   2. File format PNG valid\n");
        printf("   3. ImageMagick terinstall: brew install imagemagick\n");
        return 1;
    }

    int src_h = src_img->height;
    int src_w = src_img->width;
    
    // Scaling parameters
    int new_h = src_h * 2;  // 2x scaling
    int new_w = src_w * 2;

    printf("Ukuran gambar sumber: %dx%d (RGB)\n", src_h, src_w);
    printf("Ukuran gambar hasil: %dx%d (RGB)\n", new_h, new_w);
    printf("Faktor scaling: 2.0x\n\n");

    // Convert to 2D array untuk processing
    unsigned char** src = image_to_2d(src_img->data, src_h, src_w);

    // ==================== EKSEKUSI SERIAL ====================
    printf("--- EKSEKUSI SERIAL ---\n");
    double time_start_serial = omp_get_wtime();
    
    unsigned char** result_serial = bilinear_resize_serial(src, src_h, src_w, new_h, new_w);
    
    double time_end_serial = omp_get_wtime();
    double time_serial = time_end_serial - time_start_serial;
    
    printf("Waktu eksekusi SERIAL: %.4f detik\n", time_serial);

    // Save serial result
    write_ppm("result_serial.ppm", result_serial, new_h, new_w);

    // ==================== EKSEKUSI PARALEL ====================
    int thread_counts[] = {2, 4, 8};
    int num_tests = 3;
    unsigned char** result_parallel_8 = NULL;

    printf("\n--- EKSEKUSI PARALEL (OpenMP) ---\n");
    printf("Jumlah core tersedia: %d\n\n", omp_get_max_threads());

    for (int t = 0; t < num_tests; t++) {
        int num_threads = thread_counts[t];
        
        printf("Testing dengan %d threads:\n", num_threads);
        double time_start_parallel = omp_get_wtime();
        
        unsigned char** result_parallel = bilinear_resize_parallel(
            src, src_h, src_w, new_h, new_w, num_threads);
        
        double time_end_parallel = omp_get_wtime();
        double time_parallel = time_end_parallel - time_start_parallel;
        
        printf("  Waktu eksekusi: %.4f detik\n", time_parallel);
        
        // Hitung speedup
        double speedup = time_serial / time_parallel;
        double efficiency = (speedup / num_threads) * 100.0;
        
        printf("  Speedup: %.2fx\n", speedup);
        printf("  Efficiency: %.2f%%\n", efficiency);
        
        // Verifikasi hasil
        int is_correct = verify_results(result_serial, result_parallel, new_h, new_w);
        printf("  Verifikasi: %s\n\n", is_correct ? "BENAR ✓" : "SALAH ✗");
        
        // Simpan result parallel 8-thread
        if (num_threads == 8) {
            result_parallel_8 = result_parallel;
        } else {
            free_image_2d(result_parallel, new_h);
        }
    }

    // ==================== RINGKASAN ====================
    printf("\n=================================================================\n");
    printf("  RINGKASAN PERFORMA\n");
    printf("=================================================================\n");
    printf("Algoritma ini adalah MEMORY BOUND, bukan COMPUTE BOUND\n");
    printf("Speedup terbatas oleh bandwidth memori, bukan jumlah core CPU.\n");
    printf("\nHasil disimpan:\n");
    printf("  - result_serial.ppm (hasil dari versi SERIAL)\n");
    printf("  - result_parallel_8.ppm (hasil dari versi PARALLEL 8-thread)\n");
    printf("=================================================================\n");

    // ==================== SAVE PARALLEL RESULT ====================
    if (result_parallel_8) {
        write_ppm("result_parallel_8.ppm", result_parallel_8, new_h, new_w);
    }

    // ==================== CONVERT PPM TO PNG ====================
    printf("\n🔄 Converting PPM to PNG...\n");
    
    // Convert serial result
    char convert_cmd[256];
    snprintf(convert_cmd, sizeof(convert_cmd),
             "convert result_serial.ppm result_serial.png 2>/dev/null");
    int convert_ret = system(convert_cmd);
    
    if (convert_ret == 0) {
        printf("✅ PNG file created: result_serial.png\n");
    } else {
        printf("⚠️  Warning: Could not convert serial result to PNG.\n");
    }

    // Convert parallel result
    if (result_parallel_8) {
        snprintf(convert_cmd, sizeof(convert_cmd),
                 "convert result_parallel_8.ppm result_parallel_8.png 2>/dev/null");
        convert_ret = system(convert_cmd);
        
        if (convert_ret == 0) {
            printf("✅ PNG file created: result_parallel_8.png\n");
        } else {
            printf("⚠️  Warning: Could not convert parallel result to PNG.\n");
        }
    }

    // Cleanup
    free_image_2d(src, src_h);
    free_image_2d(result_serial, new_h);
    if (result_parallel_8) {
        free_image_2d(result_parallel_8, new_h);
    }
    free_image_struct(src_img);

    return 0;
}

