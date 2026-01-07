# Semantic Equivalence Tests

## Purpose
These tests verify that optimized code produces **identical behavior** to the original code by executing both versions and comparing final CPU state.

## Structure

- `input/` - Original assembly test cases
- `output/` - Generated optimized assembly and binary files
- `state/` - Initial and expected state files for each test

## State Files

### Initial State (optional): `state/testname_init.txt`
Specifies the starting CPU state before execution.

```ini
[registers]
A=0x00
X=0xFF
Y=0x00
Z=0x00

[flags]
C=0
N=0
Z=1
V=0

[memory]
0x1000=0x00
0x1001=0xFF
```

### Expected State: `state/testname_expect.txt`
Specifies the expected final CPU state after execution.

```ini
[registers]
A=0x42
X=0x00
Y=0xFF

[flags]
C=1
N=0
Z=0
V=0

[memory]
0x1000=0x42
```

## Running Tests

```bash
# Install py65 (one-time setup)
pip3 install py65

# Run semantic tests
cd tests/semantic
python3 run_semantic_tests.py
```

## Test Case Requirements

Each test case should:
1. Be a complete, executable 6502 program
2. Start at a defined entry point (loaded at 0x1000)
3. End with RTS instruction
4. Have deterministic behavior (no timing-dependent code)
5. Complete within reasonable cycle count (< 10000 cycles)

## Example Test Case

**input/redundant_load.asm:**
```asm
test_start:
    LDA #$42
    LDA #$42    ; Redundant - should be optimized away
    TAX
    RTS
```

**state/redundant_load_expect.txt:**
```ini
[registers]
A=0x42
X=0x42
```

The test runner will:
1. Run original: Verify A=0x42, X=0x42
2. Run optimized: Verify A=0x42, X=0x42
3. Compare: Both produce identical state → PASS
