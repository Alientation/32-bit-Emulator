
; INCLUDE_BINC
;*
	Start of file
*;
.global _start
.section .text
		add	x1, x2, #123			; this code will never run
_start:
		add	x1, x2, #$1234
		sub	x1, x3, #$1234
		adrp	x0, #datalabel		; equivalent to adrp x1, #datalabel
		add	x0, #:lo12:datalabel
		ldr	x0, [x0]
		bl 	func
		hlt
func:
		mul	x3, x1, x2
		ret
;*#macro	testmacro(arg1, arg2)
		add	x1, x2, arg1
		sub	x1, x3, arg2
#macend

#invoke	testmacro(50, 30)*;
;* NOT YET IMPLEMENTED
.section .data
datalabel:
	.byte		$01, $02, $03, $04
	.dbyte		$1234, $5678
	.word		$12345678,
	.dword		$0123456789abcdef,
*;
;*
	End of file
*;
