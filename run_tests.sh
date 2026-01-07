#!/bin/bash
# Regression tests - Golden file comparison
# For comprehensive testing, use: ./run_all_tests.sh
set -e
echo "Running regression tests (golden file comparison)..."
for testdir in tests/*; do
    # Skip if not a directory or if no input/expected subdirectories exist
    [ -d "$testdir" ] || continue
    [ -d "$testdir/input" ] || continue
    [ -d "$testdir/expected" ] || continue

    cpufamily=$(basename "$testdir")
    cpu=""
    case "$cpufamily" in
        6502_opt)
            cpu="-cpu 6502"
            ;;
        65c02_opt)
            cpu="-cpu 65c02"
            ;;
        45gs02_opt)
            cpu="-cpu 45gs02"
            ;;
    esac

    for testfile in "$testdir"/input/*.asm; do
        # Skip if glob didn't match any files
        [ -f "$testfile" ] || continue
        testname=$(basename "$testfile" .asm)
        outputfile="$testdir/output/$testname.asm"
        expectedfile="$testdir/expected/$testname.asm"
        mkdir -p "$testdir/output"

        # Handle CPU-specific tests by filename (for validation tests)
        test_cpu="$cpu"
        if [[ "$testname" == *"45gs02"* ]] && [[ -z "$cpu" ]]; then
            test_cpu="-cpu 45gs02"
        elif [[ "$testname" == *"65c02"* ]] && [[ -z "$cpu" ]]; then
            test_cpu="-cpu 65c02"
        fi

        echo "Testing $testname with cpu flag: $test_cpu"
        ./opt6502 -speed $test_cpu "$testfile" "$outputfile"

        # Compare files, ignoring trailing whitespace but preserving label syntax (colons)
        # Create temporary files with trailing whitespace stripped
        expected_normalized=$(mktemp)
        output_normalized=$(mktemp)
        sed 's/[[:space:]]*$//' "$expectedfile" > "$expected_normalized"
        sed 's/[[:space:]]*$//' "$outputfile" > "$output_normalized"

        if diff -u "$expected_normalized" "$output_normalized"; then
            echo "✓ $testname ($cpufamily) passed"
            rm -f "$expected_normalized" "$output_normalized"
        else
            echo "✗ $testname ($cpufamily) failed"
            rm -f "$expected_normalized" "$output_normalized"
            exit 1
        fi
    done
done
