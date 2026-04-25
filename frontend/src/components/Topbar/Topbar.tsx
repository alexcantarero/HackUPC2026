import "./Topbar.css";
import FileUploadIcon from "@mui/icons-material/FileUpload";
import ClearAllIcon from "@mui/icons-material/ClearAll";
import VideocamIcon from "@mui/icons-material/Videocam";
import ViewInArIcon from "@mui/icons-material/ViewInAr";
import {
  Button,
  FormControl,
  FormControlLabel,
  FormLabel,
  Menu,
  Radio,
  RadioGroup,
} from "@mui/material";
import { useState } from "react";

type TopbarProps = {
  caseNumber: number;
  onCaseChange: (caseNumber: number) => void;
  onToggleCameraView: () => void;
  onToggleGaps: () => void;
  onOpenSolverPanel: () => void;
  showGaps: boolean;
};

export default function Topbar({
  caseNumber,
  onCaseChange,
  onToggleCameraView,
  onToggleGaps,
  onOpenSolverPanel,
  showGaps,
}: TopbarProps) {
  const [casesMenuAnchor, setCasesMenuAnchor] = useState<null | HTMLElement>(
    null,
  );

  const isCasesMenuOpen = Boolean(casesMenuAnchor);

  const openCasesMenu = (event: React.MouseEvent<HTMLElement>) => {
    setCasesMenuAnchor(event.currentTarget);
  };

  const closeCasesMenu = () => {
    setCasesMenuAnchor(null);
  };

  const handleCaseChange = (event: React.ChangeEvent<HTMLInputElement>) => {
    onCaseChange(Number(event.target.value));
    closeCasesMenu();
  };

  return (
    <div className="topbar">
      <div className="leading-tooltip">Mecalux Bay Distributor</div>
      <div className="central-functions"></div>
      <div className="trailing-functions">
        <Button
          variant="contained"
          sx={{
            backgroundColor: showGaps ? "#ed8200" : "#288aed",
            "&:hover": { backgroundColor: showGaps ? "#d67500" : "#2176c7" },
          }}
          onClick={onToggleGaps}
        >
          <ViewInArIcon />
        </Button>
        <Button
          variant="contained"
          sx={{ backgroundColor: "#288aed" }}
          onClick={onOpenSolverPanel}
        >
          <FileUploadIcon />
        </Button>
        <Button
          variant="contained"
          className="bubble"
          sx={{ backgroundColor: "#288aed" }}
          onClick={onToggleCameraView}
        >
          <VideocamIcon />
        </Button>
        <Button
          variant="contained"
          className="bubble"
          sx={{ backgroundColor: "#288aed" }}
          onClick={openCasesMenu}
        >
          <ClearAllIcon sx={{}} />
        </Button>
        <Menu
          anchorEl={casesMenuAnchor}
          open={isCasesMenuOpen}
          onClose={closeCasesMenu}
          anchorOrigin={{ vertical: "bottom", horizontal: "right" }}
          transformOrigin={{ vertical: "top", horizontal: "right" }}
        >
          <FormControl sx={{ px: 2, py: 1 }}>
            <FormLabel id="case-radio-group-label">Display case</FormLabel>
            <RadioGroup
              aria-labelledby="case-radio-group-label"
              name="case-radio-group"
              value={String(caseNumber)}
              onChange={handleCaseChange}
            >
              <FormControlLabel value="0" control={<Radio />} label="Case 0" />
              <FormControlLabel value="1" control={<Radio />} label="Case 1" />
              <FormControlLabel value="2" control={<Radio />} label="Case 2" />
              <FormControlLabel value="3" control={<Radio />} label="Case 3" />
            </RadioGroup>
          </FormControl>
        </Menu>
      </div>
    </div>
  );
}
