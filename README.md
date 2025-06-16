# MemSC

## Build instructions

`cmake -B build -DCMAKE_INSTALL_PREFIX=<prefix> && cmake --build build -j`

## Installing
`cmake --install build`

## To run

`./build/memsc`

## To use without sudo
`echo 0 | sudo tee /proc/sys/kernel/yama/ptrace_scope`
