#!/usr/bin/env python3

import subprocess
import sys
import re
from pathlib import Path

def parse_perf_results(output):
    metrics = {}
    
    # Extract key metrics
    patterns = {
        'msg_rate': r'Message Success Rate: (\d+)%',
        'process_time': r'Avg Process Time: (\d+)us',
        'mem_usage': r'Memory Usage: (\d+)%',
        'recovery_rate': r'Recovery Rate: (\d+)%',
        'cache_gain': r'Cache Performance Gain: (\d+)%'
    }
    
    for key, pattern in patterns.items():
        match = re.search(pattern, output)
        if match:
            metrics[key] = int(match.group(1))
    
    return metrics

def check_performance(metrics):
    # Performance thresholds
    thresholds = {
        'msg_rate': 95,        # Minimum 95% success rate
        'process_time': 1000,  # Maximum 1ms processing time
        'mem_usage': 80,       # Maximum 80% memory usage
        'recovery_rate': 90,   # Minimum 90% recovery rate
        'cache_gain': 20       # Minimum 20% cache performance gain
    }
    
    failed = []
    for key, threshold in thresholds.items():
        if key in metrics:
            if key in ['msg_rate', 'recovery_rate', 'cache_gain']:
                if metrics[key] < threshold:
                    failed.append(f"{key}: {metrics[key]}% < {threshold}%")
            else:
                if metrics[key] > threshold:
                    failed.append(f"{key}: {metrics[key]} > {threshold}")
    
    return failed

def main():
    build_dir = Path("build_test")
    
    # Run tests and capture output
    try:
        output = subprocess.check_output(
            ["./system_integration_test"], 
            cwd=build_dir,
            stderr=subprocess.STDOUT,
            universal_newlines=True
        )
    except subprocess.CalledProcessError as e:
        print(f"Error running tests: {e}")
        return 1
    
    # Parse and check results
    metrics = parse_perf_results(output)
    failed_checks = check_performance(metrics)
    
    if failed_checks:
        print("Performance checks failed:")
        for fail in failed_checks:
            print(f"  - {fail}")
        return 1
    else:
        print("All performance checks passed!")
        print("\nMetrics:")
        for key, value in metrics.items():
            print(f"  {key}: {value}")
        return 0

if __name__ == "__main__":
    sys.exit(main()) 