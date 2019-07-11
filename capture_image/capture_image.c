// #include "address_map_arm.h"
// Group 7C
// XI Kun Zou, Daniel Turner, MengLing Shi
// Lab 4 Camera

#include <time.h>								
#include <stdio.h>

#include "hps_soc_system.h"					
#include "hps.h"
#include "socal.h"

#define SDRAM_START		      0xC0000000
#define KEY_BASE              0xFF200050
#define SW_BASE				  0xFF200040		
#define VIDEO_IN_BASE         0xFF203060		
#define FPGA_ONCHIP_BASE      0xC8000000		
#define CHARACTER_BASE        0xC9000000		


/* This program demonstrates the use of the D5M camera with the DE1-SoC Board
 * 	1. Capture one frame of video when any key is pressed.
 * 	2. Display the captured frame when any key is pressed.		  
*/
/* Note: Set the switches SW1 and SW2 to high and rest of the switches to low for correct exposure timing while compiling and the loading the program in the Altera Monitor program.
 * Note: KEY_ptr means address of KEY_ptr, while *KEY_ptr means value stored in KEY_ptr
*/

short picture [320][240];     // used to store a picture
// initialize variables

volatile int * KEY_ptr = (int *) KEY_BASE;
volatile int * SW_ptr = (int *) SW_BASE;
volatile int * Video_In_DMA_ptr	= (int *) VIDEO_IN_BASE;
volatile short * Video_Mem_ptr	= (short *) FPGA_ONCHIP_BASE;
volatile short * Char_ptr = (short *) CHARACTER_BASE;
volatile int * SDRAM_start_ptr = (int *) SDRAM_START;

int allx,ally,thisx,thisy,fifo_status,rready,idata,SDRAM_bits = 0;

int recordingBits=0;
char picture_array[40][240];	// bits array

/* Display a null-terminated text string at coordinates x, y. Assume that the text fits on one line */
void display(int x, int y, char *ptr)
 {
 	int offset;
	  	volatile char * character_buffer = (char *) CHARACTER_BASE;	// VGA character buffer
		offset = (y << 7) + x;
		while ( *(ptr) )
		{
			*(character_buffer + offset) = *(ptr);	// write to the character buffer
			++ptr;
			++offset;
		}
 }
 void clean(){
    // clear all the text display on monitor		
	display(3,56,"                                                             ");
	display(3,58,"                                                             ");
	display(2,2, "                                                             ");
	display(2,4, "                                                             ");
	display(2,6, "                                                             ");
	display(2,8, "                                                             ");
	display(2,10, "                                                            ");
	display(2,12, "                                                            ");
	display(2,14, "                                                            ");
	display(2,16, "                                                            ");
}


int main(void){										// main method

	*(Video_In_DMA_ptr + 3)	= 0x4;					// Enable the video
	while (1){										// while loop
		if(*KEY_ptr !=0){                           // if key is pressed
		switch(*SW_ptr){                            // find cases below
			
			case 1:                               // SW0 On and press a button
			    resume_video();
				clean();
				break;
			case 6:                               // SW1 & SW2 Both on and press to do following
			    clean();                          // clear screen	
				printf("\nDisplaying Black and White image");					
				take_picture();					  // takes a picture
				blackAndWhite();							  // converting to Black and White
				
				printf("\nDisplaying Black screen after went through compression");
				while(*SW_ptr == 6){}             // stays Black and white Until switch 0,1,2 all on(flip SW0)
      			compressionInRle();                   // image compress in the function 
				black_screen();                   // display black screen to note that compression is Done
				
				
				while(*SW_ptr == 7){}             // stays black screen until switch 0,1,2,3 all on (flip SW3)
				printf("\nDisplaying Decompressed black and White image");	  
				decompressFromSDRAM();	              // decompress function displaying the image on screen
				            
				break;
			
			default:		
				break;	
		              }	
                       }
	            }	
                 }	

int take_picture(){	
	*(Video_In_DMA_ptr + 3) = 0x0;
	return 0;}
	
int resume_video(){	
	*(Video_In_DMA_ptr + 3)	= 0x4;					
	return 0;}

int black_screen(){
	int x,y;
	short* base_address = Video_Mem_ptr;
	for (y = 0; y < 240; y++) {
		for (x = 0; x < 320; x++) {
			*(base_address + (y << 9) + x ) = 0x0000;
		}
	}
}

int blackAndWhite(){      // taken from Lab2 and made some adjustions
	int x,y;
	short sum = 0;
	short black = 0x0000; 
	short white = 0xFFFF;
	short* base_address = Video_Mem_ptr;
	
	for (y = 0; y < 240; y++) {
		for (x = 0; x < 320; x++) {
			short value =  *(base_address + (y << 9) + x );
			
			if (value<4000){
				*(base_address + (y << 9) + x ) = black;
				picture_array[x/8][y] &= ~(1 << x%8);	
			}
			else{
				*(base_address + (y << 9) + x ) = white;
				picture_array[x/8][y] |= (1 << x%8);	
			}
		}
	}
	for (y = 0; y < 240; y++) {             
		for (x = 0; x < 320; x++) {
			short temp2 = *(Video_Mem_ptr + (y << 9) + x);
			picture[x][y]= temp2;
			}}
	return 0;
}

