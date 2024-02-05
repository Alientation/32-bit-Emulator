;#include "include.binc"
;
;.org $FFFF
3 * 2
; conditional
	.db "M_PI is defined", 0
	
	
	
		.db "M_FOUR is defined and M_TWO is defined", 0
	
;*#macro add (a: DBYTE, b: DBYTE) : DBYTE
	.equ _c a + b
	#macret _c
#macend

#macro multiply (a: DBYTE, b: DBYTE) : DBYTE
	.equ _c a * b
	#macret _c
#macend

#macro math (a: DBYTE, b: DBYTE, c: DBYTE) : DBYTE
	.equ _d ((a * b) + c) / b
	#macret _d
#macend

#invoke multiply(300, 300) output
*;
