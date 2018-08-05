// Bitmap Reader
//
#pragma warning(disable : 4996)

#include <windows.h>
#include <iostream>
#include <stdio.h>
#include <io.h>
#include <math.h>
#include <string>
#include <map>
#include <iterator>
#include "bpcs.h"

// Global Variables for File Data Pointers
BITMAPFILEHEADER *pCoverFileHdr, *pMsgFileHdr, *pStegoFileHdr;
BITMAPINFOHEADER *pCoverInfoHdr, *pMsgInfoHdr, *pStegoInfoHdr;
RGBQUAD *pSrcColorTable, *pTgtColorTable;
unsigned char *pCoverFile, *pMsgFile, *pStegoFile, *pCoverData, *pMsgData, *pStegoData, *pCoverBlock, *pMsgBlock, *pStegoBlock;
int coverFileSize, msgFileSize, blockNum, lastBit = 0;

const int bitBlockSize = 8;
const int blockSize = 8;

unsigned char toCGC[bitBlockSize][bitBlockSize];
unsigned char toPBC[256];
unsigned char cover_bits[blockSize][bitBlockSize];
unsigned char message_bits[blockSize][bitBlockSize];
unsigned char temp_bits[blockSize][bitBlockSize];
unsigned char stego_bits[blockSize][bitBlockSize];
unsigned char *pTempBlock;

int blockFlag = 0;	//1 for message, 0 for cover
//alpha is the threshold variable
float alpha = 0.30;
float blockComplex = 0.0;


// default values
unsigned char gNumLSB = 1, gMask = 0xfe, gShift = 7;



  // reads specified bitmap file from disk
unsigned char *readFile(char *fileName, int *fileSize)
{
	FILE *ptrFile;
	unsigned char *pFile;

	ptrFile = fopen(fileName, "rb");	// specify read only and binary (no CR/LF added)

	if (ptrFile == NULL)
	{
		printf("Error in opening file: %s.\n\n", fileName);
		return(NULL);
	}

	*fileSize = filelength(fileno(ptrFile));

	// malloc memory to hold the file, include room for the header and color table
	pFile = (unsigned char *)malloc(*fileSize);

	if (pFile == NULL)
	{
		printf("Memory could not be allocated in readFile.\n\n");
		return(NULL);
	}

	// Read in complete file
	// buffer for data, size of each item, max # items, ptr to the file
	fread(pFile, sizeof(unsigned char), *fileSize, ptrFile);
	fclose(ptrFile);
	return(pFile);
} // readFile

  // writes modified bitmap file to disk
  // gMask used to determine the name of the file
int writeFile(unsigned char *pFile, int fileSize, char *fileName)
{
	FILE *ptrFile;
	char newFileName[256], msk[4];
	int x;

	// convert the mask value to a string
	sprintf(msk, "%02x", gMask);

	// make a new filename based upon the original
	strcpy(newFileName, fileName);

	// remove the .bmp (assumed)
	x = (int)strlen(newFileName) - 4;
	newFileName[x] = 0;

	strcat(newFileName, "_mask_");
	strcat(newFileName, msk);	// name indicates which bit plane(s) was/were saved

								// add the .bmp back to the file name
	strcat(newFileName, ".bmp");

	// open the new file, MUST set binary format (text format will add line feed characters)
	ptrFile = fopen(newFileName, "wb+");
	if (ptrFile == NULL)
	{
		printf("Error opening new file for writing.\n\n");
		return(FAILURE);
	}

	// write the file
	x = (int)fwrite(pFile, sizeof(unsigned char), fileSize, ptrFile);

	// check for success
	if (x != fileSize)
	{
		printf("Error writing file %s.\n\n", newFileName);
		return(FAILURE);
	}
	fclose(ptrFile); // close file
	return(SUCCESS);
} // writeFile

  // prints help message to the screen
void printHelpHide()
{
	printf("BPCS: Hiding Mode:\n");
	printf("Usage: bpcs.exe -h 'source filename' 'target filename' ['threshold'] [bit slice]\n\n");
	printf("\tsource filename:\tThe name of the bitmap file to hide.\n");
	printf("\ttarget filename:\tThe name of the bitmap file to conceal within the source.\n");
	printf("\tthreshold:\t\tThe number of bits to hide, range is (.3 - .5).\n");
	printf("The bit slice is the number of bits to hide or extract, range is (1 - 7).\n");
	printf("\t\tIf threshold specified is out of bounds .3 bits will be used as the default.\n\n");
	return;
} // printHelpHide

  // prints help extract message to the screen
