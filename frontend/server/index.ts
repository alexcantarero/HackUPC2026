import cors from "cors";
import express from "express";
import multer from "multer";
import { spawn } from "node:child_process";
import fs from "node:fs/promises";
import os from "node:os";
import path from "node:path";
import { fileURLToPath } from "node:url";

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const app = express();
const port = Number(process.env.API_PORT ?? 8787);

const requiredFileFields = [
  "warehouse",
  "obstacles",
  "ceiling",
  "types_of_bays",
] as const;

type RequiredField = (typeof requiredFileFields)[number];

type SolveResponse = {
  ok: boolean;
  message: string;
  stdout: string;
  stderr: string;
  bestScore: number | null;
  outputFileName: string | null;
  outputCsv: string | null;
  resultId: string | null;
  csvInputs: Record<RequiredField, string>;
  exitCode: number;
  durationMs: number;
};

const upload = multer({
  storage: multer.memoryStorage(),
  limits: {
    files: requiredFileFields.length,
    fileSize: 10 * 1024 * 1024,
  },
});

app.use(cors());
app.use(express.json());

app.get("/api/health", (_req, res) => {
  res.json({ ok: true });
});

/**
 * Fetch a specific result by ID (folder name)
 */
app.get("/api/results/:id", async (req, res) => {
  const { id } = req.params;
  const workspaceRoot = path.resolve(__dirname, "../..");
  const resultDir = path.join(workspaceRoot, "data", "output", id);

  try {
    const filesInFolder = await fs.readdir(resultDir);
    const algorithmResults = [];
    const csvInputs: any = {};
    
    // Load persisted inputs
    for (const field of requiredFileFields) {
      const inputFile = `input_${field}.csv`;
      if (filesInFolder.includes(inputFile)) {
        csvInputs[field] = await fs.readFile(path.join(resultDir, inputFile), "utf8");
      }
    }

    for (const file of filesInFolder) {
      if (file.endsWith(".json")) {
        const filePath = path.join(resultDir, file);
        const jsonContent = await fs.readFile(filePath, "utf8");
        const parsedJson = JSON.parse(jsonContent);
        let csvContent = null;
        const csvFile = file.replace(".json", ".csv");
        if (filesInFolder.includes(csvFile)) {
          csvContent = await fs.readFile(path.join(resultDir, csvFile), "utf8");
        }

        algorithmResults.push({
          ...parsedJson,
          outputFile: file,
          outputCsv: csvContent
        });
      }
    }
    
    res.json({
      ok: true,
      algorithmResults,
      csvInputs,
      resultId: id
    });
  } catch (err) {
    res.status(404).json({ ok: false, message: "Result not found" });
  }
});

function parseBestScore(stdout: string): number | null {
  const match = stdout.match(/Best solution:\s*score=([-+]?\d*\.?\d+(?:[eE][-+]?\d+)?)/);
  if (!match) return null;
  const score = Number(match[1]);
  return Number.isFinite(score) ? score : null;
}

function normalizeBoolean(value: unknown): boolean {
  if (typeof value !== "string") return false;
  return value.toLowerCase() === "true" || value === "1";
}

