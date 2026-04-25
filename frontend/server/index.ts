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

function parseBestScore(stdout: string): number | null {
  const match = stdout.match(/Best solution:\s*score=([-+]?\d*\.?\d+(?:[eE][-+]?\d+)?)/);
  if (!match) return null;
  const score = Number(match[1]);
  return Number.isFinite(score) ? score : null;
}

async function listOutputFiles(outputDir: string): Promise<Map<string, number>> {
  const result = new Map<string, number>();
  let names: string[];
  try {
    names = await fs.readdir(outputDir);
  } catch {
    return result;
  }

  await Promise.all(
    names.map(async (name) => {
      if (!name.endsWith(".csv")) return;
      const fullPath = path.join(outputDir, name);
      try {
        const stat = await fs.stat(fullPath);
        result.set(name, stat.mtimeMs);
      } catch {
        // Ignore files removed between readdir and stat.
      }
    }),
  );

  return result;
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
    const outputDir = path.join(workspaceRoot, "data", "output");

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
    const beforeFiles = await listOutputFiles(outputDir);

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

      const afterFiles = await listOutputFiles(outputDir);
      let outputFileName: string | null = null;
      let newestMtime = -1;

      for (const [name, mtimeMs] of afterFiles.entries()) {
        const previousMtime = beforeFiles.get(name) ?? -1;
        if (mtimeMs <= previousMtime || mtimeMs < startedAt) continue;
        if (mtimeMs > newestMtime) {
          newestMtime = mtimeMs;
          outputFileName = name;
        }
      }

      let outputCsv: string | null = null;
      if (includeOutputCsv && outputFileName) {
        const outputPath = path.join(outputDir, outputFileName);
        try {
          outputCsv = await fs.readFile(outputPath, "utf8");
        } catch {
          outputCsv = null;
        }
      }

      const payload: SolveResponse = {
        ok: runResult.exitCode === 0,
        message:
          runResult.exitCode === 0
            ? "Solver finished successfully"
            : "Solver finished with errors",
        stdout: runResult.stdout,
        stderr: runResult.stderr,
        bestScore: parseBestScore(runResult.stdout),
        outputFileName,
        outputCsv,
        csvInputs,
        exitCode: runResult.exitCode,
        durationMs: Date.now() - startedAt,
      };

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

app.listen(port, () => {
  console.log(`Solver API listening on http://localhost:${port}`);
});
