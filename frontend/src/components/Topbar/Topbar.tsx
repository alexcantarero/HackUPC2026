import "./Topbar.css";
import FileUploadIcon from "@mui/icons-material/FileUpload";
import ClearAllIcon from "@mui/icons-material/ClearAll";
import { Button } from "@mui/material";

export default function Topbar() {
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
        >
          <FileUploadIcon />
        </Button>
      </div>
    </div>
  );
}
