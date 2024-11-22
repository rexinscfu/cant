# CAN't Programming Language Specification (v0.0.1)

## Overview
CAN't is a domain-specific programming language designed for automotive ECU and signal simulation, with a focus on real-time performance, safety, and hardware integration.

## Basic Syntax

### ECU Definition
An ECU (Electronic Control Unit) is defined using the `ecu` keyword. Example:

    ecu MainController {
        // ECU configuration
        frequency: 200MHz;
        memory: 512KB;
        
        // Signal definitions
        signal EngineSpeed: CAN {
            id: 0x100;
            length: 16;
            unit: rpm;
            range: 0..8000;
        }
    }

### Signal Processing
Signal processing blocks are defined using the `process` keyword. Example:

    process EngineSpeedFilter {
        input: EngineSpeed;
        filter: MovingAverage(window: 10ms);
        output: FilteredEngineSpeed;
    }

## Core Features (Planned)

### Real-time Features
- Deterministic execution
- Priority-based scheduling
- Deadline monitoring
- Worst-case execution time analysis

### Automotive Protocols
- CAN bus (2.0A/B)
- CAN FD
- FlexRay
- Automotive Ethernet
- LIN

### Safety Features
- ISO 26262 compliance tools
- MISRA-C compliance checking
- Runtime bounds checking
- Memory safety guarantees

### Signal Processing
- Real-time filters (FIR, IIR)
- FFT analysis
- Kalman filtering
- Sensor fusion algorithms

### Development Tools
- Built-in profiler
- Memory analysis
- Timing analysis
- Protocol debugging

## Type System

### Basic Types
- `u8`, `u16`, `u32`, `u64`: Unsigned integers
- `i8`, `i16`, `i32`, `i64`: Signed integers
- `f32`, `f64`: Floating point numbers
- `bool`: Boolean values
- `time`: Time values (ns, µs, ms, s)
- `freq`: Frequency values (Hz, kHz, MHz)

### Signal Types
- `CAN`: CAN bus signals
- `FlexRay`: FlexRay signals
- `LIN`: LIN bus signals
- `Analog`: Analog signals
- `Digital`: Digital signals

## Memory Model
- Static allocation by default
- No dynamic allocation in real-time contexts
- Stack size verification at compile time
- Memory pool allocators for deterministic behavior

## Error Handling
- Compile-time checks for timing constraints
- Runtime assertions for safety-critical code
- Error propagation through the call chain
- Fault containment regions

## Build System Integration
- AUTOSAR build system compatibility
- Vector tools integration
- Hardware-in-the-loop support
- Continuous integration ready
