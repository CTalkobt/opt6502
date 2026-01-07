; Test: Flag states must be preserved across optimization
; This tests arithmetic and flag manipulation

test_start:
    LDA #$10
    CLC
    ADC #$05    ; A = 0x15, C=0, N=0, Z=0
    SEC
    SBC #$10    ; A = 0x05, C=1
    TAX
    RTS
