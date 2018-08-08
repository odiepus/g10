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

//define macros for use in mapping conjugation
#define SetBit(A, k) (A[(k/32)] |= (1 << (k%32)))
#define TestBit(A, k) (A[(k/32)] & (1 << (k%32)))

// Global Variables for File Data Pointers
BITMAPFILEHEADER *pCoverFileHdr, *pMsgFileHdr, *pStegoFileHdr, *pExtractFileHdr, *pOutFileHdr;
BITMAPINFOHEADER *pCoverInfoHdr, *pMsgInfoHdr, *pStegoInfoHdr, *pExtractInfoHdr, *pOutInfoHdr;
RGBQUAD *pSrcColorTable, *pTgtColorTable;

unsigned char *pCoverFile, *pMsgFile, *pStegoFile, *pExtractFile, *pOutFile,
*pCoverData, *pMsgData, *pStegoData, *pExtractData, *pOutData,
*pCoverBlock, *pMsgBlock, *pStegoBlock, *pExtractBlock, *pOutBlock;

unsigned char *pTempBlock;

int coverFileSize, msgFileSize, extractFileSize, pOutFileSize, blockNum;
int conjugationMap[8132]; //set a conjugation map to 2^13 ints each at 32 bits long thus 262144 entries

const int bitBlockSize = 8;
const int blockSize = 8;

unsigned char toCGC[bitBlockSize][bitBlockSize];
unsigned char toPBC[256];
unsigned char cover_bits[blockSize][bitBlockSize];
unsigned char message_bits[blockSize][bitBlockSize];
unsigned char temp_bits[blockSize][bitBlockSize];
unsigned char conjugate_bits[blockSize][bitBlockSize];
unsigned char stego_bits[blockSize][bitBlockSize];
unsigned char extract_bits[blockSize][bitBlockSize];
unsigned char out_bits[blockSize][bitBlockSize];

unsigned char temp_out_array[65];
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
int writeFile(unsigned char *pFile, int fileSize, int flag)
{
	FILE *ptrFile;
	char newFileName[256], msk[4];
	int x;

	// convert the mask value to a string
	sprintf(msk, "%d", gNumLSB);

	// make a new filename based upon the original
	strcpy(newFileName, "Output_File");

	// remove the .bmp (assumed)
	x = (int)strlen(newFileName);
	newFileName[x] = 0;

	//If the switch is set to -h
	if (flag == 1) {
		strcat(newFileName, "_mask_");
		strcat(newFileName, msk);	// name indicates which bit plane(s) was/were saved  
	}

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
	printf("Usage: Project1.exe -h 'source filename' 'target filename' ['threshold'] [bit slice]\n\n");
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
	printf("Usage: Project1.exe -e 'stego filename' ['threshold'] [bit slice]\n\n");
	printf("\tstego filename:\t\tThe name of the file in which a bitmap may be hidden.\n");
	printf("\tthreshold:\t\tThe number of bits to hide, range is (.3 - .5).\n");
	printf("The bit slice is the number of bits to hide or extract, range is (1 - 7).\n");
	printf("\t\tIf threshold specified is out of bounds .3 bits will be used as the default.\n\n");
	return;
} // printHelpExtract

float calcComplexity(unsigned char toCGC[bitBlockSize][bitBlockSize], int blockLength) {
	int	n = 0, p = 0;
	//Below i calc the change from bit to bit horiz then vert
	int horizChangeCount = 0, vertChangeCount = 0;
	for (p = 0; p < 8; p++) {
		n = 0;
		for (; n < blockLength; n++) {
			if (toCGC[n][p] = !toCGC[n + 1][p]) { horizChangeCount++; }
		}
	}
	//printf("Horizontal change count is: %d\nComplexity: %f\n", horizChangeCount, ((float)horizChangeCount / 56.00));

	n = 0;
	for (; n < blockLength; n++) {
		p = 0;
		for (; p < 8; p++) {
			if (toCGC[n][p] = !toCGC[n][p + 1]) { vertChangeCount++; }
		}
	}
	//printf("Vertical change count is: %d\n", vertChangeCount);
	if ((((float)vertChangeCount / (blockLength * 7))> alpha) && (((float)horizChangeCount / ((blockLength - 1) * 8))> alpha)) {
		return 1;
	}
	else {
		return 0;
	}
}

