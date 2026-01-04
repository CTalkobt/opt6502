# 45GS02 (MEGA65) Optimization Guide

This guide details the specialized optimization techniques for the 45GS02 CPU found in the MEGA65, as implemented in `opt6502`. The 45GS02 offers several unique instructions and architectural features that allow for significant performance and code size improvements over the standard 6502/65C02.

## Key Differences & Considerations for 45GS02

- **STZ (Store Z Register)**: CRITICAL DIFFERENCE! On the 45GS02, `STZ` *stores the Z register*, not a zero value. This makes it incredibly useful for repeated stores of a specific byte. The `opt6502` *never* converts `LDA #0, STA addr` to `STZ addr` for 45GS02, as it would be incorrect.
- **Z Register**: A general-purpose 8-bit register, similar to A, X, Y.
- **Q Register**: A 32-bit composite register `[Z:Y:X:A]`. Operations on Q affect all four underlying 8-bit registers.
- **New Instructions**: `LDZ`, `STZ`, `NEG`, `ASR`, `LDQ`, `STQ`, `ADCQ`, `SBCQ`, `CMPQ`, `ASRQ`, `RORQ`, `ROLQ`, `INC16`, `DEC16`, `PHW`, `PLW`, `BRA` (always).
- **Extended NOP**: `NOP #cycles` allows for precise multi-cycle delays.

## 1. Z Register for Repeated Stores

Leverages the `LDZ` and `STZ` instructions to efficiently store the same value to multiple memory locations.

**Pattern**: Repeated sequence of loading an immediate value and storing it.

**Before:**
```asm
  LDA #$20
  STA $0400
  LDA #$20
  STA $0401
  LDA #$20
  STA $0402
```

**After:**
```asm
  LDZ #$20      ; Load Z register once
  STZ $0400     ; Store from Z
  STZ $0401     ; Store from Z
  STZ $0402     ; Store from Z
```
**Benefit**: Saves 2 bytes and 2 cycles per additional store (`LDA`/`STA` is 4 bytes/6 cycles, `STZ` is 2 bytes/4 cycles). This is a significant optimization for screen fills, memory initialization, etc.

## 2. 32-bit Q Register Operations (`LDQ`, `STQ`, `ADCQ`, `SBCQ`, etc.)

The Q register is a powerful feature for 32-bit operations by treating A, X, Y, and Z as a single 32-bit entity. `opt6502` identifies sequences of 8-bit loads that form a 32-bit constant.

**Pattern**: Four consecutive immediate loads into A, X, Y, Z to form a 32-bit value `[Z:Y:X:A]`.

**Before:**
```asm
  LDA #$AA    ; Low byte
  LDX #$BB    ; Mid-low byte
  LDY #$CC    ; Mid-high byte
  LDZ #$DD    ; High byte
```

**After:**
```asm
  LDQ #$DDCCBBAA ; Load 32-bit value into Q
; OPT: LDX #$BB removed
; OPT: LDY #$CC removed
; OPT: LDZ #$DD removed
```
**Benefit**: Replaces four instructions (12 bytes, ~16 cycles) with one `LDQ` instruction (5 bytes, 8 cycles), offering massive speed and size improvements for 32-bit constant loading.

## 3. NEG Instruction

The 45GS02 has a dedicated `NEG` (negate accumulator) instruction, which is much more efficient than the traditional 6502 sequence.

**Pattern**: `EOR #$FF, SEC, ADC #$01` (6502 way to negate A).

**Before:**
```asm
  EOR #$FF
  SEC
  ADC #$01
```

**After:**
```asm
  NEG A      ; Negate accumulator
; OPT: SEC removed
; OPT: ADC #$01 removed
```
**Benefit**: Saves 4 bytes and 5 cycles.

## 4. ASR (Arithmetic Shift Right)

The 45GS02 includes an `ASR` instruction, which performs an arithmetic shift right, preserving the sign bit. This is faster and smaller than the typical 6502 sequence.

**Pattern**: `CMP #$80, ROR` (a common 6502 sequence for signed right shift).

**Before:**
```asm
  CMP #$80   ; Check sign
  ROR A      ; Shift right
```

**After:**
```asm
  ASR A      ; Arithmetic Shift Right Accumulator
; OPT: CMP #$80 removed
```
**Benefit**: Saves 2 bytes and 2 cycles.

## 5. Extended NOP

The 45GS02 `NOP` instruction can take an operand to specify a delay in cycles, making it ideal for precise timing loops or replacing multiple `NOP` instructions.

**Pattern**: Multiple consecutive `NOP` instructions.

**Before:**
```asm
  NOP
  NOP
  NOP
  NOP
```

**After:**
```asm
  NOP #8     ; Four NOPs (2 cycles each) replaced with NOP for 8 cycles
; OPT: NOP removed
; OPT: NOP removed
; OPT: NOP removed
```
**Benefit**: Saves bytes by consolidating multiple `NOP`s into a single instruction with a cycle count. (Each NOP is 2 cycles; so NOP #8 replaces 4 NOP instructions).

This guide covers the major 45GS02-specific optimizations within `opt6502`. Utilizing these features effectively can lead to highly performant and compact code on the MEGA65.
