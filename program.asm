
	lib r0 abshi@init-lock
	sib r0 abslo@init-lock

	core r1
	sz r1
	bia abs@aux-cores

	; main core
	; just waste some cycles to emulate setting things up
	nop nop nop nop
	nop nop nop nop

	; unlock other cores
	ncores r1
	dec r2 r1 1
	wab r0 r1

	; when other cores are done they will decrement init lock
@main-spin:
	rab r1 r0
	sz r1
	bia abs@main-spin

	lib r0 10
	portw r0 151

	; power off
	portw r0 #ff

@aux-cores: ; core# is in r1
@spin-on-init-lock:
	rab r3 r0
	snz r3
	bia abs@spin-on-init-lock

	lib r4 97
	add r5 r4 r4 r1
	portw r4 151

	lib r4 #ff
	fetchadd r4 r0 r4

	; spin
	bia .

>0100 @init-lock:
