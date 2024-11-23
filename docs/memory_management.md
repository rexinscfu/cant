# Memory Management System

## Overview
The CANT memory management system provides a robust and efficient memory allocation infrastructure designed specifically for automotive diagnostic and therapeutic applications. It features both heap and pool allocators, memory protection, and comprehensive debugging capabilities.

## Features
- Configurable heap memory management
- Fixed-size memory pools for efficient small allocations
- Memory guards for buffer overflow detection
- Memory leak tracking and reporting
- Memory statistics and diagnostics
- Memory defragmentation
- Thread-safe operations
- Aligned memory allocation support

## Usage

### Initialization 
```c
MemConfig config = {
    .heap_size = 1024 * 1024, // 1MB heap
    .pool_sizes = [32, 64, 128], // Pools for 32, 64, and 128 byte allocations
    .pool_counts = [10, 5, 3], // 10, 5, and 3 blocks per pool
    .enable_guards = true, // Enable memory guards
    .enable_tracking = true, // Enable memory tracking
    .enable_stats = true // Enable memory statistics
};
Memory_Init(config);
```

### Memory Allocation 
```c
// Basic allocation
void ptr = MEMORY_ALLOC(size);
// Aligned allocation
void aligned_ptr = MEMORY_ALLOC_ALIGNED(size, 16);
// Zero-initialized allocation
void zeroed_ptr = MEMORY_CALLOC(count, size);
// Resize allocation
ptr = MEMORY_REALLOC(ptr, new_size);
// Free memory
MEMORY_FREE(ptr);
``` 

### Memory Diagnostics
```c
// Get memory statistics
MemStats stats;
Memory_GetStats(&stats);
// Check memory integrity
if (!Memory_CheckIntegrity()) {
// Handle corruption
}
// Defragment memory
Memory_Defragment();
// Track memory leaks
Memory_TrackLeaks(callback);
```
