#define _CRT_SECURE_NO_WARNINGS

#include <Windows.h>
#include <stdint.h>
#include <iostream>
#include <string>


struct Interrupt;

void readAndWrite(HANDLE, uint8_t*, uint8_t*, size_t);
void write(HANDLE, uint8_t*, size_t);
void read(HANDLE, uint8_t*, size_t);
void writeBit(HANDLE, uint8_t);
void readBit(HANDLE, uint8_t&, int);
void setSCK(HANDLE, int);
void loadProgram(HANDLE, uint8_t*, Interrupt*, uint8_t, size_t, size_t);
void readMemory(HANDLE, uint8_t*, uint8_t*, size_t);
int issueCmd(HANDLE, int, uint8_t, uint8_t);
uint8_t getLowByte(uint16_t);
uint8_t getHighByte(uint16_t);

void uWait(int);
void error(std::string);
void parseHex(const char* path, Interrupt** interrupts, uint8_t** program, size_t* programCount);

#define SCK_DELAY /*10000*/ 20 /*5*/ /*2*/

#define T_WD_FLASH  5  //~2.6
#define T_WD_EEPROM 5  //~3.6
#define T_WD_ERASE  12 //~10.5
#define T_WD_FUSE   6  //~4.5

#define assert(condition) if(!condition) *(int*)0 = 0
#define IVT_END 0x32
#define PAGESIZE 64 //In words, (instruction) word = 16 bits
#define INTERRUPT_COUNT 26

//64 words / page
//16K words of flash  = 32Kbytes
//256 pages


//Data output  : TTxD
//Data input   : CTS 
//SCK          : RTS
//VCC		   : DTR

static DWORD perfF;

#pragma pack(1)
uint8_t programmingEnable[4] = {		
	0xAC,
	0x53,
	0x00,
	0x00
};
#pragma pack(pop)

#pragma pack(1)
uint8_t readFuseBits[4] = {
	0x50,
	0x00,
	0x00,
	0x00,
};
#pragma pack(pop)

#pragma pack(1)
uint8_t readFuseHighBits[4] = {
	0x58,
	0x08,
	0x00,
	0x00,
};
#pragma pack(pop)

#pragma pack(1)
uint8_t loadProgramMemoryPageHighByte[4] = {
	0x48,
	0x00,
	0x00,
	0x00
};
#pragma pack(pop)

#pragma pack(1)
uint8_t loadProgramMemoryPageLowByte[4] = {
	0x40,
	0x00,
	0x00,
	0x00
};
#pragma pack(pop)

#pragma pack(1)
uint8_t writeProgramMemoryPage[4] = {
	0x4C,
	0x00,
	0x00,
	0x00
};
#pragma pack(pop)

#pragma pack(1)
uint8_t readProgramMemoryHighByte[4] = {
	0x28,
	0x00,
	0x00,
	0x00
};
#pragma pack(pop)

#pragma pack(1)
uint8_t readProgramMemoryLowByte[4] = {
	0x20,
	0x00,
	0x00,
	0x00
};
#pragma pack(pop)

#pragma pack(1)
uint8_t pollRDY[3] = {
	0xF0,
	0x00,
	0x00
};
#pragma pack(pop)

#pragma pack(1)
uint8_t chipErase[4] = {
	0xAC,
	0x80,
	0x00,
	0x00
};
#pragma pack(pop)

#pragma pack(1)
uint8_t writeFuseLowByte[4] = {
	0xAC,
	0xA0,
	0x00,
	0x62		//Default fuse
};
#pragma pack(pop)

#pragma pack(1)
uint8_t writeFuseHighByte[4] = {
	0xAC,
	0xA8,
	0x00,
	0xD9		//Default fuse
};
#pragma pack(pop)

#define WRITE_OP 0
#define READ_OP  1
#define OTHER_OP 2

typedef struct SPICommand {
	uint8_t op[4];
	int type;
	int paramsI[2];
	float delay;
};


enum {
	PROGRAMMING_ENABLE = 0,
	R_FUSE_LBITS,
	R_FUSE_HBITS,
	W_FUSE_LBYTE,
	W_FUSE_HBYTE,
	L_FLASH_LBYTE,
	L_FLASH_HBYTE,
	R_FLASH_LBYTE,
	R_FLASH_HBYTE,
	W_FLASH_PAGE,
	POLL_RDY,
	CHIP_ERASE,
};

static SPICommand commands[12];


