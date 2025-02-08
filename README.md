CANT Programming Language

A specialized programming language and runtime system for automotive diagnostics and therapeutic operations.

Features
- Strong static typing with automotive-specific primitives
- First-class CAN bus operations and diagnostic services
- Real-time execution capabilities
- Built-in security and session management
- Hardware-optimized pattern matching

Requirements
- CMake 3.12+
- C11 compliant compiler
- Python 3.8+
- LLVM 15.0+
- Flex and Bison

Build Instructions
```bash
mkdir build && cd build
cmake ..
make
```

Run Tests
```bash
ctest --output-on-failure
```

Platform Support
- Linux (primary)
- Windows (MinGW/MSVC)
- macOS
- Embedded targets (S32K3, TDA4VM)

Author
Fayssal CHOKRI <fayssal.chokri@gmail.com>

License
MIT License