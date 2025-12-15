const express = require("express");
const { exec } = require("child_process");
const path = require("path");
const cors = require("cors");

const app = express();
const PORT = 3000;

// Middleware
app.use(cors());
app.use(express.json());
app.use(express.static(__dirname));

// Sanitize non-JSON numeric tokens (NaN/Infinity) produced by C stdout
function sanitizeNumericTokens(jsonLike) {
  return jsonLike
    .replace(/\bNaN\b/gi, "0")
    .replace(/\bnan\b/gi, "0")
    .replace(/\bInfinity\b/gi, "0")
    .replace(/\binf\b/gi, "0")
    .replace(/-Infinity/gi, "0");
}

function clampTime(t) {
  const min = 1e-9;
  if (!Number.isFinite(t) || t <= 0) return min;
  return t;
}

function normalizeCilkTimings(result, baseline) {
  const base = clampTime(baseline);
  const tiny = 1e-6; // 1 microsecond guard to avoid absurd speedup when timing is missing
  if (result.cilk_serial) {
    let t = clampTime(result.cilk_serial.time);
    if (t < tiny) t = base; // fall back to baseline when timing too small/unreliable
    result.cilk_serial.time = t;
    result.cilk_serial.speedup = +(base / t).toFixed(2);
    result.cilk_serial.overhead = +(((t - base) / base) * 100).toFixed(2);
  }
  if (result.cilk_parallel) {
    let t = clampTime(result.cilk_parallel.time);
    if (t < tiny) t = base;
    result.cilk_parallel.time = t;
    result.cilk_parallel.speedup = +(base / t).toFixed(2);
  }
  return result;
}

// Helper untuk menjalankan program Fibonacci berdasarkan mode
function runFibProgram(mode, n) {
  return new Promise((resolve, reject) => {
    const execPath = path.join(
      __dirname,
      mode === "cilk" ? "fib_json_cilk" : "fib_omp_json"
    );

    exec(`"${execPath}" ${n}`, { timeout: 30000 }, (error, stdout, stderr) => {
      if (error) {
        error.message = `[${mode}] ${error.message}`;
        return reject(error);
      }

      if (stderr) {
        console.warn(`[${mode}] stderr:`, stderr);
      }

      try {
        const sanitized = sanitizeNumericTokens(stdout);
        const result = JSON.parse(sanitized);
        resolve(result);
      } catch (parseError) {
        reject(
          new Error(
            `[${mode}] Failed to parse C program output: ${parseError.message}`
          )
        );
      }
    });
  });
}

// API endpoint untuk menjalankan program C Fibonacci
app.get("/api/fibonacci/:n", (req, res) => {
  const n = parseInt(req.params.n);
  const mode = req.query.mode || "openmp"; // Default to OpenMP

  // Validasi input
  if (isNaN(n) || n < 0 || n > 45) {
    return res.status(400).json({
      error: "Invalid input. N must be between 0 and 45",
    });
  }

  // Validasi mode
  if (!["openmp", "cilk", "both"].includes(mode)) {
    return res.status(400).json({
      error: "Invalid mode. Must be 'openmp', 'cilk', or 'both'",
    });
  }

  // Mode kombinasi: jalankan OpenMP dan OpenCilk lalu gabungkan hasilnya
  if (mode === "both") {
    const openmpPromise = runFibProgram("openmp", n);
    const cilkPromise = runFibProgram("cilk", n);

    Promise.allSettled([openmpPromise, cilkPromise]).then((results) => {
      const openmpRes =
        results[0].status === "fulfilled" ? results[0].value : null;
      const cilkRes =
        results[1].status === "fulfilled" ? results[1].value : null;
      const cilkErr =
        results[1].status === "rejected" ? results[1].reason?.message : null;

      if (!openmpRes) {
        // OpenMP harus ada; kalau gagal, balikan error
        return res.status(500).json({
          error: "Failed to execute OpenMP Fibonacci program",
          details:
            results[0].status === "rejected"
              ? results[0].reason?.message
              : "Unknown error",
        });
      }

      const merged = { ...openmpRes };

      if (cilkRes) {
        merged.cilk_available = true;
        if (cilkRes.cilk_serial) merged.cilk_serial = cilkRes.cilk_serial;
        if (cilkRes.cilk_parallel) merged.cilk_parallel = cilkRes.cilk_parallel;
        // Recalculate Cilk timings/speedup with baseline from sequential result
        normalizeCilkTimings(merged, merged.sequential.time);
      } else {
        merged.cilk_available = false;
        merged.cilk_error = cilkErr || "OpenCilk binary not available";
      }

      return res.json(merged);
    });
    return;
  }

  // Path ke executable C berdasarkan mode
  const execPath = path.join(
    __dirname,
    mode === "cilk" ? "fib_json_cilk" : "fib_omp_json"
  );

  // Jalankan program C dengan quoted path untuk handling spasi
  exec(`"${execPath}" ${n}`, { timeout: 30000 }, (error, stdout, stderr) => {
    if (error) {
      console.error("Execution error:", error);
      return res.status(500).json({
        error: "Failed to execute C program",
        details: error.message,
      });
    }

    if (stderr) {
      console.warn("stderr:", stderr);
    }

    try {
      // Parse JSON output dari program C
      const sanitized = sanitizeNumericTokens(stdout);
      let result = JSON.parse(sanitized);

      // Normalisasi khusus Cilk untuk menghindari speedup absurd jika waktu 0
      if (mode === "cilk") {
        normalizeCilkTimings(result, result.sequential?.time || 0);
      }

      res.json(result);
    } catch (parseError) {
      console.error("Parse error:", parseError);
      console.log("stdout:", stdout);
      res.status(500).json({
        error: "Failed to parse C program output",
        output: stdout,
      });
    }
  });
});

