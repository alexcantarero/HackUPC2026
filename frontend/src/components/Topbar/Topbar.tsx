import "./Topbar.css";
import FileUploadIcon from "@mui/icons-material/FileUpload";
import VideocamIcon from "@mui/icons-material/Videocam";
import ViewInArIcon from "@mui/icons-material/ViewInAr";
import { Button } from "@mui/material";

type TopbarProps = {
  onToggleCameraView: () => void;
  onToggleGaps: () => void;
  onOpenSolverPanel: () => void;
  showGaps: boolean;
};

export default function Topbar({
  onToggleCameraView,
  onToggleGaps,
  onOpenSolverPanel,
  showGaps,
}: TopbarProps) {
  const isMobile = typeof window !== 'undefined' && 'ontouchstart' in window;

  return (
    <div className="topbar">
      <Button
        variant="contained"
        sx={{ 
          backgroundColor: showGaps ? "#ed8200" : "#288aed",
          "&:hover": { backgroundColor: showGaps ? "#d67500" : "#2176c7" }
        }}
        onClick={onToggleGaps}
      >
        <ViewInArIcon />
      </Button>
      <Button
        variant="contained"
        className="bubble"
        sx={{ backgroundColor: "#288aed" }}
        onClick={onToggleCameraView}
      >
        <VideocamIcon />
      </Button>
      {!isMobile && (
        <Button 
          variant="contained" 
          sx={{ backgroundColor: "#288aed" }}
          onClick={onOpenSolverPanel}
        >
          <FileUploadIcon />
        </Button>
      )}
    </div>
  );
}
