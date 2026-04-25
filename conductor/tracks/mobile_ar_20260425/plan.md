# Implementation Plan: Mobile AR Hologram Integration

## Phase 1: Environment Setup & Dependencies
- [ ] Task: Install `@react-three/xr` and `qrcode.react` dependencies.
- [ ] Task: Update Vite configuration to allow access from local IP (hotspot network).
- [ ] Task: Conductor - User Manual Verification 'Phase 1: Environment Setup & Dependencies' (Protocol in workflow.md)

## Phase 2: Dynamic QR Code Modal
- [ ] Task: Create `ARQRDialog.tsx` component.
    - [ ] Add `qrcode.react` to generate a scannable QR code.
    - [ ] Implement logic to dynamically fetch the laptop's current local IP (e.g., `172.20.10.x`) and append the Vite server port.
    - [ ] Render a Material-UI Dialog containing the QR Code and instructional text.
- [ ] Task: Update `Topbar.tsx` with a new "QR Code" button.
    - [ ] Add the button to the trailing functions group.
    - [ ] Pass necessary props (`showAR`, `onToggleAR`) to toggle the `ARQRDialog`.
- [ ] Task: Update `App.tsx` state to manage the `ARQRDialog` visibility.
- [ ] Task: Conductor - User Manual Verification 'Phase 2: Dynamic QR Code Modal' (Protocol in workflow.md)

## Phase 3: Mobile UI Responsiveness
- [ ] Task: Update CSS to conditionally hide Topbar elements on mobile devices.
    - [ ] Use CSS Media Queries (`@media (max-width: ...)`) to hide the "Mecalux Bay Distributor" title.
    - [ ] Use CSS Media Queries to hide the "Upload" button.
    - [ ] Use CSS Media Queries to hide the "QR Code" button.
    - [ ] Ensure remaining buttons are centered correctly on small screens.
- [ ] Task: Conductor - User Manual Verification 'Phase 3: Mobile UI Responsiveness' (Protocol in workflow.md)

## Phase 4: AR Hologram Core (WebXR)
- [ ] Task: Refactor `App.tsx` to support `@react-three/xr`.
    - [ ] Replace standard `@react-three/fiber` Canvas setup with `XR` store and `<XR>` provider.
    - [ ] Wrap 3D elements inside a `<Suspense>` boundary to prevent white-screen crashes on slow network loads.
- [ ] Task: Implement the "PROJECT ON TABLE (AR)" mobile button.
    - [ ] Use CSS Media Queries to ensure the button is only visible on mobile screens.
    - [ ] Bind the button to `store.enterAR()` to launch the WebXR session.
- [ ] Task: Scale the warehouse for tabletop AR projection.
    - [ ] Create a specific AR container group or utilize the export logic to drastically scale down the warehouse size (to ~20cm).
- [ ] Task: Conductor - User Manual Verification 'Phase 4: AR Hologram Core (WebXR)' (Protocol in workflow.md)

## Phase 5: Native iOS AR (USDZ)
- [ ] Task: Implement `generateUSDZ` function in `App.tsx`.
    - [ ] Utilize `USDZExporter` from `three/examples/jsm/exporters/USDZExporter.js`.
    - [ ] Clone the main warehouse group to safely apply AR-specific materials (DoubleSide) without breaking the web view.
    - [ ] Filter out the ceiling and ceiling steps from the cloned group before export.
    - [ ] Scale the cloned group down to ~20cm.
    - [ ] Generate the `.usdz` blob and automatically trigger a download/open link.
- [ ] Task: Add User-Agent sniffing for iOS Safari in `App.tsx`.
    - [ ] Conditionally render the "PROJECT ON TABLE (iOS AR)" button for iPhone users.
    - [ ] Bind the button to the `generateUSDZ` function to launch AR Quick Look.
- [ ] Task: Conductor - User Manual Verification 'Phase 5: Native iOS AR (USDZ)' (Protocol in workflow.md)