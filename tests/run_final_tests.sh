#!/bin/bash

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${GREEN}Starting Final Test Suite${NC}"

# Build directory setup
BUILD_DIR="build_test"
mkdir -p $BUILD_DIR
cd $BUILD_DIR

# Configure with test coverage
cmake .. \
    -DCMAKE_BUILD_TYPE=Debug \
    -DENABLE_COVERAGE=ON \
    -DSTM32_TARGET=1

# Build all tests
cmake --build . -- -j$(nproc)

# Run integration tests
echo -e "\n${YELLOW}Running Integration Tests${NC}"
./system_integration_test
INTEGRATION_RESULT=$?

# Run hardware tests
echo -e "\n${YELLOW}Running Hardware Tests${NC}"
./hardware_test
HARDWARE_RESULT=$?

# Run performance tests
echo -e "\n${YELLOW}Running Performance Tests${NC}"
./test_network_perf
PERF_RESULT=$?
./test_system_perf
SYS_PERF_RESULT=$?

# Generate coverage report
gcovr -r .. --html --html-details -o coverage_report.html

# Check results
FAILED=0
if [ $INTEGRATION_RESULT -ne 0 ]; then
    echo -e "${RED}Integration Tests Failed${NC}"
    FAILED=1
fi
if [ $HARDWARE_RESULT -ne 0 ]; then
    echo -e "${RED}Hardware Tests Failed${NC}"
    FAILED=1
fi
if [ $PERF_RESULT -ne 0 ]; then
    echo -e "${RED}Performance Tests Failed${NC}"
    FAILED=1
fi
if [ $SYS_PERF_RESULT -ne 0 ]; then
    echo -e "${RED}System Performance Tests Failed${NC}"
    FAILED=1
fi

# Print summary
echo -e "\n${GREEN}Test Summary:${NC}"
echo "Integration Tests: $([ $INTEGRATION_RESULT -eq 0 ] && echo 'PASSED' || echo 'FAILED')"
echo "Hardware Tests: $([ $HARDWARE_RESULT -eq 0 ] && echo 'PASSED' || echo 'FAILED')"
echo "Performance Tests: $([ $PERF_RESULT -eq 0 ] && echo 'PASSED' || echo 'FAILED')"
echo "System Performance: $([ $SYS_PERF_RESULT -eq 0 ] && echo 'PASSED' || echo 'FAILED')"

# Check memory leaks
echo -e "\n${YELLOW}Running Memory Leak Check${NC}"
valgrind --leak-check=full ./system_integration_test 2> valgrind_report.txt
if grep -q "definitely lost: 0 bytes" valgrind_report.txt; then
    echo -e "${GREEN}No Memory Leaks Detected${NC}"
else
    echo -e "${RED}Memory Leaks Detected${NC}"
    FAILED=1
fi

# Final status
if [ $FAILED -eq 0 ]; then
    echo -e "\n${GREEN}All Tests Passed Successfully${NC}"
    exit 0
else
    echo -e "\n${RED}Some Tests Failed - Check Reports${NC}"
    exit 1
fi 