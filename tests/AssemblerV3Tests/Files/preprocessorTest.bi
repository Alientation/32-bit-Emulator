; This is a comment

;*{}[](),:;

#include #macro #macret #macend #invoke #define #undef #ifdef #ifndef #else #elsedef #elsendef #endif
.equ .org
#
%010111 @0137 0123456789 $0123456789aAbBcCdDeEfF
' ' "bced"
abcdef
+ - * / % || && << >> ^ & | ~ != ! == <= >= < >
*;



; #ifndef INCLUDE_BINC
; #define INCLUDE_BINC

LDA #$1200

; #endif"include.binc"
;*
.org $FFFF

#macro add (a: DBYTE, b: DBYTE) : DBYTE
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

#12334567890
*;