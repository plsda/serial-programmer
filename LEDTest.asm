.org 0x000B ;0x0016 
	jmp TIMER1_COMPA




.org 0x0040;0x0033 ;0x001A 			        ;End of the interrupt vector table
start:
	ldi r16, 1

	;Pin setup
	out DDRB, r16
	out PORTB, r16
	
	cli;

	;Clock setup
	lds r16, TCCR1B				
	ori r16, ( 1 << CS10 )			//clk/64
	ori r16, ( 1 << CS11 )
	
	;ori r16, (1 << CS12)			//clk/256
	
    ;ori r16, (1 << CS10)			//clk/1024
	;ori r16, (1 << CS12)

	ori r16, ( 1 << WGM12)
	sts TCCR1B, r16

	lds r16, TIMSK1
	ori r16, ( 1 << OCIE1A ) 
	sts TIMSK1, r16

	;With 3.579545 MHz ext. clock and CKDIV8 programmed, ~6990(0x1B4E) cycles / second
	ldi r16, 0x4E
	ldi r17, 0x1B

	sts OCR1AH, r17		;NOTE: High byte must be written first(and vice versa when reading) since accessing low byte triggers 16-bit write operation
	sts OCR1AL, r16

	;Sleep setup
	in r16, SMCR
	ori r16, (1 << SE)
	out SMCR, r16

	sei;

	jmp WAIT;


WAIT:
	sleep;
	jmp WAIT;

TIMER1_COMPA: 
	ldi r16, 1
	in r17, PORTB
	eor r17, r16
	out PORTB, r17

	reti				;Return from interrupt: exits interrupt routine and enables interrupts again






