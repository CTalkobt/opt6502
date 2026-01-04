Start:
    LDA #$01
    JMP End
    LDA #$02 ; Dead code
    STA $0200 
End:
    RTS
