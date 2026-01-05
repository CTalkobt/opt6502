; Optimized for speed
; Assembler: Generic
; Target CPU: 6502
; Total optimizations: 1

:	 
test_loads:	 
LDA #$00
LDX #$80
LDY #$01
:	 
test_transfers:	 
TAY 
TYA 
:	 
test_arithmetic:	 
CLC 
ADC #$05
SEC 
SBC #$02
:	 
test_logical:	 
AND #$0F
ORA #$10
EOR #$FF
:	 
test_shifts:	 
ASL 
LSR 
ROL 
ROR 
:	 
test_comparisons:	 
CMP #$42
CPX #$00
CPY #$FF
:	 
test_inc_dec:	 
INX 
INY 
DEX 
DEY 
:	 
test_stack:	 
PHA 
PLA 
:	 
test_bit:	 
BIT $1000
:	 
RTS 
