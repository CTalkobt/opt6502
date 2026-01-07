; 65C02 Correctness Test: STZ stores literal zero
; Verify that LDA #0; STA is optimized to STZ
; and that STZ actually stores zero value

test_stz_zero:
    ; Pre-fill memory with non-zero
    LDA #$FF
    STA $1000
    STA $1001
    STA $1002

    ; These should be optimized to STZ on 65C02
    LDA #$00
    STA $1000

    LDA #$00
    STA $1001

    ; Verify memory contains zeros
    LDA $1000   ; Should be 0x00
    TAX
    LDA $1001   ; Should be 0x00
    TAY

    RTS

; Expected final state:
; A = 0x00
; X = 0x00
; Y = 0x00
; Memory[0x1000] = 0x00
; Memory[0x1001] = 0x00
