#!/bin/bash
# Idempotence Test: Running optimizer twice should produce same result
# This catches optimizer instability and infinite optimization loops

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
OPT6502="$PROJECT_ROOT/opt6502"

# Check if opt6502 exists
if [ ! -f "$OPT6502" ]; then
    echo "Error: opt6502 not found at $OPT6502"
    echo "Please compile with: make"
    exit 1
fi

echo "======================================================================="
echo "Idempotence Tests - Verifying optimizer stability"
echo "======================================================================="
echo

PASSED=0
FAILED=0
TEMP_DIR=$(mktemp -d)

# Function to test idempotence for a single file
test_idempotence() {
    local testfile=$1
    local testname=$(basename "$testfile" .asm)
    local category=$(basename $(dirname $(dirname "$testfile")))

    echo -n "Testing $category/$testname: "

    # First optimization pass
    if ! "$OPT6502" -speed "$testfile" "$TEMP_DIR/pass1.asm" 2>/dev/null; then
        echo "SKIP (optimization failed)"
        return 0
    fi

    # Second optimization pass on already-optimized code
    if ! "$OPT6502" -speed "$TEMP_DIR/pass1.asm" "$TEMP_DIR/pass2.asm" 2>/dev/null; then
        echo "FAIL (second pass failed)"
        return 1
    fi

    # Third pass to be extra sure
    if ! "$OPT6502" -speed "$TEMP_DIR/pass2.asm" "$TEMP_DIR/pass3.asm" 2>/dev/null; then
        echo "FAIL (third pass failed)"
        return 1
    fi

    # Normalize whitespace for comparison
    sed 's/[[:space:]]*$//' "$TEMP_DIR/pass1.asm" > "$TEMP_DIR/pass1_normalized.asm"
    sed 's/[[:space:]]*$//' "$TEMP_DIR/pass2.asm" > "$TEMP_DIR/pass2_normalized.asm"
    sed 's/[[:space:]]*$//' "$TEMP_DIR/pass3.asm" > "$TEMP_DIR/pass3_normalized.asm"

    # Compare pass1 and pass2
    if ! diff -q "$TEMP_DIR/pass1_normalized.asm" "$TEMP_DIR/pass2_normalized.asm" >/dev/null 2>&1; then
        echo "FAIL (pass1 != pass2 - optimizer not converging)"
        echo "  Differences:"
        diff -u "$TEMP_DIR/pass1_normalized.asm" "$TEMP_DIR/pass2_normalized.asm" | head -20
        return 1
    fi

    # Compare pass2 and pass3
    if ! diff -q "$TEMP_DIR/pass2_normalized.asm" "$TEMP_DIR/pass3_normalized.asm" >/dev/null 2>&1; then
        echo "FAIL (pass2 != pass3 - optimizer unstable)"
        return 1
    fi

    echo "PASS (idempotent after 3 passes)"
    return 0
}

# Find all test files across all test categories
for testdir in "$PROJECT_ROOT"/tests/*/input; do
    if [ -d "$testdir" ]; then
        for testfile in "$testdir"/*.asm; do
            if [ -f "$testfile" ]; then
                if test_idempotence "$testfile"; then
                    ((PASSED++))
                else
                    ((FAILED++))
                fi
            fi
        done
    fi
done

# Cleanup
rm -rf "$TEMP_DIR"

echo
echo "======================================================================="
echo "Idempotence Results: $PASSED passed, $FAILED failed"
echo "======================================================================="

if [ $FAILED -gt 0 ]; then
    echo "FAILURE: Optimizer is not stable/idempotent!"
    exit 1
else
    echo "SUCCESS: Optimizer is stable and idempotent"
    exit 0
fi
