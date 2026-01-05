; Optimized for speed
; Assembler: Generic
; Target CPU: 6502
; Total optimizations: 1

:
test_loads:
    LDA #$00	; A=0, Z=1, N=0
    LDX #$80	; X=$80, Z=0, N=1
    LDY #$01	; Y=$01, Z=0, N=0
:
test_transfers:
    TAY	; Y=A, Z=1, N=0
    TYA	; A=Y, Z=1, N=0
:
test_arithmetic:
    CLC	; C=0
    ADC #$05	; A=A+5+C, affects C,N,Z,V
    SEC	; C=1
    SBC #$02	; A=A-2-!C, affects C,N,Z,V
:
test_logical:
    AND #$0F	; A=A&$0F, affects N,Z
    ORA #$10	; A=A|$10, affects N,Z
    EOR #$FF	; A=A^$FF, affects N,Z
:
test_shifts:
    ASL	; A=A<<1, affects C,N,Z
    LSR	; A=A>>1, affects C,N,Z (N=0)
    ROL	; A=(A<<1)|C, affects C,N,Z
    ROR	; A=(A>>1)|(C<<7), affects C,N,Z
:
test_comparisons:
    CMP #$42	; Compare A with $42, affects C,N,Z
    CPX #$00	; Compare X with $00, affects C,N,Z
    CPY #$FF	; Compare Y with $FF, affects C,N,Z
:
test_inc_dec:
    INX	; X++, affects N,Z
    INY	; Y++, affects N,Z
    DEX	; X--, affects N,Z
    DEY	; Y--, affects N,Z
:
test_stack:
    PHA	; Push A
    PLA	; Pull A, affects N,Z
:
test_bit:
    BIT $1000	; Test bits, N=bit7, V=bit6, Z=A&M
:
    RTS
