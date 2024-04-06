
	lib r0 abshi@init-lock
	sib r0 abslo@init-lock

	core r1
	sz r1
	bia abs@aux-cores

	; main core
	; just waste some cycles to emulate setting things up
	nop nop nop nop nop
	nop nop nop nop nop
	nop nop nop nop nop

	; unlock other cores
	ncores r1
	wad r0 r1

	; spin
	bia .

@aux-cores: ; core# is in r1
@spin-on-init-lock:
	rad r3 r0
	snz r3
	bia abs@spin-on-init-lock

	lib r4 97
	add r5 r4 r4 r1
	portw r4 151

	; TODO: atomically decrement init-lock

	; spin
	bia .

>0100 @init-lock:
