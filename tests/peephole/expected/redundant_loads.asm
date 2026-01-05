; Optimized for speed
; Assembler: Generic
; Target CPU: 6502
; Total optimizations: 20

ClearScreen:
    LDA #$00
    STA $D020
    STA $D021
    LDA #$00	; Redundant
    TAX
@loop:
    STA $0400,X
    STA $0500,X
    INX
    BNE @loop
    RTS
