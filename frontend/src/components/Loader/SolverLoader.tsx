import './SolverLoader.css';

export default function SolverLoader() {
  return (
    <div className="solver-loader-overlay">
      <div className="solver-loader-container">
        <h2 className="solver-loader-text">
          Sorting the Warehouse<span>.</span><span>.</span><span>.</span>
        </h2>
        <div className="solver-loader-bar-bg">
          <div className="solver-loader-bar"></div>
        </div>
        <div className="solver-loader-subtitle">running searches</div>
      </div>
    </div>
  );
}
