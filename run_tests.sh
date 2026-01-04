#!/bin/bash
set -e
echo "Running regression tests..."
for testdir in tests/*; do
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
        testname=$(basename "$testfile" .asm)
        outputfile="$testdir/output/$testname.asm"
        expectedfile="$testdir/expected/$testname.asm"
        mkdir -p "$testdir/output"
        echo "Testing $testname with cpu flag: $cpu"
        ./opt6502 -speed $cpu "$testfile" "$outputfile"
        if diff -uw "$expectedfile" "$outputfile"; then
            echo "✓ $testname ($cpufamily) passed"
        else
            echo "✗ $testname ($cpufamily) failed"
            exit 1
        fi
    done
done
