; Test file for 45GS02 Z register tracking

start:
    LDZ #$42        ; Load Z register with $42
    STZ $2000       ; Store Z register to memory
    LDA #$10        ; Load A with $10
    TAX             ; Transfer A to X
    NEG             ; Negate A (45GS02 instruction)
    ASR             ; Arithmetic shift right (45GS02 instruction)
    RTS
