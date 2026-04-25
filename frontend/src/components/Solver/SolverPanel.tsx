import React from "react";
import type { RequiredCsvField } from "../../types/solver";

interface SolverPanelProps {
  onSubmit: (event: React.FormEvent<HTMLFormElement>) => void;
  onFileChange: (field: RequiredCsvField, file: File | null) => void;
  isSubmitting: boolean;
  error: string;
}

export default function SolverPanel({
  onSubmit,
  onFileChange,
  isSubmitting,
  error,
}: SolverPanelProps) {
  return (
    <div className="solver-panel" role="dialog" aria-label="Solver upload">
      <div className="solver-panel-header">
        <h2>Solver</h2>
      </div>
      <form className="solver-form" onSubmit={onSubmit}>
        <label>
          warehouse.csv
          <input
            type="file"
            accept=".csv,text/csv"
            onChange={(event) =>
              onFileChange("warehouse", event.target.files?.[0] ?? null)
            }
          />
        </label>
        <label>
          obstacles.csv
          <input
            type="file"
            accept=".csv,text/csv"
            onChange={(event) =>
              onFileChange("obstacles", event.target.files?.[0] ?? null)
            }
          />
        </label>
        <label>
          ceiling.csv
          <input
            type="file"
            accept=".csv,text/csv"
            onChange={(event) =>
              onFileChange("ceiling", event.target.files?.[0] ?? null)
            }
          />
        </label>
        <label>
          types_of_bays.csv
          <input
            type="file"
            accept=".csv,text/csv"
            onChange={(event) =>
              onFileChange("types_of_bays", event.target.files?.[0] ?? null)
            }
          />
        </label>
        <button type="submit" disabled={isSubmitting}>
          {isSubmitting ? "Running solver..." : "Upload and run"}
        </button>
      </form>
      {error && <p className="solver-error">{error}</p>}
    </div>
  );
}
