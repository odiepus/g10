// Bitmap Reader
//
#pragma warning(disable : 4996)

#include <windows.h>
#include <stdio.h>
#include <io.h>
#include <math.h>
#include "bpcs.h"

// Global Variables for File Data Pointers
BITMAPFILEHEADER *pCoverFileHdr, *pMsgFileHdr;
BITMAPINFOHEADER *pCoverInfoHdr, *pMsgInfoHdr;
RGBQUAD *pSrcColorTable, *pTgtColorTable;
unsigned char *pCoverFile, *pMsgFile, *pCoverData, *pMsgData, *pCoverBlock, *pMsgBlock;
int coverFileSize, msgFileSize, blockNum;

const int bitBlockSize = 8;
const int elementCount = 64;

unsigned char toCGC[bitBlockSize][bitBlockSize];
unsigned char toPBC[256];
unsigned char cover_bits[elementCount][bitBlockSize];
unsigned char message_bits[elementCount][bitBlockSize];
unsigned char temp_bits[elementCount][bitBlockSize];
unsigned char stego_bits[elementCount][bitBlockSize];
unsigned char *pTempBlock;

int blockFlag = 0;	//1 for message, 0 for cover
float alpha = 0.3;
float blockComplex = 0.0;


// default values
unsigned char gNumLSB = 1, gMask = 0xfe, gShift = 7;

// Show the various bitmap header bytes primarily for debugging
void displayFileInfo(char *pFileName,
	BITMAPFILEHEADER *pFileHdr,
	BITMAPINFOHEADER *pFileInfo,
	RGBQUAD *pColorTable,
	unsigned char *pData)
{
	// first two characters in bitmap file should be "BM"
	char *pp = (char *) &(pFileHdr->bfType);
	int numColors, i;
	RGBQUAD *pCT = pColorTable;

	printf("\nFile Info for %s: \n\n", pFileName);

	printf("File Header Info:\n");
	printf("File Type: %c%c\nFile Size:%d\nData Offset:%d\n\n",
		*pp, *(pp + 1), pFileHdr->bfSize, pFileHdr->bfOffBits);

	switch (pFileInfo->biBitCount)
	{
	case 1:
		numColors = 2;
		break;
	case 4:
		numColors = 16;
		break;
	case 8:
		numColors = 256;
		break;
	case 16:
		numColors = 65536;
		break;
	case 24:
		numColors = 16777216;
		break;
	default:
		numColors = 16777216;
	}

	printf("Bit Map Image Info:\n\nSize:%d\nWidth:%d\nHeight:%d\nPlanes:%d\n"
		"Bits/Pixel:%d ==> %d colors\n"
		"Compression:%d\nImage Size:%d\nRes X:%d\nRes Y:%d\nColors:%d\nImportant Colors:%d\n\n",
		pFileInfo->biSize,
		pFileInfo->biWidth,
		pFileInfo->biHeight,
		pFileInfo->biPlanes,
		pFileInfo->biBitCount, numColors,
		pFileInfo->biCompression,
		pFileInfo->biSizeImage,
		pFileInfo->biXPelsPerMeter,
		pFileInfo->biYPelsPerMeter,
		pFileInfo->biClrUsed,
		pFileInfo->biClrImportant);

	//* Display Color Tables if Desired

	//	There are no color tables
	if (pFileInfo->biBitCount > 16) return;

	//	only needed for debugging
	printf("Color Table:\n\n");

	for (i = 0; i < numColors; i++)
	{
		printf("R:%02x   G:%02x   B:%02x\n", pCT->rgbRed, pCT->rgbGreen, pCT->rgbBlue);
		pCT++;
	}
	//*/

	return;
} // displayFileInfo

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
void printHelp()
{
	printf("Usage: Steg_LSB 'source filename' 'target filename' [bits to use] \n\n");
	printf("Where 'source filename' is the name of the bitmap file to hide.\n");
	printf("Where 'target filename' is the name of the bitmap file to conceal the source.\n");
	printf("To extract data from the source, name the target file \"ex\".\n");
	printf("To bit slice the source, name the target file \"bs\".\n");
	printf("The number of bits to hide or extract, range is (1 - 7).\n");
	printf("If not specified, 1 bit is used as the default.\n\n");
	return;
} // printHelp

  // prints help extract message to the screen
