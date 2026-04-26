#!/bin/bash

# --- Mecalux Warehouse Optimizer: Robust Startup ---

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
    echo -e "${ORANGE}Installing frontend dependencies (this may take a minute)...${NC}"
    (cd frontend && npm install)
fi

# 4. Start API and Web Server
echo -e "${GREEN}Initializing servers...${NC}"
cd frontend

# Remove old logs
rm -f api.log web.log

# Launch API
npm run api > api.log 2>&1 &
API_PID=$!

# Launch Vite
npm run dev -- --host > web.log 2>&1 &
VITE_PID=$!

# 5. Wait for readiness
echo -e "${BLUE}Waiting for ports to open...${NC}"
READY=0
for i in {1..20}; do
    if grep -q "Local:" web.log || grep -q "Network:" web.log; then
        READY=1
        break
    fi
    sleep 1
    echo -n "."
done
echo ""

if [ $READY -eq 1 ]; then
    # Extract the local IP address for the user
    LOCAL_IP=$(ip addr show | grep 'inet ' | grep -v '127.0.0.1' | awk '{print $2}' | cut -d/ -f1 | head -n 1)
    
    echo -e "\n${BLUE}--------------------------------------------------${NC}"
    echo -e "${GREEN}  APPLICATION READY!${NC}"
    echo -e "${BLUE}  Local:   ${NC}${ORANGE}http://localhost:5173${NC}"
    echo -e "${BLUE}  Network: ${NC}${ORANGE}http://${LOCAL_IP}:5173${NC}"
    echo -e "${BLUE}--------------------------------------------------${NC}"
    echo -e "Press Ctrl+C to stop both servers."
else
    echo -e "${RED}Error: Web server failed to start.${NC}"
    echo -e "Last 5 lines of web.log:"
    tail -n 5 web.log
    kill $API_PID $VITE_PID 2>/dev/null
    exit 1
fi

cleanup() {
    echo -e "\n${BLUE}Shutting down...${NC}"
    kill $API_PID $VITE_PID 2>/dev/null
    exit
}

trap cleanup SIGINT SIGTERM
wait
