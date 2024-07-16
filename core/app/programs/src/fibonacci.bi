.global _start
.text
_start:
		add	x0, xzr, #6		; number of iterations
		add	x1, xzr, #1		; first num
		add	x2, xzr, #1		; second num
loop_begin:
		cmp	x0, #1
		b.le	loop_end
		add 	x3, x1, #0			; temp variable
		add 	x1, x2, #0
		add 	x2, x2, x3
		sub	x0, x0, #1
		b	loop_begin
loop_end:
