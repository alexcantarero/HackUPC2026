# Specification: Mobile AR Hologram Integration

## Overview
Reimplement the AR hologram feature from scratch on the `improve-frontend` branch. This track introduces a new QR code button in the desktop GUI that allows users (specifically hackathon judges) to open the web application on their mobile devices. The mobile view will feature a streamlined GUI and an AR mode that projects a scaled-down (20cm) 3D hologram of the warehouse (excluding ceiling and ceiling steps), obstacles, and bays onto a table. The AR implementation must support all major browsers by utilizing both WebXR (for Android/Chrome) and USDZ exports (for native iOS Safari).

## Functional Requirements
1.  **Desktop GUI Updates:**
    *   Add a new "QR Code" button to the Topbar.
    *   Clicking the button displays a modal with a QR code.
    *   The QR code URL must be dynamically generated to match the local IP address of the laptop on the mobile hotspot network (e.g., `172.20.10.x`), ensuring any device on the hotspot can access the server.
2.  **Mobile GUI Updates (via CSS Media Queries):**
    *   Hide the "Mecalux Bay Distributor" title text.
    *   Hide the "Upload" button.
    *   Hide the newly added "QR Code" button.
    *   Display a large, prominently styled "View in AR" button at the bottom of the screen.
3.  **AR Hologram Mode:**
    *   **Trigger:** Clicking the "View in AR" button on mobile initiates the AR experience.
    *   **Content:** The hologram must include the warehouse floor, walls, obstacles, and bays. The ceiling and ceiling steps must be explicitly excluded from the AR view.
    *   **Scale:** The 3D model must be drastically scaled down so its maximum physical footprint is approximately 20cm x 20cm (tabletop size).
4.  **Cross-Browser AR Support:**
    *   **Android/Chrome:** Use `@react-three/xr` to handle standard WebXR sessions.
    *   **iOS Safari:** Detect iOS Safari via User-Agent and conditionally use `USDZExporter` from `three/examples/jsm/exporters/USDZExporter` to generate a `.usdz` file on the fly, triggering Apple's native AR Quick Look.

## Acceptance Criteria
- [ ] A user clicking the QR button on desktop sees a valid QR code pointing to the hotspot IP.
- [ ] Scanning the QR code on a mobile device connected to the hotspot opens the application.
- [ ] On mobile devices (simulated via CSS width), the topbar title, upload button, and QR button are hidden.
- [ ] On mobile devices, a "View in AR" button is visible.
- [ ] Clicking "View in AR" on an Android device launches a WebXR session displaying a ~20cm table-top hologram.
- [ ] Clicking "View in AR" on an iPhone (Safari) downloads/opens a `.usdz` file in AR Quick Look displaying a ~20cm table-top hologram.
- [ ] The AR hologram does not display the warehouse ceiling or ceiling steps.
- [ ] The desktop view remains completely unaffected by the mobile UI changes and AR scaling.

## Out of Scope
*   Full VR (Virtual Reality) navigation.
*   Deploying a public cloud tunnel (e.g., Cloudflare, ngrok) - the solution relies strictly on the mobile hotspot network.