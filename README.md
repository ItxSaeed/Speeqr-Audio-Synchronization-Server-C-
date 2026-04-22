#  Speeqr — Day 1

A lightweight C++ server implementing a custom **NTP-like clock synchronization** system with a real-time web interface.

---

##  Features
- NTP-style offset calculation (T1, T2, T3, T4)
- High-precision timestamps (sub-ms)
- EMA smoothing (α = 0.1)
- WebSocket communication
- Live offset display in browser

---

##  Requirements
- C++17
- CMake ≥ 3.20
- Boost libraries

---

##  Install

### Ubuntu
```bash
sudo apt install build-essential cmake libboost-all-dev

###  Build & Run
mkdir build && cd build
cmake --build . --config Release
.\Release\speeqr_server.exe


###  Open browser
http://localhost:8080
