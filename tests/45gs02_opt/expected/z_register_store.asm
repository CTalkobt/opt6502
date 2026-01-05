; Optimized for speed
; Assembler: Generic
; Target CPU: 45GS02
; Total optimizations: 4

FillScreen:
    LDZ #$20
    STZ $0400
    STZ $0401
    STZ $0402
