; #ifndef INCLUDE_BINC
; #define INCLUDE_BINC

LDA #$1200

; #endif


.org $FFFF







.equ output 0 : DBYTE
.scope
	.equ a 300 : DBYTE
	.equ b 300 : DBYTE

	.equ _c a * b
	.equ output _c : DBYTE

.scend

;.equ output 0 : DBYTE
;.scope
;	.equ a 300 : DBYTE
;	.equ b 300 : DBYTE
;	.equ _c a*b : DBYTE
;	.equ output _c : DBYTE
;.scope
