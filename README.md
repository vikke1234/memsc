# MemSC

## Build instructions

`cmake -B build && cmake --build build -j`

## To run

`./build/memsc`

## To use without sudo
`echo 0 | sudo tee /proc/sys/kernel/yama/ptrace_scope`
