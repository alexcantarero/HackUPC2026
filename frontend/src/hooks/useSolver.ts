import { useState, useCallback } from "react";
import type { FormEvent } from "react";
import type { RequiredCsvField, SolveResponse, AlgorithmResultDisplay } from "../types/solver";

export function useSolver() {
  const [uploadFiles, setUploadFiles] = useState<Partial<Record<RequiredCsvField, File>>>({});
  const [isSubmitting, setIsSubmitting] = useState(false);
  const [error, setError] = useState("");
  const [result, setResult] = useState<SolveResponse | null>(null);
  const [comparisonResults, setComparisonResults] = useState<AlgorithmResultDisplay[]>([]);
  const [activeLayoutCsv, setActiveLayoutCsv] = useState<string | null>(null);
  const [selectedAlgoName, setSelectedAlgoName] = useState<string | null>(null);

  const loadResultById = useCallback(async (id: string) => {
    setIsSubmitting(true);
    setError("");
    try {
      const response = await fetch(`/api/results/${id}`);
      const payload = await response.json();
      if (!response.ok || !payload.ok) throw new Error("Failed to load result");

      if (payload.algorithmResults && payload.algorithmResults.length > 0) {
        const results = payload.algorithmResults.map((ar: any) => ({
          algorithm: ar.algorithm_name,
          title: ar.algorithm_name.replace("_", " ").toUpperCase(),
          status: "success",
          bestScore: ar.score.toLocaleString(),
          rawScore: ar.score,
          runtimeMs: `${Math.round(ar.time_took_to_find_best_sol * 1000)} ms`,
          outputFile: ar.outputFile || "N/A",
          note: `Placed ${ar.number_of_bays} bays`,
          outputCsv: ar.outputCsv,
        }));
        results.sort((a: any, b: any) => (a.rawScore || 0) - (b.rawScore || 0));
        setComparisonResults(results);
        
        // Auto-select best
        const best = results[0];
        setActiveLayoutCsv(best.outputCsv);
        setSelectedAlgoName(best.algorithm);
        setResult(payload); // Now includes full csvInputs
      }
    } catch (err) {
      setError("Failed to sync shared layout.");
    } finally {
      setIsSubmitting(false);
    }
  }, []);

  const setUploadFile = useCallback((field: RequiredCsvField, file: File | null) => {
    setUploadFiles((prev) => {
      if (!file) {
        const next = { ...prev };
        delete next[field];
        return next;
      }
      return { ...prev, [field]: file };
    });
  }, []);

  const submitRequest = useCallback(async (event: FormEvent<HTMLFormElement>) => {
    event.preventDefault();

    const requiredFields: RequiredCsvField[] = ["warehouse", "obstacles", "ceiling", "types_of_bays"];

    for (const field of requiredFields) {
      if (!uploadFiles[field]) {
        setError(`Missing file: ${field}.csv`);
        return;
      }
    }

    setError("");
    setResult(null);
    setActiveLayoutCsv(null);
    setSelectedAlgoName(null);
    setIsSubmitting(true);

    const formData = new FormData();
    for (const field of requiredFields) {
      formData.append(field, uploadFiles[field] as File);
    }
    formData.append("includeOutputCsv", "true");
    formData.append("mode", "multiple");

    try {
      const apiBase = import.meta.env.VITE_SOLVER_API_URL ?? "";
      const response = await fetch(`${apiBase}/api/solve`, {
        method: "POST",
        body: formData,
      });

      const payload = (await response.json()) as SolveResponse & { message?: string };
      console.log("[useSolver] API Response:", payload);

      if (!response.ok || !payload.ok) {
        setError(payload.message ?? "Solver request failed");
        setResult(null);
        return;
      }

      if (payload.algorithmResults && payload.algorithmResults.length > 0) {
        const results = payload.algorithmResults.map((ar) => ({
          algorithm: ar.algorithm_name,
          title: ar.algorithm_name.replace("_", " ").toUpperCase(),
          status: "success",
          bestScore: ar.score.toLocaleString(),
          rawScore: ar.score,
          runtimeMs: `${Math.round(ar.time_took_to_find_best_sol * 1000)} ms`,
          outputFile: ar.outputFile || "N/A",
          note: `Placed ${ar.number_of_bays} bays with training score ${ar.training_score.toFixed(2)}`,
          outputCsv: ar.outputCsv,
        }));
        
        results.sort((a, b) => (a.rawScore || 0) - (b.rawScore || 0));
        setComparisonResults(results);
      }

      setResult(payload);
      if (payload.outputCsv) {
        setActiveLayoutCsv(payload.outputCsv);
        
        // Find the best run to highlight it as "Viewing Layout"
        if (payload.algorithmResults && payload.algorithmResults.length > 0) {
          const bestRun = payload.algorithmResults.reduce((prev, current) => 
            (prev.score < current.score) ? prev : current
          );
          setSelectedAlgoName(bestRun.algorithm_name);
        }
      }
    } catch {
      setError("Unable to reach solver API.");
    } finally {
      setIsSubmitting(false);
    }
  }, [uploadFiles]);

  return {
    uploadFiles,
    setUploadFile,
    isSubmitting,
    error,
    result,
    comparisonResults,
    activeLayoutCsv,
    setActiveLayoutCsv,
    selectedAlgoName,
    setSelectedAlgoName,
    submitRequest,
    setComparisonResults,
    loadResultById,
  };
}