void printHelpExtract()
{
	printf("BPCS: Extracting Mode:\n");
	printf("Usage: bpcs.exe -e 'stego filename' ['threshold'] [bit slice]\n\n");
	printf("\tstego filename:\t\tThe name of the file in which a bitmap may be hidden.\n");
	printf("\tthreshold:\t\tThe number of bits to hide, range is (.3 - .5).\n");
	printf("The bit slice is the number of bits to hide or extract, range is (1 - 7).\n");
	printf("\t\tIf threshold specified is out of bounds .3 bits will be used as the default.\n\n");
	return;
} // printHelpExtract

float calcComplexity(unsigned char toCGC[bitBlockSize][bitBlockSize]) {
	int	n = 0, p = 0;
	//Below i calc the change from bit to bit horiz then vert
	int horizChangeCount = 0, vertChangeCount = 0;
	for (p = 0; p < 8; p++) {
		n = 0;
		for (; n < 8; n++) {
			if (toCGC[n][p] = !toCGC[n + 1][p]) { horizChangeCount++; }
		}
	}
	//printf("Horizontal change count is: %d\nComplexity: %f\n", horizChangeCount, ((float)horizChangeCount / 56.00));

	n = 0;
	for (; n < 8; n++) {
		p = 0;
		for (; p < 8; p++) {
			if (toCGC[n][p] = !toCGC[n][p + 1]) { vertChangeCount++; }
		}
	}
	//printf("Vertical change count is: %d\n", vertChangeCount);

	if ((((float)vertChangeCount / 56.00)> alpha) && (((float)horizChangeCount / 56.00)> alpha)) {
		return 1;
	}
	else {
		return 0;
	}
}

float convertToCGC(unsigned char array_bits[blockSize][bitBlockSize]) {
	int	n = 0, p = 0;
	//convert each byte into CGC  
	for (; n < 8; n++) {
		p = 1;
		toCGC[n][0] = array_bits[n][0];
		for (; p < 8; p++) {
			toCGC[n][p] = array_bits[n][p] ^ array_bits[n][p - 1];
		}
	}
	printf("\n");
	return calcComplexity(toCGC);
}

//get 8x8 block and convert to bits
void getBlockBits(unsigned char *pData, int charsToGet) {
	int i = 0, m = 0;
	pTempBlock = pData;

	//change bytes to bits of (charsToGet)x8  I just grab sequentially.
	for (; i < charsToGet; i++) {
		unsigned char currentChar = *pTempBlock;
		int k = 0;
		for (; k < 8; k++) {
			unsigned char x = currentChar;//clean copy of char to work with
			unsigned char y = x << k; //remove unwanted higher bits by shifting bit we want to MSB
			unsigned char z = y >> 7;//then shift the bit we want all the way down to LSB
			temp_bits[m][k] = z; //then store out wanted bit to our storage array
			//printf("%d-", temp_bits[m][k]);
		}

		//printf("Value: %x, Address: %p\n", *pTempBlock, (void *)pTempBlock);
		//printf("\n");
		pTempBlock++;
		m++;
	}
}


void conjugateBits(unsigned char bits[blockSize][bitBlockSize])
{
	//if the newly embedded block is not complex enough then we must conjugate it 
	//to raise its complexity
	printf("Stego block not complex enough\n\n");
	int i = 0;
	for (; i < 8; i++) {
		int d = 0;
		for (; d < 8; d++) {
			if (d % 2)
				stego_bits[i][d] = stego_bits[i][d] ^ 1;
			else
				stego_bits[i][d] = stego_bits[i][d] ^ 0;

		}
	}
}