void printHelpExtract()
{
	printf("Steg_BPSC: Extracting Mode:\n");
	printf("Usage: Steg_BPSC -e 'stego filename' ['threshold']\n\n");
	printf("\tstego filename:\t\tThe name of the file in which a bitmap may be hidden.\n");
	printf("\tthreshold:\t\tThe number of bits to hide, range is (1 - 7).\n");
	printf("\t\tIf not specified 1 bit will be used as the default.\n\n");
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
	printf("Horizontal change count is: %d\nComplexity: %f\n", horizChangeCount, ((float)horizChangeCount / 56.00));

	n = 0;
	for (; n < 8; n++) {
		p = 0;
		for (; p < 8; p++) {
			if (toCGC[n][p] = !toCGC[n][p + 1]) { vertChangeCount++; }
		}
	}
	printf("Vertical change count is: %d\n", vertChangeCount);

	if ((((float)vertChangeCount / 56.00)> alpha) && (((float)horizChangeCount / 56.00)> alpha)) {
		return 1;
	}
	else {
		return 0;
	}
}

float convertToCGC(unsigned char array_bits[elementCount][bitBlockSize]) {
	int	n = 0, p = 0;
	//convert each byte into CGC  
	for (; n < 8; n++) {
		p = 1;
		toCGC[n][0] = array_bits[n][0];
		for (; p < 8; p++) {
			toCGC[n][p] = array_bits[n][p] ^ array_bits[n][p - 1];
		}
	}

	//print out CGC
	printf("Print CGC of 8x8 block\n");
	for (n = 0; n < 8; n++) {
		for (p = 0; p < 8; p++) {
			printf("%d-", toCGC[n][p]);
		}
		printf("\n");
	}
	printf("\n");

	return calcComplexity(toCGC);
}

//get 8x8 block and convert to bits
float getBlockBits(unsigned char *pData, int charsToGet, int flag) {
	int i = 0, m = 0;
	pTempBlock = pData;

	printf("Print PCB of (charsToGet)x8 block\n");

	//change bytes to bits of (charsToGet)x8  I just grab sequentially.
	for (; i < charsToGet; i++) {
		unsigned char currentChar = *pTempBlock;

		int k = 0;
		for (; k < 8; k++) {
			unsigned char x = currentChar;//clean copy of char to work with
			unsigned char y = x << k; //remove unwanted higher bits by shifting bit we want to MSB
			unsigned char z = y >> 7;//then shift the bit we want all the way down to LSB
			temp_bits[m][k] = z; //then store out wanted bit to our storage array
			printf("%d-", temp_bits[m][k]);
		}

		printf("Value: %x, Address: %p\n", *pTempBlock, (void *)pTempBlock);
		printf("\n");
		pTempBlock++;
		m++;
	}

	//get size of bit stream so i can copy it to its respective array
	//the arrays will be used later, i think
	size_t sizeOfArray = sizeof(temp_bits);

	if (flag == 0) {
		memcpy(cover_bits, temp_bits, sizeOfArray);
	}
	else {

		size_t sizeM = sizeof(message_bits);
		memcpy(message_bits, temp_bits, sizeOfArray);
	}

	if (flag == 0) {
		return convertToCGC(temp_bits);
	}
	else {
		return 0;
	}
}


void embed(unsigned char * pMsgBlock, unsigned char *pSrcBlock) {
	int bitPlane = gNumLSB, i = 0, j = 0;
	int numOfBitsToEmbed = bitPlane * 8;
	blockFlag = 1;

	printf("\nBitplane is: %d\n", bitPlane);

	/*i pass in the bitplane because this will get for me the nuber of bits I will embed.
	suppose bitplane is 4. then we would only want to embed 4*8bits = 32 totals bits to embed.
	thus when i return the message_bits array will have all the bits I need to embed into
	the cover bits in a new array stego_bits.*/
	getBlockBits(pMsgBlock, bitPlane, blockFlag);
	//printf("%c\n", message_bits[0][0]);

	//starting at the LSB bit plane start embeding and stop at the defined bit plane
	if (convertToCGC(message_bits)) {
		for (j = 0; j < bitPlane; j++) {
			for (i = 0; i < 8; i++) {
				printf("Before: Stego bit at bitplane %x is %x\n", bitPlane, stego_bits[i][j]);
				stego_bits[i][j] = message_bits[i][j];
				printf("After: Stego bit at bitplane %x is %x\n", bitPlane, stego_bits[i][j]);
			}
		}

	}
	else {

	}
	
}

