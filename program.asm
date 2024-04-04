	inc r0 r0 1
	callir rel@foo
	inc r1 r1 1
	bia .

@foo:
	dec r0 r0 1
	callia abs@bar
	dec r2 r2 1
	ret

@bar:
	inc r2 r2 1
	ret
