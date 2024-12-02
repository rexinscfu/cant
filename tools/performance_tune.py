#!/usr/bin/env python3

import subprocess
import json
import sys
from pathlib import Path

def run_benchmark(config):
    # Write temporary config
    with open('temp_config.json', 'w') as f:
        json.dump(config, f)
    
    # Run benchmark
    try:
        result = subprocess.run(
            ['./system_integration_test', '--config', 'temp_config.json'],
            capture_output=True,
            text=True
        )
        return parse_results(result.stdout)
    except Exception as e:
        print(f"Benchmark failed: {e}")
        return None

def optimize_parameters():
    # Base configuration
    config = {
        'buffer_size': 512,
        'pool_size': 32,
        'cache_size': 1024,
        'dma_threshold': 64
    }
    
    # Test different configurations
    best_config = config.copy()
    best_performance = run_benchmark(config)
    
    # Buffer size optimization
    for size in [256, 512, 1024, 2048]:
        test_config = config.copy()
        test_config['buffer_size'] = size
        perf = run_benchmark(test_config)
        if perf and perf['msg_rate'] > best_performance['msg_rate']:
            best_config = test_config
            best_performance = perf
    
    # Pool size optimization
    for size in [16, 32, 64, 128]:
        test_config = best_config.copy()
        test_config['pool_size'] = size
        perf = run_benchmark(test_config)
        if perf and perf['msg_rate'] > best_performance['msg_rate']:
            best_config = test_config
            best_performance = perf
    
    return best_config, best_performance

def main():
    print("Starting performance optimization...")
    
    best_config, best_performance = optimize_parameters()
    
    print("\nOptimal Configuration:")
    print(json.dumps(best_config, indent=2))
    
    print("\nPerformance Metrics:")
    print(f"Message Rate: {best_performance['msg_rate']} msg/s")
    print(f"Process Time: {best_performance['process_time']}us")
    print(f"Memory Usage: {best_performance['mem_usage']}%")
    
    # Save optimal configuration
    with open('optimal_config.json', 'w') as f:
        json.dump(best_config, f, indent=2)
    
    print("\nConfiguration saved to optimal_config.json")

if __name__ == "__main__":
    main() 