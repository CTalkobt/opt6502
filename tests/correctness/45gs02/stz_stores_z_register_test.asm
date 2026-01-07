; 45GS02 Correctness Test: STZ stores Z register (NOT zero!)
; This is the critical difference between 65C02 and 45GS02
; On 45GS02, STZ stores the Z register value, not literal zero

test_45gs02_stz:
    ; Set Z register to non-zero value
    LDZ #$42

    ; Store Z register to memory (STZ on 45GS02)
    STZ $1000

    ; Change Z register
    LDZ #$99
    STZ $1001

    ; Verify memory contains Z register values, not zeros
    LDA $1000   ; Should be 0x42 (first Z value)
    TAX
    LDA $1001   ; Should be 0x99 (second Z value)
    TAY

    RTS

; Expected final state:
; A = 0x99
; X = 0x42
; Y = 0x99
; Z = 0x99
; Memory[0x1000] = 0x42
; Memory[0x1001] = 0x99