app.post(
  "/api/solve",
  upload.fields(requiredFileFields.map((name) => ({ name, maxCount: 1 }))),
  async (req, res) => {
    const workspaceRoot = path.resolve(__dirname, "../..");
    const backendDir = path.join(workspaceRoot, "backend");
    const solverPath = path.join(backendDir, "bin", "solver");

    try {
      await fs.access(solverPath);
    } catch {
      res.status(500).json({
        ok: false,
        message:
          "Solver binary not found. Build it first with: cd backend && make",
      });
      return;
    }

    const uploadedFiles = req.files as
      | Record<string, Express.Multer.File[]>
      | undefined;

    for (const field of requiredFileFields) {
      const file = uploadedFiles?.[field]?.[0];
      if (!file) {
        res.status(400).json({
          ok: false,
          message: `Missing required CSV file: ${field}`,
        });
        return;
      }

      if (!file.originalname.toLowerCase().endsWith(".csv")) {
        res.status(400).json({
          ok: false,
          message: `File for '${field}' must be a .csv file`,
        });
        return;
      }
    }

    const includeOutputCsv = normalizeBoolean(req.body.includeOutputCsv);
    const algo = typeof req.body.algo === "string" ? req.body.algo : "greedy";
    const mode = typeof req.body.mode === "string" ? req.body.mode : "parallel";
    const csvInputs = {} as Record<RequiredField, string>;

    for (const field of requiredFileFields) {
      const file = uploadedFiles?.[field]?.[0];
      if (file) {
        csvInputs[field] = file.buffer.toString("utf8");
      }
    }

    let caseDir = "";
    const startedAt = Date.now();

    try {
      caseDir = await fs.mkdtemp(path.join(os.tmpdir(), "solver-case-"));

      for (const field of requiredFileFields) {
        const file = uploadedFiles?.[field]?.[0];
        if (!file) continue;
        const targetPath = path.join(caseDir, `${field}.csv`);
        await fs.writeFile(targetPath, file.buffer);
      }

      const args = ["--case", caseDir, "--mode", mode, "--algo", algo];

      const runResult = await new Promise<{
        exitCode: number;
        stdout: string;
        stderr: string;
      }>((resolve, reject) => {
        const child = spawn(solverPath, args, {
          cwd: backendDir,
          env: process.env,
        });

        let stdout = "";
        let stderr = "";

        child.stdout.on("data", (chunk: Buffer) => {
          stdout += chunk.toString();
        });

        child.stderr.on("data", (chunk: Buffer) => {
          stderr += chunk.toString();
        });

        child.on("error", (error) => {
          reject(error);
        });

        child.on("close", (code) => {
          resolve({
            exitCode: code ?? 1,
            stdout,
            stderr,
          });
        });
      });

      const logDebug = (msg: string) => {
        console.log(`[API] ${msg}`);
      };

      // Discovery logic: Try regex first (robust version), fallback to newest folder in data/output
      let absoluteFolderPath = "";
      const stdoutClean = runResult.stdout.replace(/\u001b\[[0-9;]*m/g, ""); // Strip ANSI colors
      const folderMatch = stdoutClean.match(/Output folder:\s*([^\s\n\r]+)/i);
      
      if (folderMatch && folderMatch[1]) {
        const rawFolderPath = folderMatch[1].trim();
        const path1 = path.resolve(backendDir, rawFolderPath);
        const path2 = path.resolve(workspaceRoot, rawFolderPath.replace(/^\.\.\//, ""));
        
        try {
          await fs.access(path1);
          absoluteFolderPath = path1;
        } catch {
          try {
            await fs.access(path2);
            absoluteFolderPath = path2;
          } catch {
            logDebug(`Regex paths not accessible: ${path1} OR ${path2}`);
          }
        }
      } 
      
      if (!absoluteFolderPath) {
        logDebug("Falling back to newest folder in data/output...");
        try {
          const outputDir = path.join(workspaceRoot, "data", "output");
          const entries = await fs.readdir(outputDir, { withFileTypes: true });
          const folders = entries
            .filter(e => e.isDirectory())
            .map(e => ({ name: e.name, path: path.join(outputDir, e.name) }));
          
          if (folders.length > 0) {
            const foldersWithTime = await Promise.all(folders.map(async f => {
              const stat = await fs.stat(f.path);
              return { ...f, mtime: stat.mtimeMs };
            }));
            foldersWithTime.sort((a, b) => b.mtime - a.mtime);
            absoluteFolderPath = foldersWithTime[0].path;
          }
        } catch (err) {
          logDebug(`Fallback discovery failed: ${err}`);
        }
      }

      const payload: SolveResponse & { algorithmResults?: any[], debug?: string[] } = {
        ok: runResult.exitCode === 0,
        message:
          runResult.exitCode === 0
            ? "Solver finished successfully"
            : "Solver finished with errors",
        stdout: runResult.stdout,
        stderr: runResult.stderr,
        bestScore: parseBestScore(runResult.stdout),
        outputFileName: null,
        outputCsv: null,
        resultId: absoluteFolderPath ? path.basename(absoluteFolderPath) : null,
        csvInputs,
        exitCode: runResult.exitCode,
        durationMs: Date.now() - startedAt,
        debug: []
      };

      if (absoluteFolderPath) {
        try {
          // Copy original inputs to the output folder so they can be synced to other devices
          for (const field of requiredFileFields) {
            const source = path.join(caseDir, `${field}.csv`);
            const target = path.join(absoluteFolderPath, `input_${field}.csv`);
            await fs.copyFile(source, target);
          }

          const filesInFolder = await fs.readdir(absoluteFolderPath);
          const algorithmResults = [];

          for (const file of filesInFolder) {
            if (file.endsWith(".json")) {
              const filePath = path.join(absoluteFolderPath, file);
              const jsonContent = await fs.readFile(filePath, "utf8");
              try {
                const parsedJson = JSON.parse(jsonContent);
                let csvContent = null;
                const csvFile = file.replace(".json", ".csv");
                if (includeOutputCsv && filesInFolder.includes(csvFile)) {
                  csvContent = await fs.readFile(path.join(absoluteFolderPath, csvFile), "utf8");
                }

                algorithmResults.push({
                  ...parsedJson,
                  outputFile: file,
                  outputCsv: csvContent
                });
              } catch (e) {
                logDebug(`Failed to parse JSON ${file}: ${e}`);
              }
            }
          }
          
          payload.algorithmResults = algorithmResults;
          
          if (algorithmResults.length > 0) {
            const bestRun = algorithmResults.reduce((prev, current) => (prev.score < current.score) ? prev : current);
            payload.outputFileName = bestRun.outputFile;
            payload.outputCsv = bestRun.outputCsv;
          }
        } catch (err) {
          logDebug(`Failed to read folder content: ${err}`);
        }
      }

      res.status(runResult.exitCode === 0 ? 200 : 500).json(payload);
    } catch (error) {
      const message =
        error instanceof Error ? error.message : "Unknown server error";
      res.status(500).json({
        ok: false,
        message,
      });
    } finally {
      if (caseDir) {
        await fs.rm(caseDir, { recursive: true, force: true });
      }
    }
  },
);

app.listen(port, "0.0.0.0", () => {
  console.log(`Solver API listening on http://0.0.0.0:${port}`);
});