// Main function in LSB Steg
// Parameters are used to indicate the input file and available options
void main(int argc, char *argv[])
{
	if (argc < 3 || argc > 4)
	{
		printHelp();
		printHelpExtract();
		return;
	}

	//Roberts CODE;
	// get the number of bits to use for data hiding or data extracting
	// if not specified, default to one
	if ((strcmp(argv[1], "-h") == 0 && argc == 5) || (strcmp(argv[1], "-e") == 0 && argc == 4))
	{
		// the range for gNumLSB is 1 - 7;  if gNumLSB == 0, then the mask would be 0xFF and the
		// shift value would be 8, leaving the target unmodified during embedding or extracting
		// if gNumLSB == 8, then the source would completely replace the target

		if (strcmp(argv[1], "-h") == 0)
			gNumLSB = (unsigned char)argv[4];
		else if (strcmp(argv[1], "-e") == 0)
			gNumLSB = (unsigned char)argv[3];

		if (gNumLSB < 1 || gNumLSB > 7)
		{
			gNumLSB = 1;
			printf("The number specified for LSB was invalid, using the default value of '1'.\n\n");
		}
		gMask = 256 - (int)pow(2, gNumLSB);
		gShift = 8 - gNumLSB;
	}
	/* Format for hiding		argc
	0	exe					1
	1	-h					2
	2	cover file			3
	3	message file		4
	4	threshold			5
	*/

	// read the message file
	pMsgFile = readFile(argv[2], &msgFileSize);
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
	pCoverFile = readFile(argv[1], &coverFileSize);
	if (pCoverFile == NULL) return;

	// Set up pointers to various parts of the source file
	pCoverFileHdr = (BITMAPFILEHEADER *)pCoverFile;
	pCoverInfoHdr = (BITMAPINFOHEADER *)(pCoverFile + sizeof(BITMAPFILEHEADER));

	// pointer to first RGB color palette, follows file header and bitmap header
	pSrcColorTable = (RGBQUAD *)(pCoverFile + sizeof(BITMAPFILEHEADER) + pCoverInfoHdr->biSize);

	// file header indicates where image data begins
	pCoverData = pCoverFile + pCoverFileHdr->bfOffBits;

	pCoverBlock = pCoverData;
	printf("Size of file: %ld\n", pCoverFileHdr->bfSize);

	int sizeOfCoverData = pCoverFileHdr->bfSize - pCoverFileHdr->bfOffBits;
	int iterateCover = sizeOfCoverData - (sizeOfCoverData % 8);

	//testing
	//embed(pCoverBlock, pMsgData);

	/*Here is where I start the loop for grabbing bits and checking for complexity and
	embed if complex enough.*/
	int n = 0;
	//for testing I changed size of loop to 8. it should be variable iterateCover
	for (; n < 8;) {

		//set flag to let getBlockBits func know to grab bits from cover 
		//convert to CGC and calc coplexity
		blockFlag = 0;

		//if block complex enuff then embed from here
		//because we still on the block we working on
		if (getBlockBits(pCoverBlock, 8, blockFlag)) {
			printf("Complex enough!!!!\n\n");
			embed(pCoverBlock, pMsgData);
		}
		//if block not complex enuff then move on to next block
		else {
			printf("Not complex enough!!!!\n\n");
		}
		pCoverBlock = pCoverBlock + 8;
		n = n + 8;
		printf("%d, %d\n", n, sizeOfCoverData);
	}

	//for debugging purposes, show file info on the screen
	//displayFileInfo(argv[1], pCoverFileHdr, pSrcInfoHdr, pSrcColorTable, pCoverData);

	// for debugging purposes, show file info on the screen
	//displayFileInfo(argv[2], pMsgFileHdr, pMsgInfoHdr, pTgtColorTable, pMsgData);

	// write the file to disk
	//x = writeFile(pMsgFile, pMsgFileHdr->bfSize, argv[2]);
	printf("Pointer value to start of data: %x, At Address: %p\n", *pCoverData, (void *)pCoverData);
	printf("Pointer value to end of data: %x, At Address: %p\n", *pCoverBlock, (void *)pCoverBlock);
	printf("Size of Cover data: %ld\n", sizeOfCoverData);
	printf("Size of Message data: %ld\n", sizeOfMsgData);
	printf("%d\n", pCoverFileHdr->bfOffBits);
	return;
} // main



