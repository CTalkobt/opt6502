; Optimized for speed
; Assembler: Generic
; Target CPU: 6502
; Total optimizations: 0

:	 
start:	 
LDA #$00
STA $1000
LDX #$FF
TAX 
INX 
CLC 
ADC #$05
STA $2000
:	 
loop:	 
LDY #$10
DEY 
BNE loop
:	 
CMP #$05
BEQ equal
:	 
equal:	 
SEC 
SBC #$01
RTS 
