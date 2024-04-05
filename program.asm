; hello world
; writes bytes "Hello World\n" to port 151 (common terminal output)

; r0 pointer
; r1 remaining len
; r2 read byte

	; TODO: have a way of getting high and low bytes of labels
	; lib abshi@hello
	; sib abslo@hello

	lib r0 #01
	sib r0 #00

	lib r1 12

@loop:
	rab r2 r0
	inc r0 r0 1
	dec r1 r1 1
	portw r2 151
	sz r1
	bir rel@loop
	bia .

>0100 @hello:
#48 #65 #6c #6c #6f #20 #57 #6f #72 #6c #64 #0a
