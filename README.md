# ALGOKOM Demo - Fibonacci (C Backend) & Bilinear Interpolation

Demo interaktif yang mengintegrasikan **program C dengan OpenMP** ke dalam web interface menggunakan **Node.js** sebagai backend.

## 🎯 Fitur

- ✅ **Fibonacci Calculator** dengan 3 implementasi C:
  - Pure Sequential (baseline)
  - OpenMP Serial (mengukur overhead)
  - OpenMP Parallel (task-based parallelism)
- ✅ **Bilinear Interpolation** visual demo (JavaScript)

- ✅ **Real-time performance comparison** dengan speedup & efficiency metrics

## 📁 Struktur File

```
├── fibonacci_json.c          # Program C dengan JSON output
├── server.js                 # Node.js Express server
├── package.json              # Dependencies
├── index.html                # Frontend dengan C Backend integration
├── index copy.html           # Backup versi original (pure JS)
└── README.md                 # Dokumentasi ini
```

## 🚀 Setup & Installation

### 1. Install Node.js Dependencies

```bash
npm install
```

### 2. Compile Program C

```bash
# Compile dengan OpenMP support
gcc-15 -fopenmp -DUSE_OPENMP -o fib_omp_json fibonacci_json.c

# Atau gunakan npm script
npm run compile
```

### 3. Jalankan Server

```bash
npm start
```

Server akan berjalan di: **http://localhost:3000**

### 4. Buka Browser

Buka: **http://localhost:3000**

## 🔧 API Endpoints

### GET `/api/fibonacci/:n`

Menghitung Fibonacci ke-n menggunakan program C.

**Parameter:**

- `n` (integer): 0-45

**Response (JSON):**

```json
{
  "n": 35,
  "sequential": {
    "name": "Pure Sequential",
    "result": 9227465,
    "time": 0.063415,
    "speedup": 1.0,
    "efficiency": 100.0
  },
  "num_threads": 8,
  "openmp_serial": {
    "name": "OpenMP Serial",
    "result": 9227465,
    "time": 0.037574,
    "speedup": 1.69,
    "overhead": -40.74
  },
  "openmp_parallel": {
    "name": "OpenMP Parallel",
    "result": 9227465,
    "time": 0.011172,
    "speedup": 5.68,
    "efficiency": 70.95,
    "cutoff": 20,
    "model": "Fork-Join with Task Dependency"
  }
}
```

**Contoh:**

```bash
curl http://localhost:3000/api/fibonacci/35
```

## 📊 Cara Kerja

1. **Frontend** (index.html) mengirim request ke server
2. **Node.js Server** (server.js) menerima request
3. **Server** menjalankan program C (`fib_omp_json`)
4. **Program C** menghitung Fibonacci dan output JSON
5. **Server** parse JSON dan kirim ke browser
6. **Frontend** menampilkan hasil dengan visualisasi

## 🔍 Tentang Interface

`index.html` adalah interface terpadu yang menggabungkan:

| Feature                | Implementasi             |
| ---------------------- | ------------------------ |
| Fibonacci Backend      | C + OpenMP (server-side) |
| Performance            | Native C speed           |
| Parallel Computing     | OpenMP multi-threading   |
| Real-time Execution    | Ya (via API)             |
| Bilinear Interpolation | JavaScript (client-side) |

## 🛠️ Development

### Compile untuk testing

```bash
npm run compile
```

### Run dengan auto-reload (development)

```bash
npm run dev
```

### Test API

```bash
npm test
# atau
curl http://localhost:3000/api/fibonacci/35
```

## 📝 Notes

- **Cutoff Threshold (20)**: Untuk menghindari overhead task creation pada subproblem kecil
- **Warm-up Run**: Program C menjalankan warm-up untuk stabilisasi performa
- **N maksimal 45**: Untuk menghindari timeout dan overflow
- **Timeout 30 detik**: Request akan dibatalkan jika > 30 detik

## ⚙️ Environment Variables (Optional)

```bash
# Jumlah thread OpenMP (default: semua core)
export OMP_NUM_THREADS=4

# Port server (default: 3000)
export PORT=3000
```

## 🐛 Troubleshooting

### Error: "Failed to execute C program"

- Pastikan program C sudah dikompilasi: `npm run compile`
- Check permission: `chmod +x fib_omp_json`

### Error: "ECONNREFUSED"

- Pastikan server sudah running: `npm start`
- Check port 3000 tidak dipakai program lain

### Kompilasi error: "gcc-15: command not found"

- Install GCC 15: `brew install gcc@15`
- Atau ganti compiler di package.json

## 📄 License

MIT
