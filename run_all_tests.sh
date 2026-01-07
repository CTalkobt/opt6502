#!/bin/bash
# Comprehensive test runner for opt6502
# Runs all test categories: regression, semantic, idempotence, etc.
# Usage: ./run_all_tests.sh [test_suite_name]
#   Without arguments: runs all available test suites
#   With argument: runs only the specified test suite

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Parse arguments
SPECIFIC_TEST="$1"

# Show help if requested
if [ "$SPECIFIC_TEST" = "-h" ] || [ "$SPECIFIC_TEST" = "--help" ]; then
    echo "Usage: $0 [test_suite_name]"
    echo
    echo "Available test suites:"
    echo "  regression     - Run regression tests"
    echo "  idempotence    - Run idempotence tests"
    echo "  semantic       - Run semantic equivalence tests"
    echo "  correctness    - Run CPU-specific correctness tests"
    echo "  performance    - Run performance validation tests"
    echo
    echo "Without arguments, all available test suites will be run."
    exit 0
fi

if [ -n "$SPECIFIC_TEST" ]; then
    echo "======================================================================="
    echo "opt6502 Test Suite: $SPECIFIC_TEST"
    echo "======================================================================="
else
    echo "======================================================================="
    echo "opt6502 Comprehensive Test Suite"
    echo "======================================================================="
fi
echo

# Track overall results
TOTAL_PASSED=0
TOTAL_FAILED=0
SUITES_RUN=0
SUITES_FAILED=0

# Function to check if we should run a specific suite
should_run_suite() {
    local suite_key=$1
    [ -z "$SPECIFIC_TEST" ] || [ "$SPECIFIC_TEST" = "$suite_key" ]
}

# Function to run a test suite
run_suite() {
    local suite_key=$1
    local suite_name=$2
    local suite_command=$3

    if ! should_run_suite "$suite_key"; then
        return 0
    fi

    echo
    echo "-----------------------------------------------------------------------"
    echo "Running: $suite_name"
    echo "-----------------------------------------------------------------------"

    SUITES_RUN=$((SUITES_RUN + 1))

    if eval "$suite_command"; then
        echo -e "${GREEN}✓ $suite_name PASSED${NC}"
    else
        echo -e "${RED}✗ $suite_name FAILED${NC}"
        SUITES_FAILED=$((SUITES_FAILED + 1))
        # When running all tests, continue on failure. When running a specific test, respect CONTINUE_ON_FAILURE
        if [ -n "$SPECIFIC_TEST" ] && [ "${CONTINUE_ON_FAILURE:-0}" != "1" ]; then
            echo
            echo "Test suite failed. Set CONTINUE_ON_FAILURE=1 to continue despite failures."
            exit 1
        fi
    fi
}

# Check if opt6502 is built
if [ ! -f "./opt6502" ]; then
    echo "Error: opt6502 not found. Building..."
    make || {
        echo "Build failed!"
        exit 1
    }
fi

# Track if specific test was found (for error checking)
SUITE_FOUND=false

# 1. Run existing regression tests
if should_run_suite "regression"; then
    SUITE_FOUND=true
    run_suite "regression" "Regression Tests" "./run_tests.sh"
fi

# 2. Run idempotence tests
if should_run_suite "idempotence"; then
    if [ -f "tests/idempotence/test_idempotence.sh" ]; then
        SUITE_FOUND=true
        run_suite "idempotence" "Idempotence Tests" "tests/idempotence/test_idempotence.sh"
    elif [ -n "$SPECIFIC_TEST" ]; then
        echo -e "${RED}Error: Idempotence tests not found${NC}"
        exit 1
    else
        echo -e "${YELLOW}⊘ Idempotence tests not found (skipping)${NC}"
    fi
fi