void embed(unsigned char *pMsgBlock, unsigned char *pStegoBlock) {
	int bitPlane = gNumLSB;
	blockFlag = 1;

	unsigned char *pMsgBlockBit;
	pMsgBlockBit = pMsgBlock;

	unsigned char *pStegoBlockIterate;
	pStegoBlockIterate = pStegoBlock;

	printf("\nBitplane is: %d\n", bitPlane);

	/*i pass in the bitplane because this will get for me the nuber of bits I will embed.
	suppose bitplane is 4. then we would only want to embed 4*8bits = 32 totals bits to embed.
	thus when i return the message_bits array will have all the bits I need to embed into
	the cover bits in a new array stego_bits.*/
	getBlockBits(pMsgBlockBit, bitPlane);

	//copy from temp_bits array that getBlockBits populated to message_bits array for further use
	//and to prevent confusion later on
	memcpy(message_bits, temp_bits, sizeof(unsigned char) * blockSize * bitBlockSize);

	//copy cover bits to stego bits so we can get ready to embed
	memcpy(stego_bits, cover_bits, sizeof(unsigned char) * blockSize * bitBlockSize);

	printf("Want to embed into this:\n");
	int c = 0;
	for (; c < blockSize; c++) {
		int d = 0;
		for (; d < bitBlockSize; d++) {
			printf("%d-", stego_bits[c][d]);
		}
		printf("\n");
	}

	printf("\nMessage stream I will embed into stego bit block\n");
	//message bits should never be more than 64 bits total
	unsigned char temp_array[64];
	//copy message bits to linear temp array for easier copying into stego array
	int e = 0, f = 0;
	for (; e < bitPlane; e++) {
		int b = 0;
		for (; b < bitBlockSize; b++) {
			temp_array[f] = message_bits[e][b];
			printf("%d", temp_array[f]);
			f++;
		}
		printf("\n");
	}
	printf("\n");
	printf("\n");

	//I save the last bit from last block into the first bit of this block. 
	//This is because the last bit in the last block has to hold the conjugation bit
	//thus every block has a last bit of bitplane saved for use to denote conjugation
	stego_bits[0][7] = lastBit;

	f = 0;
	int g = 7, h = 1;

	//starting at the LSB bit plane start embeding and stop at the defined bit plane
	//because in earlier part of code I save msb at index 0 of arrays I have to 
	//start embedding at index 7 of these arrays to embed at LSB
	while (f < ((bitPlane * 8) - 1)) {
		for(h = 0; h <bitBlockSize; h++){

			//because I saved the last bit from last block into first bit of this block
			//I must shift the saving of bits by one position forward. The last bit will 
			//be saved and put in the next blocks first bit position
			if (f == 0) h = 1;		
			stego_bits[h][g] = temp_array[f];
			f++;
		}
		g--;
		if (g < 0) g = 7;
	}
	printf("f is: %d\n\n", f);
	lastBit = temp_array[f];

	printf("What the stego bit block looks like after embed:\n");
	int i = 0;
	for (i = 0; i < 8; i++) {
		int d = 0;
		for (; d < 8; d++) {
			printf("%d-", stego_bits[i][d]);
		}
		printf("\n");
	}
	printf("\n");
	printf("\n");
	

	//NOTE:: the last bit in the block only matters for conjugation purposes
	if (convertToCGC(stego_bits) == 0) {
		conjugateBits(stego_bits);
		stego_bits[7][bitPlane - 1] = 1;
	}
	else stego_bits[7][bitPlane - 1] = 0;

	printf("What the stego bit block looks like after conjugation:\n");
	for (i = 0; i < 8; i++) {
		int d = 0;
		for (; d < 8; d++) {
			printf("%d-", stego_bits[i][d]);
		}
		printf("\n");
	}
	printf("\n");
	printf("\n");

	printf("Writing the following to Stegofile in heap\n");
	int sum = 0;
	for (i = 0; i < 8; i++) {
		int d = 0, sum = 0;
		for (; d < 8; d++) {
			if (stego_bits[i][d] == 1) {
				sum += pow(2, (7 - d));
			}
		}

		*pStegoBlockIterate = sum;
		printf("After write, value: %x, Address: %p\n", *pStegoBlockIterate, (void *)pStegoBlockIterate);
		pStegoBlockIterate++;
	}
	
	printf("\n");
	printf("\n");
}

