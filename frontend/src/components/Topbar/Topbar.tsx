import "./Topbar.css";
import FileUploadIcon from "@mui/icons-material/FileUpload";
import ClearAllIcon from "@mui/icons-material/ClearAll";
import VideocamIcon from "@mui/icons-material/Videocam";
import { Button } from "@mui/material";

type TopbarProps = {
  onToggleCameraView: () => void;
};

export default function Topbar({ onToggleCameraView }: TopbarProps) {
  return (
    <div className="topbar">
      <div className="leading-tooltip">Mecalux Bay Distributor</div>
      <div className="central-functions"></div>
      <div className="trailing-functions">
        <Button variant="contained" sx={{ backgroundColor: "#288aed" }}>
          <FileUploadIcon />
        </Button>
        <Button
          variant="contained"
          className="bubble"
          sx={{ backgroundColor: "#288aed" }}
        >
          <ClearAllIcon sx={{}} />
        </Button>
        <Button
          variant="contained"
          className="bubble"
          sx={{ backgroundColor: "#288aed" }}
          onClick={onToggleCameraView}
        >
          <VideocamIcon />
        </Button>
      </div>
    </div>
  );
}