# 3. Run semantic equivalence tests (if py65 available)
if should_run_suite "semantic"; then
    if command -v python3 >/dev/null 2>&1; then
        if python3 -c "import py65" 2>/dev/null; then
            if [ -f "tests/semantic/run_semantic_tests.py" ]; then
                SUITE_FOUND=true
                run_suite "semantic" "Semantic Equivalence Tests" "cd tests/semantic && python3 run_semantic_tests.py"
            elif [ -n "$SPECIFIC_TEST" ]; then
                echo -e "${RED}Error: Semantic tests not found${NC}"
                exit 1
            else
                echo -e "${YELLOW}⊘ Semantic tests not found (skipping)${NC}"
            fi
        else
            if [ -n "$SPECIFIC_TEST" ]; then
                echo -e "${RED}Error: py65 not installed${NC}"
                echo "  Install with: pip3 install py65"
                exit 1
            else
                echo -e "${YELLOW}⊘ py65 not installed (skipping semantic tests)${NC}"
                echo "  Install with: pip3 install py65"
            fi
        fi
    else
        if [ -n "$SPECIFIC_TEST" ]; then
            echo -e "${RED}Error: Python3 not found${NC}"
            exit 1
        else
            echo -e "${YELLOW}⊘ Python3 not found (skipping semantic tests)${NC}"
        fi
    fi
fi

# 4. Run correctness tests (if implemented)
if should_run_suite "correctness"; then
    if [ -f "tests/correctness/run_correctness_tests.sh" ]; then
        SUITE_FOUND=true
        run_suite "correctness" "CPU-Specific Correctness Tests" "tests/correctness/run_correctness_tests.sh"
    elif [ -n "$SPECIFIC_TEST" ]; then
        echo -e "${RED}Error: Correctness tests not found${NC}"
        exit 1
    else
        echo -e "${YELLOW}⊘ Correctness tests not found (skipping)${NC}"
    fi
fi

# 5. Run performance validation tests (if implemented)
if should_run_suite "performance"; then
    if [ -f "tests/performance/validate_performance.py" ]; then
        if command -v python3 >/dev/null 2>&1; then
            SUITE_FOUND=true
            run_suite "performance" "Performance Validation Tests" "cd tests/performance && python3 validate_performance.py"
        elif [ -n "$SPECIFIC_TEST" ]; then
            echo -e "${RED}Error: Python3 not found${NC}"
            exit 1
        fi
    elif [ -n "$SPECIFIC_TEST" ]; then
        echo -e "${RED}Error: Performance tests not found${NC}"
        exit 1
    else
        echo -e "${YELLOW}⊘ Performance tests not found (skipping)${NC}"
    fi
fi

# Check if the requested specific test was found
if [ -n "$SPECIFIC_TEST" ] && [ "$SUITE_FOUND" = false ]; then
    echo -e "${RED}Error: Unknown test suite '$SPECIFIC_TEST'${NC}"
    echo
    echo "Available test suites:"
    echo "  regression, idempotence, semantic, correctness, performance"
    echo
    echo "Use --help for more information"
    exit 1
fi

# Summary
echo
echo "======================================================================="
echo "Test Suite Summary"
echo "======================================================================="

# Check if any tests were actually run
if [ $SUITES_RUN -eq 0 ]; then
    if [ -n "$SPECIFIC_TEST" ]; then
        echo -e "${YELLOW}No test suite ran. Check if '$SPECIFIC_TEST' is available.${NC}"
    else
        echo -e "${YELLOW}No test suites available to run.${NC}"
    fi
    exit 0
fi

echo "Suites run: $SUITES_RUN"
echo "Suites passed: $((SUITES_RUN - SUITES_FAILED))"
echo "Suites failed: $SUITES_FAILED"
echo

if [ $SUITES_FAILED -eq 0 ]; then
    echo -e "${GREEN}=======================================================================${NC}"
    echo -e "${GREEN}ALL TESTS PASSED${NC}"
    echo -e "${GREEN}=======================================================================${NC}"
    exit 0
else
    echo -e "${RED}=======================================================================${NC}"
    echo -e "${RED}SOME TESTS FAILED${NC}"
    echo -e "${RED}=======================================================================${NC}"
    exit 1
fi