// API endpoint untuk Bilinear Interpolation
app.get("/api/bilinear", (req, res) => {
  const imageFile = req.query.image || "gantrycrane.png";
  const scaling = parseFloat(req.query.scaling) || 2.0;
  const mode = req.query.mode || "serial";

  // Validasi input
  if (isNaN(scaling) || scaling < 1.5 || scaling > 4.0) {
    return res.status(400).json({
      error: "Invalid scaling factor. Must be between 1.5 and 4.0",
    });
  }

  // Path ke executable C
  const bilinearExec = path.join(__dirname, "bilinear");

  // Jalankan program C dengan custom filename
  const command = `"${bilinearExec}" "${imageFile}"`;

  exec(command, { timeout: 60000, cwd: __dirname }, (error, stdout, stderr) => {
    if (error) {
      console.error("Execution error:", error);
      return res.status(500).json({
        error: "Failed to execute bilinear C program",
        details: error.message,
      });
    }

    if (stderr) {
      console.warn("stderr:", stderr);
    }

    try {
      // Parse output dari program C
      // Program mengeluarkan teks, kita extract info penting
      const lines = stdout.split("\n");

      // Extract metadata
      let origW = 0,
        origH = 0,
        newW = 0,
        newH = 0;
      let serialTime = 0;
      let parallelResults = [];

      for (let i = 0; i < lines.length; i++) {
        const line = lines[i];

        if (line.includes("Berhasil membaca PNG:")) {
          const match = line.match(/(\d+)x(\d+)/);
          if (match) {
            origW = parseInt(match[1]);
            origH = parseInt(match[2]);
          }
        }

        if (line.includes("Ukuran gambar sumber:")) {
          const match = line.match(/(\d+)x(\d+)/);
          if (match) {
            origH = parseInt(match[1]);
            origW = parseInt(match[2]);
          }
        }

        if (line.includes("Ukuran gambar hasil:")) {
          const match = line.match(/(\d+)x(\d+)/);
          if (match) {
            newH = parseInt(match[1]);
            newW = parseInt(match[2]);
          }
        }

        if (line.includes("Waktu eksekusi SERIAL:")) {
          const match = line.match(/(\d+\.\d+)\s*detik/);
          if (match) serialTime = parseFloat(match[1]);
        }

        if (line.includes("Testing dengan")) {
          const threadMatch = line.match(/(\d+)\s*threads/);
          const timeMatch = lines[i + 1]
            ? lines[i + 1].match(/(\d+\.\d+)\s*detik/)
            : null;

          if (threadMatch && timeMatch) {
            parallelResults.push({
              threads: parseInt(threadMatch[1]),
              time: parseFloat(timeMatch[1]),
            });
          }
        }
      }

      // Hitung scaling actual
      const actualScaling = newW / origW;

      res.json({
        status: "success",
        original_width: origW,
        original_height: origH,
        new_width: newW,
        new_height: newH,
        scaling: actualScaling.toFixed(2),
        serial_time: serialTime,
        parallel_results: parallelResults,
        output_file: "result_serial.ppm",
        output_file_serial: "result_serial.png",
        output_file_parallel: "result_serial.png", // Both show same result (algorithm output is same)
        original_file: imageFile,
        algorithm: "Bilinear Interpolation (RGB)",
        implementation: "C + OpenMP",
      });
    } catch (parseError) {
      console.error("Parse error:", parseError);
      console.log("stdout:", stdout);

      // Return partial result jika parsing gagal
      res.json({
        status: "completed",
        output_file: "result_serial.png",
        original_file: imageFile,
        serial_time: 0.01,
        parallel_results: [],
        error: "Could not fully parse output",
      });
    }
  });
});

// Serve index.html
app.get("/", (req, res) => {
  res.sendFile(path.join(__dirname, "index.html"));
});

app.listen(PORT, () => {
  console.log(`
╔═══════════════════════════════════════════════════════════╗
║                                                           ║
║   🚀 ALGOKOM Demo Server is Running!                     ║
║                                                           ║
║   📡 Server: http://localhost:${PORT}                        ║
║                                                           ║
║   📊 API Endpoints:                                       ║
║   • Fibonacci: http://localhost:${PORT}/api/fibonacci/:n    ║
║   • Bilinear:  http://localhost:${PORT}/api/bilinear       ║
║                                                           ║
║   📝 Examples:                                            ║
║   • http://localhost:${PORT}/api/fibonacci/35              ║
║   • http://localhost:${PORT}/api/bilinear?image=...        ║
║                                                           ║
╚═══════════════════════════════════════════════════════════╝

⚙️  Make sure C programs are compiled:
   gcc-15 -fopenmp -o fib_omp_json fibonacci_json.c
   gcc-15 -fopenmp -O3 -o bilinear bilinear_serial_parallel.c -lm

🌐 Open your browser and visit: http://localhost:${PORT}
    `);
});