float convertToCGC(unsigned char array_bits[blockSize][bitBlockSize], int blockLength) {
	int	n = 0, p = 0;
	//convert each byte into CGC  
	for (; n < blockLength; n++) {
		p = 1;
		toCGC[n][0] = array_bits[n][0];
		for (; p < 8; p++) {
			toCGC[n][p] = array_bits[n][p] ^ array_bits[n][p - 1];
		}
	}
	return calcComplexity(toCGC, blockLength);
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
	int i = 0;
	for (; i < 8; i++) {
		int d = 0;
		for (; d < 8; d++) {
			if (d % 2)
				bits[i][d] = bits[i][d] ^ 1;
			else
				bits[i][d] = bits[i][d] ^ 0;

		}
	}
}

void embed(unsigned char *pMsgBlock, unsigned char *pStegoBlock, int *iteration) {
	int bitPlane = gNumLSB;

	unsigned char *pMsgBlockBit;
	pMsgBlockBit = pMsgBlock;

	unsigned char *pStegoBlockIterate;
	pStegoBlockIterate = pStegoBlock;


	/*i pass in the bitplane because this will get for me the nuber of bits I will embed.
	suppose bitplane is 4. then we would only want to embed 4*8bits = 32 totals bits to embed.
	thus when i return the message_bits array will have all the bits I need to embed into
	the cover bits in a new array stego_bits.*/
	getBlockBits(pMsgBlockBit, bitPlane);

	//copy from temp_bits array that getBlockBits populated to message_bits array for further use
	//and to prevent confusion later on
	memcpy(message_bits, temp_bits, sizeof(unsigned char) * blockSize * bitBlockSize);

	if (convertToCGC(message_bits, bitPlane) == 0) {
		conjugateBits(message_bits);
		SetBit(conjugationMap, *iteration);
	}

	//copy cover bits to stego bits so we can get ready to embed
	memcpy(stego_bits, cover_bits, sizeof(unsigned char) * blockSize * bitBlockSize);


	//message bits should never be more than 64 bits total
	unsigned char temp_array[64];
	//copy message bits to linear temp array for easier copying into stego array
	int e = 0, f = 0;
	for (; e < bitPlane; e++) {
		int b = 0;
		for (; b < bitBlockSize; b++) {
			temp_array[f] = message_bits[e][b];
			f++;
		}
	}

	f = 0;	//use this it iterate thru linear array of messagebits; will be used to stop at correct iteratation

			//g starts at 7 because LSB starts at index 7. h starts at 1 because the initial bit of all blocks is reserved 
			//for lastbit of last block iteration
	int g = 7, h = 0;

	//EMBEDDING: starting at the LSB bit plane start embeding and stop at the defined bit plane
	//because in earlier part of code I save msb at index 0 of arrays I have to 
	//start embedding at index 7 of these arrays to embed at LSB
	while (f < (bitPlane * 8)) {
		for (h = 0; h <bitBlockSize; h++) {
			stego_bits[h][g] = temp_array[f];
			f++;
		}
		g--;
		if (g < 0) g = 7;
	}
	double sum = 0;
	int i = 0;
	for (i = 0; i < 8; i++) {
		int d = 0, sum = 0;
		for (; d < 8; d++) {
			if (stego_bits[i][d] == 1) {
				sum += pow(2, (7 - d));
			}
		}

		*pStegoBlockIterate = sum;
		pStegoBlockIterate++;
	}

}


void embedMap(int map[8192], unsigned char bits[blockSize][bitBlockSize], int *count, unsigned char *pStegoBlock) {
	unsigned char temp_map_array[blockSize][bitBlockSize];
	unsigned char *pStegoBlockIterate;
	pStegoBlockIterate = pStegoBlock;
	int i = 0;
	for (; i < gNumLSB; i++) {
		int d = 0;
		for (; d < bitBlockSize; d++) {
			if (TestBit(conjugationMap, *count)) {
				temp_map_array[i][d] = 1;
			}
			else {
				temp_map_array[i][d] = 0;
			}
			*count += 1;
		}
	}

	unsigned char flatten_array[64];
	//flatten the conjugation map values to linear temp array for easier copying into stego array
	int e = 0, f = 0; //f is used for the linear array
	for (; e < gNumLSB; e++) {
		int b = 0;
		for (; b < bitBlockSize; b++) {
			flatten_array[f] = temp_map_array[e][b];
			f++;
		}
	}

	//g starts at 7 because LSB starts at index 7. h starts at 1 because the initial bit of all blocks is reserved 
	//for lastbit of last block iteration
	int g = 7, h = 0;
	f = 0; //f is reset for use again for the linear array

		   //EMBEDDING: starting at the LSB bit plane start embeding and stop at the defined bit plane
		   //because in earlier part of code I save msb at index 0 of arrays I have to 
		   //start embedding at index 7 of these arrays to embed at LSB
	while (f < (gNumLSB * 8)) {
		for (h = 0; h <bitBlockSize; h++) {
			bits[h][g] = flatten_array[f];
			f++;
		}
		g--;
		if (g < 0) g = 7;
	}
	double sum = 0;
	for (i = 0; i < 8; i++) {
		int d = 0, sum = 0;
		for (; d < 8; d++) {
			if (bits[i][d] == 1) {
				sum += pow(2, (7 - d));
			}
		}
		*pStegoBlockIterate = sum;
		pStegoBlockIterate++;
	}
}

