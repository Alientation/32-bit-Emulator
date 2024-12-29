
; This was included, Should only happen once
; SHOWCASE_PREPROCESSOR_H
; #include "../include/showcase_preprocessor.binc"
.global _start
.text
_start:
	
add x0, xzr, #$12
	add x0, x0, #4
hlt
