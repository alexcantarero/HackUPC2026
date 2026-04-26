import type { AlgorithmResultDisplay } from "../../types/solver";

interface AlgorithmResultsProps {
  results: AlgorithmResultDisplay[];
  selectedAlgo: string | null;
  onViewLayout: (algo: AlgorithmResultDisplay) => void;
  onClear: () => void;
}

export default function AlgorithmResults({
  results,
  selectedAlgo,
  onViewLayout,
  onClear,
}: AlgorithmResultsProps) {
  if (results.length === 0) return null;

  const isMobile = typeof window !== 'undefined' && window.innerWidth < 960;

  return (
    <div
      className="solver-results-panel"
      role="region"
      aria-label="Algorithm results"
    >
      <div className="solver-panel-header">
        <h2>Algorithm Results</h2>
        {!isMobile && (
          <button
            className="solver-close"
            onClick={onClear}
            title="Clear results"
          >
            ×
          </button>
        )}
      </div>
      <div className="solver-comparison" aria-label="Algorithm run summaries">
        {results.map((result, idx) => (
          <article key={`${result.algorithm}-${idx}`} className="solver-comparison-card">
            <div className="solver-comparison-card-header">
              <h3>{result.title}</h3>
            </div>
            <div className="solver-comparison-metrics">
              <div>
                <span>Best score</span>
                <strong>{result.bestScore}</strong>
              </div>
              <div>
                <span>Runtime</span>
                <strong>{result.runtimeMs}</strong>
              </div>
            </div>
            <p className="solver-comparison-note">{result.note}</p>
            <button
              type="button"
              className={`solver-view-layout-btn ${selectedAlgo === result.algorithm ? "active" : ""}`}
              onClick={() => onViewLayout(result)}
            >
              {selectedAlgo === result.algorithm ? "Viewing Layout" : "View Layout"}
            </button>
          </article>
        ))}
      </div>
    </div>
  );
}
