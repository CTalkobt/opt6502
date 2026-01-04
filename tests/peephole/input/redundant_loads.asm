ClearScreen:
    LDA #$00
    STA $D020
    LDA #$00        ; Redundant
    STA $D021
    LDA #$00        ; Redundant
    TAX
@loop:
    STA $0400,X
    STA $0500,X
    INX
    BNE @loop
    RTS
