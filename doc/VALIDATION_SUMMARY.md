# Register and Flag Tracking Validation - Implementation Summary

## Overview
This document describes the comprehensive register and flag tracking validation system implemented for the opt6502 optimizer. This system tracks all register usage (A, X, Y, Z) and processor flag states (C, N, Z, V) for every 6502/65C02/45GS02 instruction.

## Implementation Details

### 1. Enhanced RegisterState Structure
The `RegisterState` structure (opt6502.c:51-78) now tracks:

#### Registers:
- **A (Accumulator)**: Value, known state, zero state, modified flag
- **X (Index X)**: Value, known state, zero state, modified flag
- **Y (Index Y)**: Value, known state, zero state, modified flag
- **Z (Z Register, 45GS02)**: Value, known state, zero state, modified flag

#### Processor Flags:
- **C (Carry)**: Known state, set/clear value
- **N (Negative)**: Known state, set/clear value
- **Z (Zero)**: Known state, set/clear value
- **V (Overflow)**: Known state, set/clear value

### 2. Comprehensive Instruction Tracking
The `update_register_state()` function (opt6502.c:465-954) provides complete tracking for:

#### Load Instructions (LDA, LDX, LDY, LDZ)
- Updates register value tracking
- Sets N flag based on bit 7 of loaded value
- Sets Z flag if value is zero
- Does not affect C or V flags

#### Store Instructions (STA, STX, STY, STZ)
- No register or flag modifications
- Memory-only operations

#### Transfer Instructions (TAX, TXA, TAY, TYA, TSX, TXS)
- Copies register values and states
- Sets N and Z flags based on transferred value
- Does not affect C or V flags

#### Arithmetic Instructions (ADC, SBC)
- Modifies accumulator
- Affects all flags: C, N, Z, V
- Carry and overflow depend on operand values

#### Logical Instructions (AND, ORA, EOR)
- Modifies accumulator
- Sets N and Z flags
- Does not affect C or V flags

#### Shift/Rotate Instructions (ASL, LSR, ROL, ROR)
- Modifies accumulator or memory
- Sets C flag from shifted bit
- Sets N and Z flags from result
- LSR always clears N flag

#### Increment/Decrement (INX, INY, DEX, DEY, INC, DEC)
- Modifies register or memory
- Sets N and Z flags
- Does not affect C or V flags

#### Comparison Instructions (CMP, CPX, CPY)
- Does not modify registers
- Sets C flag if register >= operand
- Sets Z flag if register == operand
- Sets N flag from bit 7 of result

#### Flag Manipulation (CLC, SEC, CLV, CLI, SEI, CLD, SED)
- CLC: Clears carry flag
- SEC: Sets carry flag
- CLV: Clears overflow flag
- Other flags (I, D) not tracked in current implementation

#### Stack Operations (PHA, PHP, PLA, PLP)
- PLA: Pulls accumulator, sets N and Z flags
- PLP: Restores all flags from stack
- PHA, PHP: No register or flag changes

#### Branch and Jump Instructions
- No register or flag modifications
- JSR: Conservatively invalidates all registers and flags (subroutine may modify them)
- RTI: Restores flags from stack (treated as unknown)

#### 45GS02-Specific Instructions
- **NEG**: Negates accumulator, affects N, Z, C flags
- **ASR**: Arithmetic shift right, preserves sign bit, affects N, Z, C flags
- **LDZ**: Loads Z register, affects N and Z flags
- **STZ**: On 45GS02, stores Z register (NOT zero!)

### 3. Validation Function
The `validate_register_and_flag_tracking()` function (opt6502.c:997-1175) provides:

#### Comprehensive Analysis:
- Walks through entire AST
- Updates register/flag state for each instruction
- Tracks total register modifications
- Tracks total flag modifications
- Resets state at branch targets (control flow convergence)

#### Detailed Reporting:
- Total instructions analyzed
- Register modifications detected
- Flag modifications detected
- Summary of which registers are used (A, X, Y, Z)
- Summary of which flags are affected (C, N, Z, V)

#### Verbose Mode:
- With `-trace 2`, prints register and flag state after each instruction
- Shows exact values when known
- Shows which flags are set/clear
- Useful for debugging and optimization analysis

### 4. Integration
The validation function is automatically called after optimization completes:
- Located in `optimize_program_ast()` (opt6502.c:1586)
- Runs after all optimization passes complete
- Provides immediate feedback on register/flag usage

## Usage Examples

### Basic Validation:
```bash
./opt6502 -speed test.asm output.asm
```

Output includes:
```
=== Register and Flag Tracking Validation ===
=== Validation Summary ===
Total instructions analyzed: 23
Register modifications detected: 8
Flag modifications detected: 14

=== Register Usage Summary ===
Registers used:
  A (Accumulator): YES
  X (Index X):     YES
  Y (Index Y):     YES
  Z (Z register):  NO (45GS02 only)

Flags affected:
  C (Carry):       YES
  N (Negative):    YES
  Z (Zero):        YES
  V (Overflow):    YES
```

