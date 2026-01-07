; Test: Redundant load elimination
; Original has redundant LDA, optimizer should remove it
; Both versions must produce identical final state

test_start:
    LDA #$42
    LDA #$42    ; Redundant - should be optimized away
    TAX
    RTS
