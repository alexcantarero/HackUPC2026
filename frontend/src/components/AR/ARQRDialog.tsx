import { Dialog, DialogContent, DialogTitle, IconButton } from "@mui/material";
import CloseIcon from "@mui/icons-material/Close";
import { QRCodeSVG } from "qrcode.react";

interface ARQRDialogProps {
  open: boolean;
  onClose: () => void;
}

export default function ARQRDialog({ open, onClose }: ARQRDialogProps) {
  // Use the current window location so it works with local IPs, hotspots, or tunnels (ngrok/localtunnel)
  const arUrl = window.location.origin;

  return (
    <Dialog open={open} onClose={onClose}>
      <DialogTitle sx={{ m: 0, p: 2 }}>
        Scan for Hologram
        <IconButton
          onClick={onClose}
          sx={{ position: 'absolute', right: 8, top: 8 }}
        >
          <CloseIcon />
        </IconButton>
      </DialogTitle>
      <DialogContent sx={{ textAlign: 'center', pb: 4, px: 6 }}>
        <QRCodeSVG value={arUrl} size={256} />
        <p style={{ marginTop: '1rem', color: '#666' }}>
          Scan with your phone (must be on the same WiFi) to project the warehouse on your table!
        </p>
        <code style={{ fontSize: '0.8rem', color: '#888' }}>{arUrl}</code>
      </DialogContent>
    </Dialog>
  );
}
