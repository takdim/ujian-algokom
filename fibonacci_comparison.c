#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>

// ==================== Sequential Implementation ====================
int fib_sequential(int n) {
    if (n < 2) return n;
    return fib_sequential(n - 1) + fib_sequential(n - 2);
}

// ==================== OpenMP Implementation ====================
#ifdef USE_OPENMP
#include <omp.h>

#define CUTOFF 20  // Threshold untuk menghindari overhead task creation

int fib_omp_task(int n) {
    if (n < 2) return n;
    
    // Gunakan sequential untuk n kecil (menghindari overhead)
    if (n < CUTOFF) {
        return fib_sequential(n);
    }
    
    int x, y;
    
    // Buat task untuk F(n-1)
    #pragma omp task shared(x)
    {
        x = fib_omp_task(n - 1);
    }
    
    // Hitung F(n-2) di thread saat ini
    y = fib_omp_task(n - 2);
    
    // Tunggu task x selesai sebelum hasil dijumlahkan
    #pragma omp taskwait
    
    return x + y;
}

int fibonacci_openmp_parallel(int n) {
    int result = 0;
    
    #pragma omp parallel
    {
        #pragma omp single
        {
            result = fib_omp_task(n);
        }
    }
    
    return result;
}

// OpenMP Sequential (tanpa tasking)
int fibonacci_openmp_serial(int n) {
    return fib_sequential(n);
}
#endif

// ==================== OpenCilk Implementation ====================
#ifdef USE_CILK
#include <cilk/cilk.h>

int fib_cilk_task(int n) {
    if (n < 2) return n;
    
    int x, y;
    
    // Spawn task untuk F(n-1)
    x = cilk_spawn fib_cilk_task(n - 1);
    
    // Worker saat ini dapat mengeksekusi F(n-2)
    y = fib_cilk_task(n - 2);
    
    // Tunggu task yang di-spawn (x) selesai
    cilk_sync;
    
    return x + y;
}

int fibonacci_cilk_parallel(int n) {
    return fib_cilk_task(n);
}

// Cilk Serial (tanpa spawn)
int fib_cilk_serial(int n) {
    if (n < 2) return n;
    
    int x, y;
    x = fib_cilk_serial(n - 1);
    y = fib_cilk_serial(n - 2);
    
    return x + y;
}
#endif

// Fungsi untuk mengukur waktu dengan presisi tinggi
double get_wall_time() {
    struct timeval time;
    gettimeofday(&time, NULL);
    return (double)time.tv_sec + (double)time.tv_usec * 0.000001;
}