typedef struct Interrupt {
	uint8_t addr = 0xFF;
	uint8_t op[4];						//Each interrupt vector hosts two instruction words(jmp has 32-bit opcode)
};


int main() {
		
	std::ios_base::sync_with_stdio(false);
	

	commands[PROGRAMMING_ENABLE] = { { 0xAC, 0x53, 0x00, 0x00 }, OTHER_OP, {}, 0.0f };
	commands[POLL_RDY]           = { { 0xF0, 0x00, 0x00, 0x00 }, READ_OP,  {}, 0.0f };
	commands[R_FUSE_LBITS]       = { { 0x50, 0x00, 0x00, 0x00 }, READ_OP,  {}, 0.0f };
	commands[R_FUSE_HBITS]       = { { 0x58, 0x08, 0x00, 0x00 }, READ_OP,  {}, 0.0f };
	commands[R_FLASH_LBYTE]      = { { 0x20, 0x00, 0x00, 0x00 }, READ_OP,  {1, 2}, 0.0f };
	commands[R_FLASH_HBYTE]      = { { 0x28, 0x00, 0x00, 0x00 }, READ_OP,  {1, 2}, 0.0f };
	commands[CHIP_ERASE]         = { { 0xAC, 0x80, 0x00, 0x00 }, WRITE_OP, {}, T_WD_ERASE };
	commands[W_FUSE_LBYTE]       = { { 0xAC, 0xA0, 0x00, 0x62 }, WRITE_OP, {3, 3}, T_WD_FUSE };
	commands[W_FUSE_HBYTE]       = { { 0xAC, 0xA8, 0x00, 0xD9 }, WRITE_OP, {3, 3}, T_WD_FUSE };
	commands[L_FLASH_LBYTE]      = { { 0x40, 0x00, 0x00, 0x00 }, WRITE_OP, {2, 3}, 0.0f };
	commands[L_FLASH_HBYTE]      = { { 0x48, 0x00, 0x00, 0x00 }, WRITE_OP, {2, 3}, 0.0f };
	commands[W_FLASH_PAGE]       = { { 0x4C, 0x00, 0x00, 0x00 }, WRITE_OP, {1, 2}, T_WD_FLASH };


   // Interrupt* interrupts;
   // uint8_t* programHex;

   // size_t programCount;

  //  parseHex(/*"E:\\MyPrograms\\AVR\\BlinkyBlinkyLED\\BlinkyBlinkyLED\\Debug\\BlinkyBlinkyLED.hex"*/"E:\\MyPrograms\\AVR\\POV_screen\\POV_screen\\Debug\\POV_screen.hex", &interrupts, &programHex, &programCount);


	uint8_t receivedBuffer[4]    = {};
	//uint8_t receivedFuses[4]     = {};
	//uint8_t receivedHighFuses[4] = {};


	HANDLE hSerial = NULL;
	DCB sParams = { 0 };
	COMMTIMEOUTS timeouts = { 0 };

	hSerial = CreateFile("COM1", GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

	if (hSerial != INVALID_HANDLE_VALUE) {
		sParams.DCBlength = sizeof(sParams);

		if (GetCommState(hSerial, &sParams) != 0) {

			sParams.BaudRate    = CBR_9600;
			sParams.ByteSize    = 8;
			sParams.StopBits    = ONESTOPBIT;
			sParams.Parity      = NOPARITY;
			sParams.fDtrControl = DTR_CONTROL_ENABLE;  //Vcc
			sParams.fRtsControl = RTS_CONTROL_DISABLE; //SCK must be logical 0 at start

			if (SetCommState(hSerial, &sParams) != 0) {

				timeouts.ReadIntervalTimeout		 = 50;
				timeouts.ReadTotalTimeoutConstant	 = 50;
				timeouts.ReadTotalTimeoutMultiplier  = 10;
				timeouts.WriteTotalTimeoutConstant   = 50;
				timeouts.WriteTotalTimeoutMultiplier = 10;

				if (SetCommTimeouts(hSerial, &timeouts) != 0) {
					
					LARGE_INTEGER temp;
					QueryPerformanceFrequency(&temp);
					perfF = temp.QuadPart;

					Sleep(50);	//At least 20ms					

                    int r = issueCmd(hSerial, PROGRAMMING_ENABLE, 0, 0);
					//readAndWrite(hSerial, programmingEnable, receivedBuffer, 4);		
					//readAndWrite(hSerial, readFuseBits, receivedFuses, 4);
					//readAndWrite(hSerial, readFuseHighBits, receivedHighFuses, 4);


                    
                    uint8_t* currentHex   = 0;
                    size_t programCount   = 0;
                    Interrupt* interrupts = 0;      
                    char currentFile[100];

                    printf("\n\n  s <file path>\n  l\n  f <0xfuses(t)> \n  q\n\n\n");

                    if (r == commands[PROGRAMMING_ENABLE].op[1]) {
                        printf("\tMCU connected");
                    }
                    else {
                        printf("\tNo MCU detected");
                    }

                    printf("\n>");

                    //setvbuf(stdin, 0, _IONBF, 0);

                    char cmd = 0;
                    char params[100];
                    char lineBuf[100];

                    for(;;) {

                        fgets(lineBuf, 100, stdin);
                        sscanf(lineBuf, "%c %s", &cmd, params);

                        if (cmd == 'q') {
                            break;
                        }


                        switch (cmd) {

                            case 'c':  //Connect chip
                            {
                                int reply = issueCmd(hSerial, PROGRAMMING_ENABLE, 0, 0);

                                if (r == commands[PROGRAMMING_ENABLE].op[1]) {
                                    printf("\tMCU connected\n");
                                }
                                else {
                                    printf("\tNo MCU detected\n");
                                }

                            } break;

                            case 's': //Select and load file          s <file path>
                            {
                              strcpy(currentFile, params);
                              parseHex(currentFile, &interrupts, &currentHex, &programCount);
                            } break;

                            case 'l': //Load file to chip(flash)    
                            {
                              printf("\tConfirm flash file y/n: %s\n>", currentFile);

                              if (getchar() == 'y') {

                                  loadProgram(hSerial, currentHex, interrupts, 0x00, programCount, INTERRUPT_COUNT); //Change start address to be modified / to be read from the file
                                  printf("\tDone(no corruption check)\n");
                              }
                              else {
                                  printf("\tCancelled\n");
                              }

                            } break;

                            case 'f': //Fuse configuration           f <0x0000>
                            {
                              uint8_t currentLowFuses = issueCmd(hSerial, R_FUSE_LBITS, 0, 0);
                              uint8_t currentHighFuses  = issueCmd(hSerial, R_FUSE_HBITS, 0, 0);

                              printf("\tCurrent fuses: 0x%04x - Low fuses: %2x  High fuses: %2x\n", (currentHighFuses << 8) | currentLowFuses, currentLowFuses, currentHighFuses);
                              
                              char* ptr;
                              uint16_t fuses = strtol(params, &ptr, 16);
                              uint8_t lB = getLowByte(fuses);
                              uint8_t hB = getHighByte(fuses);



                              if (*ptr == 't') {
                                  printf("\tToggle mode\n");
                                  lB = currentLowFuses  ^ lB;
                                  hB = currentHighFuses ^ hB;
                              }
                            
                              printf("\tConfirm new LOW FUSES y/n: %02x\n>", lB);

                              fflush(stdin);
                              if (getchar() == 'y') {
                                  issueCmd(hSerial, W_FUSE_LBYTE, lB, 0);
                                  printf("\tDone(no corruption check)\n");
                              }
                              else {
                                 printf("\tCancelled\n");
                              }

                              printf("\tConfirm new HIGH FUSES y/n: %02x\n>", hB);

                              fflush(stdin);
                              if (getchar() == 'y') {
                                  issueCmd(hSerial, W_FUSE_HBYTE, hB, 0);
                                  printf("\tDone(no corruption check)\n");
                              }
                              else {
                                  printf("\tCancelled\n");
                              }

                            } break;
                              

                            default: 
                            {
                              printf("Invalid command\n");
                            } break;
                        }

                        printf(">");
                        fflush(stdin);
                    }

                    free(currentHex);
                    free(interrupts);

                    //return 0;

//                  write(hSerial, chipErase, 4);
//					Sleep(20);
                    //loadProgram(hSerial, programHex, interrupts, 0x00, 72, 26);
//                    loadProgram(hSerial, programHex, interrupts, 0x00, programCount, INTERRUPT_COUNT);

					//uint8_t contents[6] = {66, 66, 66, 66, 66, 66};
					//uint8_t lowB[4]     = {66, 66, 66, 66};
					//uint8_t highB[4]    = {66, 66, 66, 66};
					
					//uint16_t rAddr      = 0x003F;

					//readProgramMemoryLowByte[1]  = getHighByte(rAddr);
					//readProgramMemoryLowByte[2]  = getLowByte(rAddr); 

					//readProgramMemoryHighByte[1] = getHighByte(rAddr);
					//readProgramMemoryHighByte[2] = getLowByte(rAddr);

					
					//Sleep(3);
					//readAndWrite(hSerial, readProgramMemoryLowByte,  lowB,  4);
					//Sleep(3);
					//readAndWrite(hSerial, readProgramMemoryHighByte, highB, 4);
					
                    //int lVal = issueCmd(hSerial, R_FLASH_LBYTE, getHighByte(rAddr), getLowByte(rAddr));
                    //int hVal = issueCmd(hSerial, R_FLASH_HBYTE, getHighByte(rAddr), getLowByte(rAddr));
                    
                    /*
                    uint8_t progMem[300];

                    for (int i = 0x00; i < 0x80; i++) {
                        int lVal = issueCmd(hSerial, R_FLASH_LBYTE, getHighByte(i), getLowByte(i));
                        int hVal = issueCmd(hSerial, R_FLASH_HBYTE, getHighByte(i), getLowByte(i));

                        progMem[i*2] = lVal;
                        progMem[i*2 + 1] = hVal;
                    }
                    */
					

					int i = 0;
					
				}
				else {
					error("Failed to set com timeouts.");
				}

			}
			else {
				error("Failed to set com state.");
			}
		}
		else {
			error("Couldn't aquire device state.");
		}
	} 
	else {
		error("Invalid handle");
	}
	

	CloseHandle(hSerial);
	getchar();

	return 0;
}

uint8_t getHighByte(uint16_t word) {
	return (uint8_t)( word >> 8 );
}

uint8_t getLowByte(uint16_t word) {
	return  (uint8_t)( word & 0xFF );
}


static __inline void getWallClock(DWORD* dest) {
	LARGE_INTEGER c;
	QueryPerformanceCounter(&c);
	*dest = c.QuadPart;
}

void uWait(int uSec) {
	DWORD t0;
	DWORD t1;

	// (((float)(t1.QuadPart - t0.QuadPart)) / (float)perfF *  1000000.0f) < uSec
	// Can be arranged to:
	// (((float)(t1.QuadPart - t0.QuadPart))) < (uSec * (float)perfF / 1000000.0f)

	DWORD relUSec = (DWORD)((uSec * perfF) / 1000000.0f);		

	getWallClock(&t0);

	do {
		getWallClock(&t1);
	} while ((DWORD)(t1 - t0) < relUSec);		
}

void readMemory(HANDLE hndl, uint8_t* buffer, uint8_t* addr, size_t size) {

	for (int i = 0; i < size / 2; i++) {

		readProgramMemoryLowByte[1] = addr[0];
		readProgramMemoryLowByte[2] = addr[1];
		write(hndl, readProgramMemoryLowByte, 3);
		read(hndl, buffer + i, 1);

		readProgramMemoryHighByte[1] = addr[0];
		readProgramMemoryHighByte[2] = addr[1];
		write(hndl, readProgramMemoryHighByte, 3);
		read(hndl, buffer + 1 + i, 1);

		addr[1] += 1;
	}
}

void inline waitRDY(HANDLE hndl) {
	uint8_t r;

	do  {
		write(hndl, pollRDY, 3);
		read(hndl, &r, 1);
	} while (r);

}



void loadWord(HANDLE hndl, uint8_t loByte, uint8_t hiByte, uint8_t addr) {
    /*
	loadProgramMemoryPageLowByte[2] = addr;
	loadProgramMemoryPageLowByte[3] = loByte;							
	write(hndl, loadProgramMemoryPageLowByte, 4);

	loadProgramMemoryPageHighByte[2] = addr;
	loadProgramMemoryPageHighByte[3] = hiByte;
	write(hndl, loadProgramMemoryPageHighByte, 4);
    */

    issueCmd(hndl, L_FLASH_LBYTE, addr, loByte);
    issueCmd(hndl, L_FLASH_HBYTE, addr, hiByte);

}


int issueCmd(HANDLE hndl, int cmdI, uint8_t p1, uint8_t p2) {

	SPICommand* curCmd = &commands[cmdI];
	uint8_t* curOp = curCmd->op;

	uint8_t rVal = 0;


    int i1 = curCmd->paramsI[0];
    int i2 = curCmd->paramsI[1];

    if (i1) {
        curOp[i1] = p1;
    }

    if (i2 && (i2 != i1)) {
        curOp[i2] = p2;
    }


	switch (curCmd->type) {

		case WRITE_OP:
        {
			//curOp[curCmd->paramsI[0]] = p1;
			//curOp[curCmd->paramsI[1]] = p2;

			write(hndl, curOp, 4);

		} break;

		case READ_OP: 
        {	
			write(hndl, curOp, 3);
			read(hndl, &rVal, 1);

		} break;

		case OTHER_OP:
        {
			write(hndl, curOp, 2);
            read(hndl, &rVal, 1);
            write(hndl, curOp+2, 1);
		} break;

	}

	 
	Sleep(curCmd->delay);

	return rVal;
}

void loadProgram(HANDLE hndl, uint8_t* hex, Interrupt* iTable, uint8_t addr, size_t size, size_t sizeInterrupts) {				
    /*
    loadWord(hndl, 0x55, 0x55, 0x00);
    loadWord(hndl, 0x55, 0x55, 0x01);

    //loadProgramMemoryPageLowByte[3]  = 0x55;
    //loadProgramMemoryPageHighByte[3] = 0x55;


    //write(hndl, loadProgramMemoryPageLowByte, 4);
    //write(hndl, loadProgramMemoryPageHighByte, 4);


    write(hndl, writeProgramMemoryPage, 4);
    Sleep(20);
    
    */
            issueCmd(hndl, CHIP_ERASE, 0, 0);           //!!!
    
    
	assert((size % 2 == 0));
	assert((addr < PAGESIZE));

	int pageNumber     = 0;
	uint16_t pageAddr  = 0;
	
	if (iTable) {
		for (int i = 0; i < sizeInterrupts; i++) {
			Interrupt& curI = iTable[i];

            if (curI.addr < IVT_END) {
			    loadWord(hndl, curI.op[0], curI.op[1], curI.addr);
			    loadWord(hndl, curI.op[2], curI.op[3], curI.addr + 1);
            }
		}
	}
	
    issueCmd(hndl, W_FLASH_PAGE, getHighByte(pageAddr), getLowByte(pageAddr));

	pageNumber++;
	pageAddr = pageNumber * PAGESIZE;
    
	for (int i = 0; i < (size / 2); i++) {

		if (addr < PAGESIZE) {		
			uint8_t lowByte  = *hex++;
			uint8_t highByte = *hex++;

			loadWord(hndl, lowByte, highByte, addr);
		
			addr++; 
		}
		else {	
            issueCmd(hndl, W_FLASH_PAGE, getHighByte(pageAddr), getLowByte(pageAddr));

			addr = 0x00;
			pageNumber++;
			pageAddr = pageNumber*PAGESIZE;

			i--;
			

		}
			
	}
	
	
    if (addr != 0x00) {
        issueCmd(hndl, W_FLASH_PAGE, getHighByte(pageAddr), getLowByte(pageAddr));
    }
    
    
    
	
}

void writeBit(HANDLE hndl, uint8_t bit) {
	EscapeCommFunction(hndl, bit ? SETBREAK : CLRBREAK);
}

void readBit(HANDLE hndl, uint8_t& recByte, int at) {
	DWORD status;
	GetCommModemStatus(hndl, &status);
	
	uint8_t bit = (bool)(status & MS_CTS_ON);
	recByte |= (bit << at);
}

void writeByte() {

}

void readByte() {

}


void readAndWrite(HANDLE hndl, uint8_t* bytes, uint8_t* recBuffer, size_t count) {		//Clocked on rising AND falling edge
	for (int i = 0; i < count; i++) {
		uint8_t curByteW = bytes[i];
		uint8_t& curByteR = recBuffer[i] = 0;

		for (int j = 7; j >= 0; j--) {

			uint8_t bitW = curByteW & (1 << j);

			setSCK(hndl, 0);

			readBit(hndl, curByteR, j);

			writeBit(hndl, bitW);
			uWait(SCK_DELAY);

			setSCK(hndl, 1);			
			uWait(SCK_DELAY);

		}
	}

}

void write(HANDLE hndl, uint8_t* bytes, size_t count) {		//Clocked on rising edge
	for (int i = 0; i < count; i++) {
		uint8_t curByte = bytes[i];

		for (int j = 7; j >= 0; j--) {
			uint8_t bit = curByte & (1 << j);

			setSCK(hndl, 0);
			
			writeBit(hndl, bit);
			uWait(SCK_DELAY);
			
			setSCK(hndl, 1);
            uWait(SCK_DELAY); 

			setSCK(hndl, 0);						
		}
	}

}

void read(HANDLE hndl, uint8_t* buffer, size_t count) {		//Clocked on falling edge
	for (int i = 0; i < count; i++) {
		uint8_t& curByte = buffer[i] = 0;

		for (int j = 7; j >= 0; j--) {

			//setSCK(hndl, 1);
			//uWait(SCK_DELAY);

			setSCK(hndl, 0);

			readBit(hndl, curByte, j);

			uWait(SCK_DELAY); 
			
            setSCK(hndl, 1);
            uWait(SCK_DELAY);

			
		}

	}
}

void setSCK(HANDLE hndl, int state) {
	EscapeCommFunction(hndl, state ? SETRTS : CLRRTS);
}

void error(std::string e) {
	DWORD err = GetLastError();
	std::cout << "Error: " << err << std::endl;
	std::cout << e << std::endl;
}


#pragma pack(1)
typedef struct LineHeader {
	uint8_t sc;
	uint8_t  dataLength;
	uint16_t address;
	uint8_t  type;
};
#pragma pop(pack)

#define HEADER_SIZE sizeof(LineHeader)
#define AUXILARY_SIZE (HEADER_SIZE + 2)		//Header + checksum(2 bytes)
#define LINE_ID ':'
#define RECORD_DATA 0
#define RECORD_EOF 1

void parseHex(const char* path, Interrupt** interrupts, uint8_t** program, size_t* programCount) {
	uint8_t* tempBuf = 0;

	FILE* f = fopen(path, "rb");

	if (f) {

		fseek(f, 0L, SEEK_END);


		int length = ftell(f);
		rewind(f);

		tempBuf = (uint8_t*)malloc(length);

		if (tempBuf) {

			//fread(tempBuf, length, 1, f);

			for (int i = 0; i < length; i++) {
				int val1 = 0;
				int val2 = 0;

				if (fscanf(f, "%1x", &val1)) {
					int r = fscanf(f, "%1x", &val2);
                    tempBuf[i] = (val1 << 4) | val2;
				}
				else {
					fscanf(f, "%1c", &tempBuf[i]);
				}

			}
			

			fclose(f);

			//parse buffer

            *interrupts = (Interrupt*)malloc(sizeof(Interrupt)*INTERRUPT_COUNT);
            memset(*interrupts, 0xFF, sizeof(Interrupt)*INTERRUPT_COUNT);

            uint8_t* tempProgBuffer = (uint8_t*)malloc(length);  //An array that is sure to hold all program instructions
            int programByteCount = 0;

			LineHeader header = {};

			int programI = 0;

			for (int i = 0; i < length; i += (AUXILARY_SIZE + header.dataLength - 1)) { 

				memcpy(&header, (tempBuf + i), HEADER_SIZE);

                uint16_t addrHi = header.address & 0x00FF;//*(uint8_t*)&header.address;            //Intel hex usually big endian
                uint16_t addrLo = header.address >> 8;

                uint16_t corrected = (addrHi << 8) | addrLo;        

				//header.address    >>= 8;
                header.address = corrected;

				if (header.sc == LINE_ID) {

					if (header.type == RECORD_DATA) {

						if (header.address < IVT_END) {
							//Add interrupt
							int iNum = header.address >> 1;

							(*interrupts)[iNum].addr = header.address;

							(*interrupts)[iNum].op[0] = tempBuf[i + HEADER_SIZE];
							(*interrupts)[iNum].op[1] = tempBuf[i + HEADER_SIZE + 1];
							(*interrupts)[iNum].op[2] = tempBuf[i + HEADER_SIZE + 2];
							(*interrupts)[iNum].op[3] = tempBuf[i + HEADER_SIZE + 3];
						}
						else {
							for (int j = 0; j < header.dataLength; j += 2) {		//Word(Instruction size) = 2 bytes
								/*program*/tempProgBuffer[programI++] = tempBuf[i + HEADER_SIZE + j];
								/*program*/tempProgBuffer[programI++] = tempBuf[i + HEADER_SIZE + j + 1];
							}

                            programByteCount += header.dataLength;
						}
					}
					else if (header.type == RECORD_EOF) {
						break;
					}

				}

			}

            *program = (uint8_t*)malloc(programByteCount);
            memcpy(*program, tempProgBuffer, programByteCount);

            *programCount = programByteCount;

            free(tempProgBuffer);
		}
	}


	free(tempBuf);
}