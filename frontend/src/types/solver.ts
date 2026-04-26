export type RequiredCsvField = "warehouse" | "obstacles" | "ceiling" | "types_of_bays";

export type SolveResponse = {
  ok: boolean;
  message: string;
  stdout: string;
  stderr: string;
  bestScore: number | null;
  outputFileName: string | null;
  outputCsv: string | null;
  resultId?: string | null;
  csvInputs: Partial<Record<RequiredCsvField, string>>;
  exitCode: number;
  durationMs: number;
  debug?: string[];
  algorithmResults?: Array<{
    algorithm_name: string;
    score: number;
    training_score: number;
    time_took_to_find_best_sol: number;
    number_of_bays: number;
    outputFile?: string;
    outputCsv?: string;
  }>;
};

export type AlgorithmResultDisplay = {
  algorithm: string;
  title: string;
  status: string;
  bestScore: string;
  rawScore?: number;
  runtimeMs: string;
  outputFile: string;
  note: string;
  outputCsv?: string | null;
};
