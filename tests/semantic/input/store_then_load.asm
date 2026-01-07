; Test: STA followed by LDA of same address
; Optimizer should remove redundant LDA
; Final state must be identical

test_start:
    LDA #$99
    STA $1000
    LDA $1000   ; Redundant - A already has value
    TAY
    RTS
