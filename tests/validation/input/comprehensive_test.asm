; Comprehensive test of register and flag tracking

; Test all register loads
test_loads:
    LDA #$00    ; A=0, Z=1, N=0
    LDX #$80    ; X=$80, Z=0, N=1
    LDY #$01    ; Y=$01, Z=0, N=0

; Test transfers
test_transfers:
    TAX         ; X=A, Z=1, N=0
    TXA         ; A=X, Z=1, N=0
    TAY         ; Y=A, Z=1, N=0
    TYA         ; A=Y, Z=1, N=0

; Test arithmetic
test_arithmetic:
    CLC         ; C=0
    ADC #$05    ; A=A+5+C, affects C,N,Z,V
    SEC         ; C=1
    SBC #$02    ; A=A-2-!C, affects C,N,Z,V

; Test logical
test_logical:
    AND #$0F    ; A=A&$0F, affects N,Z
    ORA #$10    ; A=A|$10, affects N,Z
    EOR #$FF    ; A=A^$FF, affects N,Z

; Test shifts
test_shifts:
    ASL         ; A=A<<1, affects C,N,Z
    LSR         ; A=A>>1, affects C,N,Z (N=0)
    ROL         ; A=(A<<1)|C, affects C,N,Z
    ROR         ; A=(A>>1)|(C<<7), affects C,N,Z

; Test comparisons
test_comparisons:
    CMP #$42    ; Compare A with $42, affects C,N,Z
    CPX #$00    ; Compare X with $00, affects C,N,Z
    CPY #$FF    ; Compare Y with $FF, affects C,N,Z

; Test inc/dec
test_inc_dec:
    INX         ; X++, affects N,Z
    INY         ; Y++, affects N,Z
    DEX         ; X--, affects N,Z
    DEY         ; Y--, affects N,Z

; Test stack
test_stack:
    PHA         ; Push A
    PLA         ; Pull A, affects N,Z

; Test bit
test_bit:
    BIT $1000   ; Test bits, N=bit7, V=bit6, Z=A&M

    RTS