// ==================== Main Program ====================
int main(int argc, char *argv[]) {
    int N = 35; // Default value
    
    if (argc > 1) {
        N = atoi(argv[1]);
    }
    
    printf("==================================================================\n");
    printf("        PERBANDINGAN FIBONACCI: SERIAL vs PARALLEL               \n");
    printf("==================================================================\n");
    printf("Menghitung Fibonacci ke-%d\n", N);
    
#ifdef USE_OPENMP
    int num_threads = omp_get_max_threads();
    printf("Jumlah thread yang tersedia: %d\n", num_threads);
#endif
    
    printf("==================================================================\n");
    
    // WARM-UP RUN untuk mengatasi cache warming effect
    printf("\n[Warm-up] Menjalankan warm-up untuk stabilisasi performa...\n");
    volatile int warmup_result = fib_sequential(N);
    printf("[Warm-up] Selesai\n\n");
    
    double start, end, time_taken;
    int result;
    
    // ============== Pure Sequential ==============
    printf("1. PURE SEQUENTIAL (Baseline)\n");
    printf("   ----------------------------------------\n");
    start = get_wall_time();
    result = fib_sequential(N);
    end = get_wall_time();
    time_taken = end - start;
    
    printf("   Hasil      : %d\n", result);
    printf("   Waktu      : %.6f detik\n", time_taken);
    printf("   Speedup    : 1.00x (baseline)\n");
    printf("   Efficiency : 100.00%%\n\n");
    
    double baseline_time = time_taken;
    
#ifdef USE_OPENMP
    // ============== OpenMP Serial ==============
    printf("2. OPENMP SERIAL (tanpa tasking)\n");
    printf("   ----------------------------------------\n");
    start = get_wall_time();
    result = fibonacci_openmp_serial(N);
    end = get_wall_time();
    time_taken = end - start;

    double overhead_omp = 0.0;
    if (baseline_time > 0.0) {
        overhead_omp = ((time_taken - baseline_time) / baseline_time) * 100;
        if (overhead_omp < 0) overhead_omp = 0.0; // Hindari negatif
    }

    printf("   Hasil      : %d\n", result);
    printf("   Waktu      : %.6f detik\n", time_taken);
    printf("   Speedup    : %.2fx\n", baseline_time / time_taken);
    printf("   Overhead   : %.2f%%\n\n", overhead_omp);

    // ============== OpenMP Parallel ==============
    printf("3. OPENMP PARALLEL (dengan tasking)\n");
    printf("   ----------------------------------------\n");
    start = get_wall_time();
    result = fibonacci_openmp_parallel(N);
    end = get_wall_time();
    time_taken = end - start;
    
    double speedup_omp = baseline_time / time_taken;
    double efficiency_omp = (speedup_omp / num_threads) * 100;
    
    printf("   Hasil      : %d\n", result);
    printf("   Waktu      : %.6f detik\n", time_taken);
    printf("   Speedup    : %.2fx\n", speedup_omp);
    printf("   Efficiency : %.2f%%\n", efficiency_omp);
    printf("   Cutoff     : %d (task creation threshold)\n", CUTOFF);
    printf("   Model      : Fork-Join dengan Task Dependency\n\n");
#endif

#ifdef USE_CILK
    // ============== Cilk Serial ==============
    printf("4. CILK SERIAL (tanpa spawn)\n");
    printf("   ----------------------------------------\n");
    start = get_wall_time();
    result = fib_cilk_serial(N);
    end = get_wall_time();
    time_taken = end - start;

    double overhead_cilk = 0.0;
    if (baseline_time > 0.0) {
        overhead_cilk = ((time_taken - baseline_time) / baseline_time) * 100;
        if (overhead_cilk < 0) overhead_cilk = 0.0; // Hindari negatif
    }

    printf("   Hasil      : %d\n", result);
    printf("   Waktu      : %.6f detik\n", time_taken);
    printf("   Speedup    : %.2fx\n", baseline_time / time_taken);
    printf("   Overhead   : %.2f%%\n\n", overhead_cilk);

    // ============== Cilk Parallel ==============
    printf("5. CILK PARALLEL (dengan spawn)\n");
    printf("   ----------------------------------------\n");
    start = get_wall_time();
    result = fibonacci_cilk_parallel(N);
    end = get_wall_time();
    time_taken = end - start;
    
    double speedup_cilk = baseline_time / time_taken;
    
    printf("   Hasil      : %d\n", result);
    printf("   Waktu      : %.6f detik\n", time_taken);
    printf("   Speedup    : %.2fx\n", speedup_cilk);
    printf("   Model      : Work-Stealing Scheduler\n");
    printf("   P-D Bound  : T_P ≈ W/P + D\n\n");
#endif

    // ============== Summary ==============
    printf("==================================================================\n");
    printf("                            RINGKASAN                             \n");
    printf("==================================================================\n");
    printf("Perbandingan Model:\n\n");
    
    printf("┌─────────────────────┬──────────────────────────────────────────┐\n");
    printf("│ Sequential          │ Satu thread, eksekusi linear             │\n");
    printf("├─────────────────────┼──────────────────────────────────────────┤\n");
    
#ifdef USE_OPENMP
    printf("│ OpenMP Serial       │ Single thread, overhead minimal          │\n");
    printf("│ OpenMP Parallel     │ Multi-thread, Fork-Join, Task Dependency │\n");
    printf("├─────────────────────┼──────────────────────────────────────────┤\n");
#endif

#ifdef USE_CILK
    printf("│ Cilk Serial         │ Single thread, tanpa spawn               │\n");
    printf("│ Cilk Parallel       │ Multi-thread, Work-Stealing              │\n");
#endif
    
    printf("└─────────────────────┴──────────────────────────────────────────┘\n\n");
    
    printf("Keunggulan masing-masing:\n");
    printf("• Sequential  : Paling sederhana, overhead 0%%\n");
    
#ifdef USE_OPENMP
    printf("• OpenMP      : Task dependency eksplisit, kontrol fine-grained\n");
#endif

#ifdef USE_CILK
    printf("• OpenCilk    : Work-stealing efisien, load balancing dinamis\n");
#endif
    
    printf("\n==================================================================\n");
    printf("Catatan:\n");
    printf("- Speedup = Waktu_Sequential / Waktu_Parallel\n");
    printf("- Efficiency = (Speedup / Jumlah_Thread) × 100%%\n");
    printf("- Untuk hasil optimal, gunakan N >= 35\n");
    printf("- Waktu diukur menggunakan wall-clock time (gettimeofday)\n");
    printf("==================================================================\n");
    
    return 0;
}

