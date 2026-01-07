# CPU-Specific Correctness Tests

## Purpose
These tests verify that CPU-specific optimizations are applied correctly and only when appropriate for the target CPU.

## Critical Test Cases

### 65C02 Tests
- **STZ stores literal zero**: Verify `LDA #0; STA addr` → `STZ addr` actually stores zero
- **BRA usage**: Verify BRA optimization for short jumps

### 45GS02 Tests (MEGA65)
- **STZ stores Z register**: Verify STZ stores Z register, NOT literal zero
- **Z register tracking**: Verify LDZ/STZ sequence correctness
- **NEG instruction**: Verify negation optimization
- **ASR instruction**: Verify arithmetic shift right

### 6502 Tests
- **No 65C02 instructions**: Verify optimizer doesn't use STZ, BRA on base 6502
- **Classic instruction set only**

## Running Tests

Each CPU has its own subdirectory with test cases:

```bash
# Run all correctness tests
./run_correctness_tests.sh

# Run specific CPU tests
cd 65c02
../../run_tests.sh  # Uses existing test infrastructure
```

## Test Structure

Each test case should:
1. Target a specific CPU feature
2. Have clear pass/fail criteria
3. Be executable via emulator (semantic tests)
4. Document expected behavior in comments
