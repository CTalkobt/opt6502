; Optimized for speed
; Assembler: Generic
; Target CPU: 65C02
; Total optimizations: 0

; Optimization trace enabled (Level 2)
; Lines marked with ; OPT: show applied optimizations

Clear_RAM:
    LDA #$00
    STA $1000
    STA $1001