// Main function in LSB Steg
// Parameters are used to indicate the input file and available options
void main(int argc, char *argv[])
{

	if (argc < 3 || argc > 6)
	{
		printHelpHide();
		printHelpExtract();
		return;
	}

	//Roberts CODE;
	// get the number of bits to use for data hiding or data extracting
	// if not specified, default to one
	if ((strcmp(argv[1], "-h") == 0 && argc == 6) || (strcmp(argv[1], "-e") == 0 && argc == 5))
	{
		//assigns the threshold for hiding/extracting and assigns a default if not entered or invalid range
		if (strcmp(argv[1], "-h") == 0)
			alpha = atof(argv[4]);
		else if (strcmp(argv[1], "-e") == 0)
			alpha = atof(argv[3]);

		if (alpha < .3 || alpha > .5)
		{
			alpha = .3;
			printf("The number specified for Threshold was invalid, using the default value of '.3'.\n\n");
		}
		//assigns the gNumLSB
		// the range for gNumLSB is 1 - 7;  if gNumLSB == 0, then the mask would be 0xFF and the
		// shift value would be 8, leaving the target unmodified during embedding or extracting
		//if gNumLSB == 8, then the source would completely replace the target
		
		//takes the user input and converts from hex to integer value
		gNumLSB = *(argv[5]) - 48;
		if(gNumLSB < 1 || gNumLSB > 7)
		{
			gNumLSB = 1;
			printf("The number specified for LSB was invalid, using the default value of '1'.\n\n");
		}
	}

	if (strcmp(argv[1], "-h") == 0) {
		// read the message file
		pMsgFile = readFile(argv[3], &msgFileSize);
		if (pMsgFile == NULL) return;

		// Set up pointers to various parts of message file
		pMsgFileHdr = (BITMAPFILEHEADER *)pMsgFile;
		pMsgInfoHdr = (BITMAPINFOHEADER *)(pMsgFile + sizeof(BITMAPFILEHEADER));
		pTgtColorTable = (RGBQUAD *)(pMsgFile + sizeof(BITMAPFILEHEADER) + pMsgInfoHdr->biSize);

		//pointer to start of data in the message file
		//will be used to grab bits and embed into cover.
		pMsgData = pMsgFile + pMsgFileHdr->bfOffBits;

		int sizeOfMsgData = pMsgFileHdr->bfSize - pMsgFileHdr->bfOffBits;

		// read the source file
		pCoverFile = readFile(argv[2], &coverFileSize);
		if (pCoverFile == NULL) return;

		// Set up pointers to various parts of the source file
		pCoverFileHdr = (BITMAPFILEHEADER *)pCoverFile;
		pCoverInfoHdr = (BITMAPINFOHEADER *)(pCoverFile + sizeof(BITMAPFILEHEADER));

		// file header indicates where image data begins
		pCoverData = pCoverFile + pCoverFileHdr->bfOffBits;

		pCoverBlock = pCoverData;
		printf("Size of file in bytes: %ld\n", pCoverFileHdr->bfSize);

		int sizeOfCoverData = pCoverFileHdr->bfSize - pCoverFileHdr->bfOffBits;
		int iterateCover = sizeOfCoverData - (sizeOfCoverData % 8);

		//create spcae in heap to hold the stego'ed output file data
		pStegoFile = (unsigned char *)malloc(pCoverFileHdr->bfSize);

		//copy entire cover file into a new heap space
		//this will be our output where our secret message will be contained.
		memcpy(pStegoFile, pCoverFile, coverFileSize);

		pStegoFileHdr = (BITMAPFILEHEADER *)pStegoFile;
		pStegoInfoHdr = (BITMAPINFOHEADER *)(pStegoFile + sizeof(BITMAPFILEHEADER));

		pStegoData = pStegoFile + pStegoFileHdr->bfOffBits;
		/*Here is where I start the loop for grabbing bits and checking for complexity and
		embed if complex enough. I move the cover data pointer every 8 chars for each iteration.
		The message data pointer should move every bitplane for every iteration and the stego data pointer
		should move every 8 chars for every iteration. */

		int n = 0;
		//for TESTING I changed size of loop to 8. it should be variable iterateCover
		//get block of bits to work on for this iteration(8 chars per block for cover)
		for (; n < (8 * 5);) {
			getBlockBits(pCoverBlock, blockSize);	//bits saved to global temp_bits array
			memcpy(cover_bits, temp_bits, sizeof(unsigned char) * blockSize * bitBlockSize); //copy from populated temp_bits to cover_bits
			embed(pMsgData, pStegoData); //this func will grab bits embed message and check for complexity. will conjugate if necessary

			//the functions above iterate 8 times from given pointer to work on current block 
			//so out here we do it as well to pass in next start of block for next iteration
			pStegoData += 8;
			pMsgData += 8;
			pCoverBlock += 8;
			n = n + 8;
			printf("Iteration: %d, Size of cover in bytes: %d\n", n / 8, sizeOfCoverData);
		}
	}
	else {

	}

	// write the file to disk
	//x = writeFile(pMsgFile, pMsgFileHdr->bfSize, argv[2]);


} // main



