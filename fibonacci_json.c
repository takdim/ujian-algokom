#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <math.h>

// ==================== Sequential Implementation ====================
int fib_sequential(int n) {
    if (n < 2) return n;
    return fib_sequential(n - 1) + fib_sequential(n - 2);
}

// ==================== OpenMP Implementation ====================
#ifdef USE_OPENMP
#include <omp.h>

#define CUTOFF 20

int fib_omp_task(int n) {
    if (n < 2) return n;
    if (n < CUTOFF) {
        return fib_sequential(n);
    }
    
    int x, y;
    #pragma omp task shared(x)
    {
        x = fib_omp_task(n - 1);
    }
    y = fib_omp_task(n - 2);
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
#endif

// ==================== OpenCilk Implementation ====================
#ifdef USE_CILK
#include <cilk/cilk.h>

#define CILK_CUTOFF 20

int fib_cilk_task(int n) {
    if (n < 2) return n;
    if (n < CILK_CUTOFF) {
        return fib_sequential(n);
    }
    
    int x, y;
    x = cilk_spawn fib_cilk_task(n - 1);
    y = fib_cilk_task(n - 2);
    cilk_sync;
    
    return x + y;
}

int fibonacci_cilk_parallel(int n) {
    return fib_cilk_task(n);
}
#endif

double get_wall_time() {
    struct timeval time;
    gettimeofday(&time, NULL);
    return (double)time.tv_sec + (double)time.tv_usec * 0.000001;
}

// Normalize timing to avoid zero/NaN in JSON output
double normalize_time(double t) {
    const double min_time = 1e-9;
    if (t < min_time) return min_time;
    return t;
}

double safe_ratio(double num, double den) {
    double n = normalize_time(num);
    double d = normalize_time(den);
    double r = n / d;
    if (!isfinite(r) || r < 0) return 0.0;
    return r;
}

int main(int argc, char *argv[]) {
    int N = 35;
    if (argc > 1) {
        N = atoi(argv[1]);
    }
    
    volatile int warmup_result = fib_sequential(N);
    
    double start, end, time_taken;
    int result;
    
    start = get_wall_time();
    result = fib_sequential(N);
    end = get_wall_time();
    time_taken = normalize_time(end - start);
    double baseline_time = time_taken;
    
    int num_threads = 1;
    double speedup_omp = 0.0;
    double efficiency_omp = 0.0;
    double speedup_cilk = 0.0;
    
#ifdef USE_OPENMP
    num_threads = omp_get_max_threads();
#endif
    
    printf("{\n");
    printf("  \"n\": %d,\n", N);
    printf("  \"sequential\": {\n");
    printf("    \"name\": \"Pure Sequential\",\n");
    printf("    \"result\": %d,\n", result);
    printf("    \"time\": %.6f,\n", time_taken);
    printf("    \"speedup\": 1.00,\n");
    printf("    \"efficiency\": 100.00\n");
    printf("  }");
    
#ifdef USE_OPENMP
    printf(",\n");
    printf("  \"num_threads\": %d,\n", num_threads);
    
    start = get_wall_time();
    result = fibonacci_openmp_serial(N);
    end = get_wall_time();
    time_taken = normalize_time(end - start);
    
    start = get_wall_time();
    result = fibonacci_openmp_parallel(N);
    end = get_wall_time();
    time_taken = normalize_time(end - start);
    
    speedup_omp = safe_ratio(baseline_time, time_taken);
    efficiency_omp = (speedup_omp / num_threads) * 100;
    
    printf("  \"openmp_parallel\": {\n");
    printf("    \"name\": \"OpenMP Parallel\",\n");
    printf("    \"result\": %d,\n", result);
    printf("    \"time\": %.6f,\n", time_taken);
    printf("    \"speedup\": %.2f,\n", speedup_omp);
    printf("    \"efficiency\": %.2f,\n", efficiency_omp);
    printf("    \"cutoff\": %d,\n", CUTOFF);
    printf("    \"model\": \"Fork-Join with Task Dependency\"\n");
    printf("  }");
#endif

#ifdef USE_CILK
    printf(",\n");
    printf("  \"cilk_available\": true,\n");
    
    start = get_wall_time();
    result = fibonacci_cilk_parallel(N);
    end = get_wall_time();
    time_taken = normalize_time(end - start);
    if (time_taken < 1e-6) {
        time_taken = baseline_time;
    }
    
    speedup_cilk = safe_ratio(baseline_time, time_taken);
    
    printf("  \"cilk_parallel\": {\n");
    printf("    \"name\": \"Cilk Parallel\",\n");
    printf("    \"result\": %d,\n", result);
    printf("    \"time\": %.6f,\n", time_taken);
    printf("    \"speedup\": %.2f,\n", speedup_cilk);
    printf("    \"cutoff\": %d,\n", CILK_CUTOFF);
    printf("    \"model\": \"Work-Stealing Scheduler\"\n");
    printf("  }\n");
#else
#ifdef USE_OPENMP
    printf(",\n");
    printf("  \"cilk_available\": false\n");
#else
    printf(",\n");
    printf("  \"cilk_available\": false\n");
#endif
#endif

    printf("}\n");
    
    return 0;
}
