; Optimized for speed
; Assembler: Generic
; Target CPU: 6502
; Total optimizations: 0

:
start:
    LDA #$00	; Load A with 0 - sets Z flag, clears N flag
    STA $1000	; Store A - no flags affected
    LDX #$FF	; Load X with $FF - clears Z flag, sets N flag
    TAX	; Transfer A to X - sets Z flag (A is 0)
    INX	; Increment X - affects N and Z flags
    CLC	; Clear carry flag
    ADC #$05	; Add with carry - affects all flags (C, N, Z, V)
    STA $2000	; Store result
:
loop:
    LDY #$10	; Load Y with $10
    DEY	; Decrement Y - affects N and Z flags
    BNE loop	; Branch if not zero - no flags affected
:
    CMP #$05	; Compare A with 5 - affects C, N, Z flags
    BEQ equal	; Branch if equal - no flags affected
:
equal:
    SEC	; Set carry flag
    SBC #$01	; Subtract with carry - affects C, N, Z, V flags
    RTS	; Return from subroutine
