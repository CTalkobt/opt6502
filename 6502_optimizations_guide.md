# 6502 Optimization Guide

A practical guide to 6502 assembly optimization techniques implemented in `opt6502`.

This guide provides examples and explanations for each optimization category. The examples use a generic assembly syntax.

## 1. Peephole Optimizations

Peephole optimization involves examining a small "window" of instructions and replacing them with a shorter or faster sequence.

### Redundant Load/Store

**Pattern**: Storing a value to memory and immediately loading it back into the same register.

**Before:**
```asm
  STA my_var
  LDA my_var
```

**After:**
```asm
  STA my_var
; OPT: LDA my_var removed
```
**Benefit**: Saves 3-4 cycles and 2-3 bytes. The value is already in the accumulator.

### Useless Transfers

**Pattern**: Transferring a value between registers back and forth.

**Before:**
```asm
  TAX   ; A -> X
  TXA   ; X -> A
```

**After:**
```asm
; OPT: TAX removed
; OPT: TXA removed
```
**Benefit**: Saves 4 cycles and 2 bytes.

### No-Operation Instructions

**Pattern**: Instructions that have no effect on registers or flags in a specific context.

**Before:**
```asm
  ORA #$00   ; OR with zero changes nothing
  AND #$FF   ; AND with all ones changes nothing
```

**After:**
```asm
; OPT: ORA #$00 removed
; OPT: AND #$FF removed
```
**Benefit**: Saves 2 cycles and 2 bytes per removed instruction.

## 2. Dead Code Elimination

This removes code that is unreachable and can never be executed.

**Pattern**: Code that appears immediately after an unconditional jump or return instruction.

**Before:**
```asm
  JMP end_of_routine
  LDA #$01      ; This line can never be reached
  STA $D020

end_of_routine:
  RTS
```

**After:**
```asm
  JMP end_of_routine
; OPT: LDA #$01 removed
; OPT: STA $D020 removed

end_of_routine:
  RTS
```
**Benefit**: Saves bytes and prevents logical errors.

## 3. Jump & Branch Optimization

### Jump to Next Instruction

**Pattern**: A `JMP` instruction that jumps to the very next line of code.

**Before:**
```asm
  JMP continue
continue:
  LDA #$00
```
**After:**
```asm
; OPT: JMP continue removed
continue:
  LDA #$00
```
**Benefit**: Saves 3 cycles and 3 bytes.

### Tail Call Optimization

**Pattern**: A subroutine call (`JSR`) immediately followed by a return (`RTS`). The `JSR`/`RTS` can be replaced by a single `JMP`.

**Before:**
```asm
  JSR do_something
  RTS
```
**After:**
```asm
  JMP do_something
; OPT: RTS removed
```
**Benefit**: Saves 12 cycles (6 for `JSR`, 6 for `RTS`) and 1 byte. Also reduces stack usage.

## 4. Load/Store Optimization

### Redundant Loads

**Pattern**: Loading a value into a register when that value is already present.

**Before:**
```asm
  LDA #$0A
  STA some_var
  LDA #$0A     ; Redundant, A already contains $0A
  STA other_var
```
**After:**
```asm
  LDA #$0A
  STA some_var
; OPT: LDA #$0A removed
  STA other_var
```
**Benefit**: Saves 2 cycles and 2 bytes.

## 5. Constant Propagation & Folding

### Constant Propagation

The optimizer tracks the immediate values held in registers.

**Before:**
```asm
  LDA #10
  STA value
  ...
  LDA #10      ; A is known to be 10 here
  STA value2
```
**After:**
```asm
  LDA #10
  STA value
  ...
; OPT: LDA #10 removed
  STA value2
```
**Benefit**: Saves 2 cycles and 2 bytes.

### Constant Folding

The optimizer evaluates constant expressions at compile time.

**Before:**
```asm
  LDA #$10
  ORA #$20
```
**After:**
```asm
  LDA #$30
; OPT: ORA #$20 removed and folded
```
**Benefit**: Saves 2 cycles and 2 bytes.

## 6. Subroutine Inlining

If a subroutine is only called once, the optimizer can replace the `JSR` with the body of the subroutine.

**Before:**
```asm
init:
  JSR clear_memory
  RTS

clear_memory:
  LDX #$00
loop:
  STA $0400,X
  INX
  BNE loop
  RTS
```
**After:**
```asm
init:
  ; JSR clear_memory (inlined below)
  LDX #$00
loop:
  STA $0400,X
  INX
  BNE loop
  RTS

; OPT: clear_memory routine removed after inlining
```
**Benefit**: Saves 12 cycles from the `JSR`/`RTS` overhead, but increases code size. Best for `speed` optimization mode.

## 7. Strength Reduction

This technique replaces computationally "expensive" operations with cheaper ones.

**Pattern**: Multiplication by 2.

**Before:**
```asm
  CLC
  ADC my_var   ; Assuming A holds my_var, this is A = A * 2
```
**After:**
```asm
  ASL A        ; Arithmetic shift left is faster
```
**Benefit**: `ASL A` is 2 cycles, `CLC`+`ADC` is 4-5 cycles.

## 8. Flag Usage Optimization

### Redundant Flag Instructions

**Pattern**: Setting or clearing a flag that is already in the desired state.

**Before:**
```asm
  CLC
  LDA #$01
  CLC          ; Redundant, carry is already clear
  ADC #$02
```
**After:**
```asm
  CLC
  LDA #$01
; OPT: CLC removed
  ADC #$02
```
**Benefit**: Saves 2 cycles and 1 byte.

This guide covers the core optimizations for the standard 6502 processor. For specifics on 65C02 or 45GS02, please see the `README.md` and the dedicated guides.
