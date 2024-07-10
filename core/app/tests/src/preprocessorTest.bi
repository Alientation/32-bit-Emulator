
; INCLUDE_BINC
;*
	Start of file
*;
.global _start
.extern array
.text
_start:
		add	x2, xzr, #0
.align 8
.scope
	array:
		adrp	x1, #array
		add	x1, x1, #:lo12:array
.scend
		adrp	x0, #datalabel
		add	x0, x0, #:lo12:datalabel
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
.data
datalabel:
	.byte		$01, $02, $03, $04
	.dbyte		$1234, $5678
	.word		$12345678,
	.dword		$0123456789abcdef,
*;
;*
	End of file
*;
