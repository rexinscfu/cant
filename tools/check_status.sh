#!/bin/bash

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${GREEN}Running Final Status Check${NC}"

# Check build status
echo -e "\n${YELLOW}Checking Build Status${NC}"
if [ -d "build" ] && [ -f "build/system_integration_test" ]; then
    echo "✅ Build successful"
else
    echo -e "${RED}❌ Build failed or incomplete${NC}"
    exit 1
fi

# Check test results
echo -e "\n${YELLOW}Checking Test Results${NC}"
if [ -f "build/test_results.xml" ]; then
    FAILED_TESTS=$(grep -c "failure" build/test_results.xml)
    if [ $FAILED_TESTS -eq 0 ]; then
        echo "✅ All tests passed"
    else
        echo -e "${RED}❌ $FAILED_TESTS tests failed${NC}"
        exit 1
    fi
else
    echo -e "${RED}❌ Test results not found${NC}"
    exit 1
fi

# Check performance metrics
echo -e "\n${YELLOW}Checking Performance Metrics${NC}"
python3 tools/verify_performance.py
if [ $? -eq 0 ]; then
    echo "✅ Performance requirements met"
else
    echo -e "${RED}❌ Performance requirements not met${NC}"
    exit 1
fi

# Check documentation
echo -e "\n${YELLOW}Checking Documentation${NC}"
if [ -f "docs/TEST_RESULTS.md" ]; then
    echo "✅ Documentation updated"
else
    echo -e "${RED}❌ Documentation missing${NC}"
    exit 1
fi

echo -e "\n${GREEN}Status Check Complete - Ready for Commit${NC}" 