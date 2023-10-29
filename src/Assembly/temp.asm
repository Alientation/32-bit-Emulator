;*
	Start of file
*;

.org		$1000
loadlabel:
		LDA	#$FFFF
		LDA	#$1234
.data
.outfile	"test"
datalabel:
.db		$00,$01, $02, $03, $04, $05, $06, $07
.d2b		$1234, $5678,

; End