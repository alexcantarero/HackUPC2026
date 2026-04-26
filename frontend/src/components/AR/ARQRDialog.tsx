import { Dialog, DialogContent, DialogTitle, IconButton } from "@mui/material";
import CloseIcon from "@mui/icons-material/Close";
import { QRCodeSVG } from "qrcode.react";

interface ARQRDialogProps {
  open: boolean;
  onClose: () => void;
}

export default function ARQRDialog({ open, onClose }: ARQRDialogProps) {
  // Automatically detect the origin (works for localhost, IP, or Tunnels)
  const arUrl = typeof window !== 'undefined' ? window.location.origin : "";

  return (
    <Dialog open={open} onClose={onClose} maxWidth="xs" fullWidth>
      <DialogTitle sx={{ m: 0, p: 2, display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
        Scan for Mobile View
        <IconButton onClick={onClose}>
          <CloseIcon />
        </IconButton>
      </DialogTitle>
      <DialogContent sx={{ textAlign: 'center', pb: 4, pt: 2 }}>
        <div style={{ background: 'white', padding: '16px', borderRadius: '12px', display: 'inline-block' }}>
          <QRCodeSVG value={arUrl} size={256} />
        </div>
        <p style={{ marginTop: '1.5rem', color: '#666', fontSize: '0.9rem' }}>
          Scan with your phone to visualize the warehouse layout on your mobile device.
        </p>
        <code style={{ fontSize: '0.8rem', background: '#eee', padding: '4px 8px', borderRadius: '4px' }}>
          {arUrl}
        </code>
      </DialogContent>
    </Dialog>
  );
}
