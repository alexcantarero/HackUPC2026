#!/bin/bash

# --- Mecalux Warehouse Optimizer: Robust Startup & Tunneling ---

# 1. Setup
BLUE='\033[0;34m'
GREEN='\033[0;32m'
ORANGE='\033[0;33m'
RED='\033[0;31m'
NC='\033[0m'

echo -e "${BLUE}Starting Mecalux Warehouse Optimizer...${NC}"

# 2. Compile Backend Solver if needed
if [ ! -f "backend/bin/solver" ]; then
    echo -e "${ORANGE}Building C++ solver...${NC}"
    (cd backend && make)
    if [ $? -ne 0 ]; then
        echo -e "${RED}Error: Backend compilation failed.${NC}"
        exit 1
    fi
fi

# 3. Install Frontend dependencies if needed
if [ ! -d "frontend/node_modules" ]; then
    echo -e "${ORANGE}Installing frontend dependencies...${NC}"
    (cd frontend && npm install)
fi

# 4. Start API and Web Server
echo -e "${GREEN}Initializing servers...${NC}"
cd frontend

# Remove old logs
rm -f api.log web.log tunnel.log

# Launch API (Listening on all interfaces)
npm run api > api.log 2>&1 &
API_PID=$!

# Launch Vite (Listening on all interfaces)
npm run dev -- --host > web.log 2>&1 &
VITE_PID=$!

# 5. Wait for readiness
echo -e "${BLUE}Waiting for servers to wake up...${NC}"
READY=0
for i in {1..15}; do
    if grep -q "Local:" web.log || grep -q "Network:" web.log; then
        READY=1
        break
    fi
    sleep 1
    echo -n "."
done
echo ""

if [ $READY -eq 1 ]; then
    # Detect the most likely Hotspot IP (usually 172.20.10.x)
    LOCAL_IP=$(ip addr show | grep 'inet ' | grep -v '127.0.0.1' | awk '{print $2}' | cut -d/ -f1 | head -n 1)
    HOTSPOT_IP=$(ip addr show | grep '172.20.10.' | awk '{print $2}' | cut -d/ -f1 | head -n 1)
    
    DISPLAY_IP=${HOTSPOT_IP:-$LOCAL_IP}

    echo -e "\n${BLUE}--------------------------------------------------${NC}"
    echo -e "${GREEN}  APPLICATION READY!${NC}"
    echo -e "${BLUE}  Local:   ${NC}${ORANGE}http://localhost:5173${NC}"
    echo -e "${BLUE}  Hotspot: ${NC}${ORANGE}http://${DISPLAY_IP}:5173${NC}"
    echo -e "${BLUE}--------------------------------------------------${NC}"
    
    # 6. Optional Tunnel (Bypasses Firewalls/Hotspot issues)
    echo -e "${ORANGE}Tip: If the IP above doesn't work on your phone,${NC}"
    echo -e "${ORANGE}     press 't' to start a public Cloudflare Tunnel.${NC}"
    echo -e "Press Ctrl+C to stop everything."

    # Handle input for tunnel
    cleanup() {
        echo -e "\n${BLUE}Shutting down...${NC}"
        kill $API_PID $VITE_PID $TUNNEL_PID 2>/dev/null
        exit
    }
    trap cleanup SIGINT SIGTERM

    while true; do
        read -t 1 -n 1 key
        if [[ $key == "t" ]]; then
            echo -e "\n${GREEN}Starting Cloudflare Tunnel (via npx)...${NC}"
            # Use --yes to skip installation prompt and --url to specify local server
            npx --yes cloudflared tunnel --url http://localhost:5173 > tunnel.log 2>&1 &
            TUNNEL_PID=$!
            
            echo -e "${BLUE}Waiting for tunnel URL...${NC}"
            TUNNEL_URL=""
            for i in {1..30}; do
                echo -n "."
                TUNNEL_URL=$(grep -oE "https://[a-zA-Z0-9.-]+\.trycloudflare\.com" tunnel.log | head -n 1)
                if [ -n "$TUNNEL_URL" ]; then
                    break
                fi
                sleep 1
            done
            echo ""

            if [ -n "$TUNNEL_URL" ]; then
                echo -e "${GREEN}  TUNNEL READY!${NC}"
                echo -e "${BLUE}  Public URL: ${NC}${ORANGE}${TUNNEL_URL}${NC}"
                echo -e "${ORANGE}  Scan the QR code on the website now!${NC}"
            else
                echo -e "${RED}  Failed to get tunnel URL after 30s. Check tunnel.log${NC}"
                # Keep the process running in case it's still starting, but notify user
            fi
        fi
    done
else
    echo -e "${RED}Error: Web server failed to start.${NC}"
    tail -n 5 web.log
    kill $API_PID $VITE_PID 2>/dev/null
    exit 1
fi
wait