### Verbose Validation (Per-Instruction):
```bash
./opt6502 -speed -trace 2 test.asm output.asm
```

Shows detailed state after each instruction:
```
Line 3: LDA #$00
  Register state at line 3:
    A: known=yes, zero=yes, value=#$00, modified=yes
    X: known=no, zero=no, value=unknown, modified=no
    Y: known=no, zero=no, value=unknown, modified=no
    Z: known=no, zero=no, value=unknown, modified=no
    Flags:
      C (Carry):    known=no, set=unknown
      N (Negative): known=yes, set=no
      Z (Zero):     known=yes, set=yes
      V (Overflow): known=no, set=unknown
```

### 45GS02 Validation:
```bash
./opt6502 -speed -cpu 45gs02 test_45gs02.asm output.asm
```

Properly tracks Z register usage:
```
Registers used:
  A (Accumulator): YES
  X (Index X):     YES
  Y (Index Y):     NO
  Z (Z register):  YES
```

## Validation Coverage

### Instruction Categories Covered:
- ✅ Load instructions (LDA, LDX, LDY, LDZ)
- ✅ Store instructions (STA, STX, STY, STZ)
- ✅ Transfer instructions (TAX, TXA, TAY, TYA, TSX, TXS)
- ✅ Arithmetic (ADC, SBC)
- ✅ Logical operations (AND, ORA, EOR)
- ✅ Shift and rotate (ASL, LSR, ROL, ROR)
- ✅ Increment/decrement (INX, INY, DEX, DEY, INC, DEC)
- ✅ Comparison (CMP, CPX, CPY)
- ✅ Flag manipulation (CLC, SEC, CLV, CLI, SEI, CLD, SED)
- ✅ Stack operations (PHA, PHP, PLA, PLP)
- ✅ Branches and jumps (BCC, BCS, BEQ, BNE, BMI, BPL, BVC, BVS, BRA, JMP, JSR, RTS, RTI)
- ✅ Bit test (BIT)
- ✅ 45GS02 specific (NEG, ASR, LDZ)
- ✅ No operation (NOP)

### Flag Tracking Accuracy:
Each instruction correctly tracks which flags it affects:

| Instruction | C | N | Z | V | Notes |
|-------------|---|---|---|---|-------|
| LDA/LDX/LDY | - | ✓ | ✓ | - | Load sets N and Z |
| ADC/SBC | ✓ | ✓ | ✓ | ✓ | Arithmetic affects all flags |
| AND/ORA/EOR | - | ✓ | ✓ | - | Logical sets N and Z only |
| CMP/CPX/CPY | ✓ | ✓ | ✓ | - | Compare sets C, N, Z |
| ASL/LSR/ROL/ROR | ✓ | ✓ | ✓ | - | Shifts set C, N, Z |
| INC/DEC/INX/DEX/INY/DEY | - | ✓ | ✓ | - | Inc/Dec sets N and Z |
| CLC/SEC | ✓ | - | - | - | Flag manipulation |
| CLV | - | - | - | ✓ | Clear overflow |
| BIT | - | ✓ | ✓ | ✓ | Special: N=bit7, V=bit6, Z=A&M |
| PLA | - | ✓ | ✓ | - | Pull sets N and Z |

## Future Enhancement Opportunities

### Potential Optimizations Enabled by This Tracking:

1. **Dead Flag Write Elimination**: If a flag is set but never read before being set again, the first set can be eliminated.

2. **Redundant Compare Elimination**: If we know register values, we can eliminate redundant comparisons.

3. **Flag-Based Constant Propagation**: Track flag states across basic blocks for better constant propagation.

4. **Branch Prediction**: Use flag knowledge to predict branch outcomes and enable more aggressive optimization.

5. **Register Allocation**: Use register usage information to make better decisions about which registers to use.

6. **Zero Detection Optimization**: When we know a register is zero, we can use STZ (65C02) or optimize comparisons.

7. **Carry Flag Optimization**: Track carry flag state to eliminate redundant CLC/SEC instructions.

## Testing

### Test Files Included:
1. **test_validation.asm**: General 6502 instruction testing
2. **test_45gs02.asm**: 45GS02-specific Z register testing

### Verification:
- All instructions compile without errors
- Validation runs automatically after optimization
- Per-instruction tracking available with `-trace 2`
- Correctly identifies register and flag usage
- Proper handling of branch targets (control flow convergence)

## Conclusion

The register and flag tracking validation system is now fully implemented and operational. It provides:

1. ✅ Complete tracking of A, X, Y, Z register usage
2. ✅ Complete tracking of C, N, Z, V flag states
3. ✅ Per-instruction state updates
4. ✅ Comprehensive validation reporting
5. ✅ Support for 6502, 65C02, 65816, and 45GS02 CPUs
6. ✅ Integration with existing optimization infrastructure
7. ✅ Verbose tracing mode for detailed analysis

This foundation will enable future optimization enhancements that leverage register and flag state information for more intelligent code optimization decisions.