void extractData(unsigned char *pExtractBlock, unsigned char *pOutFile, int *iteration) {
	//save the passed in pointers to local variables for use locally
	unsigned char *pExtBlock = pExtractBlock;
	unsigned char *pOutBlock = pOutFile;
	unsigned char temp_bits[64];

	int bitPlane = gNumLSB;	//save bitplane value to local variable 

							//extract the message block from the 8x8 block
	int i = 7, f = 0;
	for (; i > gNumLSB - 1; i--) {
		int d = 0;
		for (; d < 8; d++) {
			temp_bits[f++] = extract_bits[d][i];
		}
	}



	f = 0;
	//place the bits in linear array into 2d array for conversion to chars
	for (i = 0; i < gNumLSB; i++) {
		int d = 0;
		for (; d < bitBlockSize; d++) {
			out_bits[i][d] = temp_bits[f++];
		}
	}

	//check for conjugation using the iteration to denote the block we are on
	if (TestBit(conjugationMap, *iteration)) {
		conjugateBits(out_bits);	//if bit is 1 then we conjugate the block to get back original message
	}

	//code to convert bits to char for storage to outfile in heap
	int sum = 0;
	for (i = 0; i < 8; i++) {
		int d = 0, sum = 0;
		for (; d < 8; d++) {
			if (out_bits[i][d] == 1) {
				sum += pow(2, (7 - d));
			}
		}

		*pOutBlock = sum;
		pOutBlock++;
	}


}
// Main function in LSB Steg
// Parameters are used to indicate the input file and available options
void main(int argc, char *argv[])
{
	//flag for the switches -h(1) or -e(0) 
	int flag = 0;

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
		if (strcmp(argv[1], "-h") == 0) {
			alpha = atof(argv[4]);
			flag = 1;
		}
		else if (strcmp(argv[1], "-e") == 0) {
			alpha = atof(argv[3]);
			flag = 0;
		}
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
		if (strcmp(argv[1], "-e") == 0) {
			gNumLSB = *(argv[4]) - 48;
		}
		else {
			gNumLSB = *(argv[5]) - 48;
		}
		if (gNumLSB < 1 || gNumLSB > 7)
		{
			gNumLSB = 1;
			printf("The number specified for LSB was invalid, using the default value of '1'.\n\n");
		}
	}

	int x = 0;
	//set conjugation map to all 0's
	for (; x < 8192; x++) {
		conjugationMap[x] = 0;
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
		pMsgBlock = pMsgFile;

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
		getBlockBits(pCoverBlock, blockSize);

		pStegoFileHdr = (BITMAPFILEHEADER *)pStegoFile;
		pStegoInfoHdr = (BITMAPINFOHEADER *)(pStegoFile + sizeof(BITMAPFILEHEADER));

		pStegoData = pStegoFile + pStegoFileHdr->bfOffBits;
		pStegoBlock = pStegoData;

		int msgSize = pMsgFileHdr->bfSize;
		/*Here is where I start the loop for grabbing bits and checking for complexity and
		embed if complex enough. I move the cover data pointer every 8 chars for each iteration.
		The message data pointer should move every bitplane for every iteration and the stego data pointer
		should move every 8 chars for every iteration. */

		int n = 0;
		int iteration = 0;
		//for TESTING I changed size of loop to 8. it should be variable iterateCover
		//get block of bits to work on for this iteration(8 chars per block for cover)
		for (; n < iterateCover;) {
			getBlockBits(pCoverBlock, blockSize);	//bits saved to global temp_bits array
			memcpy(cover_bits, temp_bits, sizeof(unsigned char) * blockSize * bitBlockSize); //copy from populated temp_bits to cover_bits
			embed(pMsgBlock, pStegoBlock, &iteration); //this func will grab bits embed message and check for complexity. will conjugate if necessary
			iteration++;//iterate the block number i am on for use in conjugationMap to record conjugations
						//the functions above iterate 8 times from given pointer to work on current block 
						//so out here we do it as well to pass in next start of block for next iteration
			pStegoBlock += 8;
			pMsgBlock += gNumLSB;
			pCoverBlock += 8;
			n = n + 8;
			if (pMsgBlock > pMsgFileHdr->bfSize + pMsgFile) {
				printf("Reached end of message file\n");
				break;
			}
		}

		//save starting address of where conjugation map will be saved
		unsigned char *pConjAddr;
		pConjAddr = pStegoBlock;
		int intCounter = 0;
		for (; n < iterateCover; ) {
			//EMBEDDING CONJUGATION MAP VALUES
			getBlockBits(pCoverBlock, blockSize);
			memcpy(cover_bits, temp_bits, sizeof(unsigned char) * blockSize * bitBlockSize);
			embedMap(conjugationMap, cover_bits, &intCounter, pStegoBlock);

			pStegoBlock += 8;
			pCoverBlock += 8;
			n += 8;
			printf("Iteration: %d\n", n / 8);
			if (intCounter >= 8192) {
				printf("Reached end of conjucgation map\n");
				break;
			}
		}

		unsigned char *pEndOfStegoFile = pStegoFile + pStegoFileHdr->bfSize;
		unsigned char* pCoverLastBlock = (pCoverFile + pCoverFileHdr->bfSize) - 8;//pointer to last block in the file
		unsigned char *pStegoLastBlock = (pStegoFile + pStegoFileHdr->bfSize) - 8;

		unsigned char *pPrintConjAddr = pConjAddr;
		int offsetFromEnd = 0;
		for (; pConjAddr < pEndOfStegoFile; pConjAddr++) {
			offsetFromEnd++;
		}

		printf("\n\n");
		printf("Start of CoverFile, value: %x, Address: %p\n", *pCoverFile, (void *)pCoverFile);
		printf("Start of StegoFile, value: %x, Address: %p\n", *pStegoFile, (void *)pStegoFile);
		printf("Start of Stego data, value: %x, Address: %p\n", *pStegoData, (void *)pStegoData);
		printf("Conjugation addr: %x\n", (void*)pPrintConjAddr);
		printf("Stego last Block: %x, Stego EOF: %x\n", (void*)pStegoLastBlock, (void*)pEndOfStegoFile);

		printf("Iteration of data embedding: %d\n", iteration);
		printf("Percentage of cover file data that was used by message file: %f\n", (float)(iteration / iterateCover) * 100);
		printf("Offset from EOF: %d\n", offsetFromEnd);
		printf("coverfilesize: %ld\n", coverFileSize);
		printf("stegofilesize: %ld\n", pStegoFileHdr->bfSize);
		printf("\n\n");

		int temp_array[32];

		//grab 32bits that the address of the last block. this last block will embedded with the address
		//of the start of the conjugation map
		int k = 0;
		for (; k < 32; k++) {
			unsigned int x = offsetFromEnd;//clean copy of char to work with
			unsigned int y = x << k; //remove unwanted higher bits by shifting bit we want to MSB
			unsigned int z = y >> 31;//then shift the bit we want all the way down to LSB
			temp_array[k] = z; //then store out wanted bit to our storage array
		}

		getBlockBits(pCoverLastBlock, blockSize);	//get the last block of data to embed address of start of conjugation map values 
		memcpy(cover_bits, temp_bits, sizeof(unsigned char) * blockSize * bitBlockSize);

		//g starts at 7 because LSB starts at index 7. h starts at 1 because the initial bit of all blocks is reserved 
		//for lastbit of last block iteration
		int g = 7, h = 0, f = 0;

		//EMBEDDING CONJUGATION ADDRESS: starting at the LSB bit plane start embeding and stop at the defined bit plane
		//because in earlier part of code I save msb at index 0 of arrays I have to 
		//start embedding at index 7 of these arrays to embed at LSB
		while (f < 32) {
			for (h = 0; h <bitBlockSize; h++) {
				cover_bits[h][g] = temp_array[f];
				f++;
			}
			g--;
			if (g < 0) g = 7;
		}

		int i = 0;
		printf("Writing the following to Stego file in heap\n");
		double sum = 0;
		for (i = 0; i < 8; i++) {
			int d = 0, sum = 0;
			for (; d < 8; d++) {
				if (cover_bits[i][d] == 1) {
					sum += pow(2, (7 - d));
				}
			}
			*pStegoLastBlock = sum;
			printf("After write, value: %x, Address: %p\n", *pStegoLastBlock, (void *)pStegoLastBlock);
			pStegoLastBlock++;
		}
	}
	else {
		pExtractFile = readFile(argv[2], &extractFileSize);
		if (pExtractFile == NULL) return;
		// Set up pointers to various parts of the embedded file
		pExtractFileHdr = (BITMAPFILEHEADER *)pExtractFile;
		pExtractInfoHdr = (BITMAPINFOHEADER *)(pExtractFile + sizeof(BITMAPFILEHEADER));

		// file header indicates where image data begins
		pExtractData = pExtractFile + pExtractFileHdr->bfOffBits;

		pExtractBlock = pExtractData;
		//create heap space for our out file This will contain our secret message
		//that will be written out to our outfile
		pOutFile = (unsigned char *)malloc(pExtractFileHdr->bfSize);

		//copy entire heap of extract file into heap of out file
		//this will be what we write out when we extract the message
		memcpy(pOutFile, pExtractFile, extractFileSize);

		pOutFileHdr = (BITMAPFILEHEADER *)pOutFile;
		pOutInfoHdr = (BITMAPINFOHEADER *)(pOutFile + sizeof(BITMAPFILEHEADER));

		int x = 0;
		//set conjugation map to all 0's
		for (; x < 8192; x++) {
			conjugationMap[x] = 0;
		}

		//pointer to last block in the extract file
		unsigned char * pExtractLastBlock = (pExtractFile + pExtractFileHdr->bfSize) - 8;
		printf("Value at Last Block: %x, Address of Block: %p\n", *pExtractLastBlock, (void*)pExtractLastBlock);

		unsigned char *pEndOfFile = pExtractFile + pExtractFileHdr->bfSize;
		printf("Address of EOF:%x\n", (void*)pEndOfFile);


		//get last block of extract file to get the addres of start of conjugation map values
		getBlockBits(pExtractLastBlock, 8);
		memcpy(extract_bits, temp_bits, sizeof(unsigned char) * blockSize * bitBlockSize);

		unsigned char addr[32];
		//EXTRACTING OFFSET TO CONJUGATION MAP from end
		int i = 7, f = 0;
		for (; i > 3; i--) {
			int d = 0;
			for (; d < 8; d++) {
				addr[f++] = extract_bits[d][i];
			}
		}

		long long sum = 0;
		for (i = 0; i < 32; i++) {
			if (addr[i] == 1) {
				sum += pow(2, (31 - i));
			}
		}

		printf("Offset from EOF:%lld\n", sum);

		while (sum-- > 0) {
			pEndOfFile--;
		}

		printf("Offset from EOF:%ld\n", sum);
		unsigned char *pConjugationAddr = pEndOfFile;
		printf("Address of start of Conjugation Map:%x\n", (void*)pConjugationAddr);

		f = 0;
		while (f < 8192 * 32) {
			getBlockBits(pConjugationAddr, blockSize);
			memcpy(extract_bits, temp_bits, sizeof(unsigned char) * blockSize * bitBlockSize);

			for (i = 0; i < 4; i++) {
				int d = 0;
				for (; d < blockSize; d++) {
					if (extract_bits[d][i] = 1) {
						SetBit(conjugationMap, f++);
					}
				}
			}
			pConjugationAddr += 8;
		}
		//get size of extraction file in 8 x 8 blocks
		int iterateExtract = pExtractFileHdr->bfSize;
		int n = 0, iteration = 0;
		for (; n < iterateExtract; n++) {
			getBlockBits(pExtractBlock, blockSize);
			memcpy(extract_bits, temp_bits, sizeof(unsigned char) * blockSize * bitBlockSize); //copy from populated temp_bits to cover_bits
			extractData(pExtractBlock, pOutData, &iteration);


			pExtractBlock += 8;

			//at some point I will write an if that will stop the loop for a moment to get the header info on message file size
			//so that I can set a stopping point without having to loop all the way thru the stego file.
		}
	}

	// write the file to disk
	if (flag == 1)
		writeFile(pStegoFile, pStegoFileHdr->bfSize, flag);
	else {
		writeFile(pOutFile, pOutFileHdr->bfSize, flag);
	}



} // main



