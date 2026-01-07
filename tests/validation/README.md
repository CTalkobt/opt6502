# Register and Flag Tracking Validation Tests

This directory contains tests for validating the register (A, X, Y, Z) and processor flag (C, N, Z, V) tracking system.

## Test Files

### input/

#### test_validation.asm
General 6502 instruction test covering:
- Load instructions (LDA, LDX, LDY)
- Store instructions (STA)
- Transfer instructions (TAX)
- Increment/Decrement (INX, DEY)
- Arithmetic (ADC, SBC)
- Comparisons (CMP)
- Flag manipulation (CLC, SEC)
- Branches (BNE, BEQ)

**Purpose**: Validate basic register and flag tracking for standard 6502 instructions.

#### test_45gs02.asm
45GS02 (MEGA65) specific test covering:
- LDZ (Load Z register)
- STZ (Store Z register - stores Z, not zero!)
- NEG (Negate accumulator)
- ASR (Arithmetic shift right)

**Purpose**: Validate Z register tracking and 45GS02-specific instruction handling.

#### comprehensive_test.asm
Comprehensive test covering all instruction categories:
- All load instructions
- All transfer instructions
- All arithmetic instructions (ADC, SBC)
- All logical instructions (AND, ORA, EOR)
- All shift/rotate instructions (ASL, LSR, ROL, ROR)
- All comparison instructions (CMP, CPX, CPY)
- All increment/decrement instructions
- Stack operations (PHA, PLA)
- Bit test (BIT)

**Purpose**: Complete validation of all 6502 instruction categories.

### output/

Contains the optimizer output files from running the validation tests. These demonstrate:
- Optimized assembly code
- Validation summary statistics
- Register and flag usage reports

## Running the Tests

### Basic Validation
```bash
./opt6502 -speed tests/validation/input/test_validation.asm tests/validation/output/test_validation_out.asm
```

### 45GS02 Validation
```bash
./opt6502 -speed -cpu 45gs02 tests/validation/input/test_45gs02.asm tests/validation/output/test_45gs02_out.asm
```

### Comprehensive Validation
```bash
./opt6502 -speed tests/validation/input/comprehensive_test.asm tests/validation/output/comprehensive_test_out.asm
```

### Verbose Per-Instruction Tracking
```bash
./opt6502 -speed -trace 2 tests/validation/input/test_validation.asm tests/validation/output/test_validation_trace.asm
```

This will show detailed register and flag state after each instruction.

## Expected Output

Each test produces a validation report showing:

```
=== Register and Flag Tracking Validation ===

=== Validation Summary ===
Total instructions analyzed: N
Register modifications detected: N
Flag modifications detected: N

=== Register Usage Summary ===
Registers used:
  A (Accumulator): YES/NO
  X (Index X):     YES/NO
  Y (Index Y):     YES/NO
  Z (Z register):  YES/NO (45GS02 only)

Flags affected:
  C (Carry):       YES/NO
  N (Negative):    YES/NO
  Z (Zero):        YES/NO
  V (Overflow):    YES/NO

=== Validation Complete ===
```

## Validation Features Tested

1. **Register Value Tracking**
   - Immediate value detection (LDA #$00)
   - Value propagation through transfers
   - Unknown values from memory loads

2. **Flag State Tracking**
   - Known flag states (CLC → C=0, SEC → C=1)
   - Flag modifications per instruction
   - Conservative handling at branch targets

3. **CPU-Specific Features**
   - Standard 6502: A, X, Y registers
   - 45GS02: Z register tracking
   - Correct instruction behavior per CPU type

4. **Instruction Accuracy**
   - Each instruction tracks correct flags
   - LSR always clears N flag
   - Load instructions set N and Z
   - Arithmetic affects all flags (C, N, Z, V)
   - Logical affects N and Z only

## Integration

These tests validate the foundation for future optimizations:
- Dead flag write elimination
- Redundant compare elimination
- Flag-based constant propagation
- Zero detection optimization
- Intelligent register allocation

## Related Documentation

See [VALIDATION_SUMMARY.md](../../doc/VALIDATION_SUMMARY.md) for complete implementation details and instruction coverage tables.
