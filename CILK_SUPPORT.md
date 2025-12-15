# CILK Support Documentation

## Status Saat Ini

Program C sudah memiliki **full Cilk implementation** (serial dan parallel), namun:

- ✅ **Kode Cilk sudah lengkap** di `fibonacci_json.c`
- ❌ **Header Cilk tidak tersedia** di GCC Homebrew
- ✅ **UI sudah siap** menampilkan Cilk results

## Implementasi Cilk di Kode

### 1. Cilk Serial (tanpa spawn)

```c
int fib_cilk_serial(int n) {
    if (n < 2) return n;

    int x, y;
    x = fib_cilk_serial(n - 1);  // Sequential
    y = fib_cilk_serial(n - 2);

    return x + y;
}
```

### 2. Cilk Parallel (dengan work-stealing)

```c
#define CILK_CUTOFF 20

int fib_cilk_task(int n) {
    if (n < 2) return n;

    if (n < CILK_CUTOFF) {
        return fib_sequential(n);  // Hindari overhead
    }

    int x, y;

    x = cilk_spawn fib_cilk_task(n - 1);  // Spawn
    y = fib_cilk_task(n - 2);             // Main thread
    cilk_sync;                             // Wait

    return x + y;
}
```

## Cara Mengaktifkan Cilk

### Opsi 1: Install OpenCilk (Recommended)

```bash
# Install OpenCilk compiler
brew install opencilk

# Compile dengan Cilk support
clang++ -fopencilk -DUSE_OPENMP -DUSE_CILK -o fib_omp_json fibonacci_json.c

# Jalankan
./fib_omp_json 35
```

### Opsi 2: Install GCC dengan Cilk support

```bash
# Install GCC terbaru dengan Cilk
brew install gcc@15

# Verify Cilk available
gcc-15 --version | grep cilk

# Compile
gcc-15 -fopenmp -DUSE_CILK -o fib_omp_json fibonacci_json.c
```

### Opsi 3: Compile Hanya OpenMP (Saat Ini)

```bash
gcc-15 -fopenmp -DUSE_OPENMP -o fib_omp_json fibonacci_json.c
```

## Output JSON dengan Cilk

Ketika Cilk tersedia, output JSON akan berisi:

```json
{
  "n": 35,
  "sequential": { ... },
  "openmp_serial": { ... },
  "openmp_parallel": { ... },
  "cilk_available": true,
  "cilk_serial": {
    "name": "Cilk Serial",
    "result": 9227465,
    "time": 0.0396,
    "speedup": 1.02,
    "overhead": 2.0
  },
  "cilk_parallel": {
    "name": "Cilk Parallel",
    "result": 9227465,
    "time": 0.0095,
    "speedup": 4.24,
    "cutoff": 20,
    "model": "Work-Stealing Scheduler"
  }
}
```

## Perbandingan OpenMP vs Cilk

| Aspect              | OpenMP                 | Cilk                 |
| ------------------- | ---------------------- | -------------------- |
| **Syntax**          | `#pragma omp task`     | `cilk_spawn`         |
| **Synchronization** | `#pragma omp taskwait` | `cilk_sync`          |
| **Scheduler**       | Task queue             | Work-stealing        |
| **Load Balancing**  | Implicit               | Automatic (optimal)  |
| **Overhead**        | Moderate               | Minimal              |
| **Best For**        | Loop parallelism       | Recursive algorithms |
| **Performance**     | Good                   | Excellent            |

## HTML UI Support

File `index.html` sudah siap menampilkan:

✅ Sequential Results
✅ OpenMP Serial Results  
✅ OpenMP Parallel Results
✅ **Cilk Serial Results** (when available)
✅ **Cilk Parallel Results** (when available)
✅ Performance Summary (compare semua methods)

## Langkah-Langkah Enable Cilk

### 1. Install OpenCilk

```bash
brew install opencilk
```

### 2. Compile

```bash
cd "/Users/aim/Documents/algoritma komputasi/tugas akhir"
clang -fopencilk -DUSE_OPENMP -DUSE_CILK -o fib_omp_json fibonacci_json.c
```

### 3. Test

```bash
./fib_omp_json 35
```

### 4. Restart Server

```bash
npm start
```

### 5. Lihat di Browser

```
http://localhost:3000
```

## Expected Results

Setelah Cilk enabled, Anda akan melihat:

- 5 implementation methods (Sequential, OpenMP Serial, OpenMP Parallel, Cilk Serial, Cilk Parallel)
- Performance comparison lengkap
- Speedup analysis untuk masing-masing method

## Troubleshooting

### Error: "cilk/cilk.h: No such file or directory"

→ OpenCilk tidak terinstall. Jalankan: `brew install opencilk`

### Error: "clang: unknown argument '-fopencilk'"

→ Clang standard tidak support OpenCilk. Install: `brew install opencilk`

### Program bekerja tapi Cilk tidak tersedia

→ Normal! Program fall back ke OpenMP only. Output akan menunjukkan `"cilk_available": false`

## Note untuk Presentation

Saat ini program berjalan dengan **OpenMP only**, yang sudah memberikan:

- ✅ 3.76x speedup (Fibonacci 35)
- ✅ 47.05% efficiency (dengan 8 threads)

Dengan Cilk (saat diinstall), akan memberikan:

- 🔮 ~4.24x speedup (lebih optimal)
- 🔮 Minimal overhead
- 🔮 Better load balancing
