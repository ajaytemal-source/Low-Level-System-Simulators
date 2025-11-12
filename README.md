# CPU & Memory Simulator

This project is a cycle-accurate CPU and cache simulator written in C++. It models a low-level processor architecture (E20 Processor) and configurable caches (E20 Cache), providing a platform to study instruction execution, memory access, and caching behavior. The E20 assembly language used by this simulator was originally developed by NYU Tandon for educational purposes.

## Features

- **E20 Processor Simulation**  
  - Implements arithmetic, logic, branching, memory, and jump instructions.  
  - Maintains program counter, general-purpose registers, and memory state.  

- **E20 Cache Simulation**  
  - Supports configurable L1 and optional L2 caches.  
  - Configurable associativity, block sizes, and replacement policies (LRU).  
  - Logs cache hits, misses, and store operations for analysis.  

- **Flexible Configuration**  
  - Load programs from machine code files.  
  - Command-line arguments to configure cache size, associativity, and block size.  

## Usage

```bash
./simulator [--cache SIZE,ASSOC,BLOCK[,SIZE,ASSOC,BLOCK]] program.bin
