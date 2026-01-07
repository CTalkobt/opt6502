; 6502 Correctness Test: No 65C02 instructions should be used
; When targeting 6502, optimizer must not use STZ, BRA, etc.

test_no_65c02_opcodes:
    ; This pattern might be optimizable on 65C02, but not on 6502
    LDA #$00
    STA $1000   ; Must remain LDA #0 / STA, NOT optimized to STZ

    LDA #$00
    STA $1001

    ; Load and verify
    LDA $1000
    TAX
    LDA $1001
    TAY

    RTS

; Expected final state:
; A = 0x00
; X = 0x00
; Y = 0x00
; Memory[0x1000] = 0x00
; Memory[0x1001] = 0x00

; IMPORTANT: Optimized output must NOT contain STZ instruction
; when compiled with: opt6502 -cpu 6502