int compressionInRle(){

   int bits = 0;
   allx,ally,thisx,thisy,fifo_status,rready,idata,SDRAM_bits = 0;

	alt_write_byte(ALT_FPGA_BRIDGE_LWH2F_OFST + RLE_RESET_BASE, 1);	         //RLE RESET
	alt_write_byte(ALT_FPGA_BRIDGE_LWH2F_OFST + RLE_FLUSH_PIO_BASE, 1);	     // ASSERT
	alt_write_byte(ALT_FPGA_BRIDGE_LWH2F_OFST + RLE_FLUSH_PIO_BASE, 0);	    //DEASSERT
	alt_write_byte(ALT_FPGA_BRIDGE_LWH2F_OFST + RLE_RESET_BASE, 0);	
        
	while((allx < 40 && ally < 240 )&& bits < 76800){             //travering throught pixels
		fifo_status= alt_read_byte(ALT_FPGA_BRIDGE_LWH2F_OFST + FIFO_IN_FULL_PIO_BASE);   // get fifo status (full or not)
		rready = alt_read_byte(ALT_FPGA_BRIDGE_LWH2F_OFST + RESULT_READY_PIO_BASE);       // get result ready status

		if(fifo_status == 0){	                                                       // fifo is not full
			alt_write_byte(ALT_FPGA_BRIDGE_LWH2F_OFST + FIFO_IN_WRITE_REQ_PIO_BASE, 1);	  // assert write request pio
			alt_write_byte(ALT_FPGA_BRIDGE_LWH2F_OFST + ODATA_PIO_BASE, picture_array[allx][ally]); // Ouput ports send bit streams
			
			// data is Sent to FIFO
			alt_write_byte(ALT_FPGA_BRIDGE_LWH2F_OFST + FIFO_IN_WRITE_REQ_PIO_BASE, 0);	// deassert write request pio
	  
		                    }
		if(rready == 0)	{      // there is an encoded data segment in the FIFO  , ACTIVE LOW
			alt_write_byte(ALT_FPGA_BRIDGE_LWH2F_OFST + FIFO_OUT_READ_REQ_PIO_BASE, 1);  // assert read req, ARM wishes to read from the FIFO out
			idata = alt_read_word(ALT_FPGA_BRIDGE_LWH2F_OFST + IDATA_PIO_BASE);  // get data using IDATA_PIO, input ports to receive encoded data
			*(SDRAM_start_ptr + SDRAM_bits) = idata;                  // Storing into the SDRAM
			SDRAM_bits++;                                             // sdram address increament
			bits = bits + (idata & 0x007FFFFF);                       // increment bits, 24 bits/output
		
			alt_write_byte(ALT_FPGA_BRIDGE_LWH2F_OFST + FIFO_OUT_READ_REQ_PIO_BASE, 0);  // deassert read request pio, no encoded data to read
          
		}
	} 
	alt_write_byte(ALT_FPGA_BRIDGE_LWH2F_OFST + RLE_FLUSH_PIO_BASE, 1);	  // RLE_FLUSH_PIO,end of the bit-stream. RLE produces the final encoded data segment immediately 
	
	return 0;
} 

// print helper method
void print(){
	int x,y;
    volatile int * KEY_ptr				= (int *) KEY_BASE;
	volatile short * Video_Mem_ptr	= (short *) FPGA_ONCHIP_BASE;
	*(Video_In_DMA_ptr + 3) = 0x0; // Disable the video to capture one frame
	while (*KEY_ptr != 0);	// wait for button to release
	
	for (y = 0; y < 240; y++) {             
		for (x = 0; x < 320; x++) {
			*(Video_Mem_ptr + (y << 9) + x) = picture[x][y];
			                                      }}
	return 0;
}
// Decompressing
int decompressFromSDRAM(){
	short* base_address = Video_Mem_ptr;
    int i,j;
	for (i = 0; i < SDRAM_bits; i++){                                  // Travesing through Sdram
		
		int FirstBit =  *(SDRAM_start_ptr+i) >> 23;                    // shift to obtain first bit
		int SecondValue = *(SDRAM_start_ptr + i) & 0x007FFFFF;         // masking to get last 23 bits, ignore first bit
		recordingBits+=24;
		
		if(FirstBit ==0)
		for(j = 0; j < SecondValue ; j++){                     
				*(base_address + (thisy << 9) + thisx -4) = 0x0000;
			     
		}
		if(FirstBit ==1)
		for(j = 0; j < SecondValue ; j++){                     
				*(base_address + (thisy << 9) + thisx -4) = 0xFFFF;
			     
		}
	}
	
	printf("Compressed image bit number  : %d bits\n", recordingBits);
	printf("Decompressed image bit number: %d bits\n", 76800);
	printf("Compression Ratio: %f\n", (float) (9600/recordingBits));
	
	print();
	return 0;
}
