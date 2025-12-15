# VS Code C/C++ Configuration untuk OpenMP

## Masalah

`#include <omp.h>` menunjukkan error di VS Code (warna merah) meskipun compiler bisa mengkompilasi.

## Solusi

File `.vscode/c_cpp_properties.json` sudah dikonfigurasi untuk:

- ✅ Menemukan header OpenMP (`omp.h`)
- ✅ Mengenali compiler GCC-15
- ✅ Enable IntelliSense untuk OpenMP functions

## File Konfigurasi

- `.vscode/c_cpp_properties.json` - Include paths dan compiler settings
- `.vscode/settings.json` - VS Code editor settings

## Kompilasi & Eksekusi

### Compile

```bash
gcc-15 -fopenmp -O3 -o bilinear bilinear_serial_parallel.c -lm
```

### Run

```bash
./bilinear
```

## Hasil Eksperimen

**Program:** Image Resizing dengan Bilinear Interpolation (4000x4000)

| Method           | Time (detik) | Speedup | Efficiency |
| ---------------- | ------------ | ------- | ---------- |
| Serial           | 0.0368       | 1.0x    | 100%       |
| OpenMP 2 threads | 0.5289       | 0.07x   | 3.48%      |
| OpenMP 4 threads | 0.5445       | 0.07x   | 1.67%      |
| OpenMP 8 threads | 0.7201       | 0.05x   | 0.63%      |

### 📌 Kesimpulan: MEMORY BOUND Algorithm

- Parallelization **memperlambat** performa (negative speedup)
- Overhead OpenMP > computation benefit
- Memory bandwidth adalah bottleneck, bukan CPU cores
- Algoritma ini demonstrate well-known concept dari "Parallel Computing" PDF

### Optimization Ideas (Future)

1. **Cache-friendly layout** - Transpose/restructure data access
2. **SIMD vectorization** - Use SSE/AVX instructions
3. **Larger grain size** - Reduce parallelization overhead
4. **Blocking strategy** - Process tiles instead of individual pixels
