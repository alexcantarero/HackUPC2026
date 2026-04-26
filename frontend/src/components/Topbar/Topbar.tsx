import "./Topbar.css";
import FileUploadIcon from "@mui/icons-material/FileUpload";
import VideocamIcon from "@mui/icons-material/Videocam";
import ViewInArIcon from "@mui/icons-material/ViewInAr";
import QrCodeIcon from "@mui/icons-material/QrCode";
import { Button } from "@mui/material";

type TopbarProps = {
  onToggleCameraView: () => void;
  onToggleGaps: () => void;
  onOpenSolverPanel: () => void;
  onToggleAR: () => void;
  showGaps: boolean;
  showAR: boolean;
};

export default function Topbar({
  onToggleCameraView,
  onToggleGaps,
  onOpenSolverPanel,
  onToggleAR,
  showGaps,
  showAR,
}: TopbarProps) {
  const isMobile = typeof window !== 'undefined' && 'ontouchstart' in window;

  return (
    <div className="topbar">
      {!isMobile && (
        <Button
          variant="contained"
          sx={{ 
            backgroundColor: showAR ? "#ff5722" : "rgba(255, 255, 255, 0.05)",
            "&:hover": { backgroundColor: showAR ? "#d67500" : "rgba(255, 255, 255, 0.12)" }
          }}
          onClick={onToggleAR}
        >
          <QrCodeIcon />
        </Button>
      )}
      <Button
        variant="contained"
        sx={{ 
          backgroundColor: showGaps ? "#ed8200" : "rgba(255, 255, 255, 0.05)",
          "&:hover": { backgroundColor: showGaps ? "#d67500" : "rgba(255, 255, 255, 0.12)" }
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
