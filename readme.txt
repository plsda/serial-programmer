
A very basic serial programmer for atmega328(or compatible AVRs) using RS232 serial interface on Windows 10.

Instructions

	c - check for a connected chip
	
	s - select and parse Intel .hex file
	
	l - flash the selected file to the chip
	
	f - enter fuse config
	
	q - quit


Building

	MSVC2017: 
		cl -GR- -O2 -Oi serial_programmer.cpp

