// Bitmap Reader
//
#pragma warning(disable : 4996)

#include <windows.h>
#include <stdio.h>
#include <io.h>
#include <math.h>
#include "bpcs.h"

// Global Variables for File Data Pointers
BITMAPFILEHEADER *pSrcFileHdr, *pTgtFileHdr;
BITMAPINFOHEADER *pSrcInfoHdr, *pTgtInfoHdr;
RGBQUAD *pSrcColorTable, *pTgtColorTable;
unsigned char *pSrcFile, *pTgtFile, *pSrcData, *pTgtData, *pSrcBlock;
int srcFileSize, tgtFileSize;

const int bitBlockSize = 8;
const int elementCount = 64;

unsigned char toCGC[bitBlockSize][bitBlockSize];
unsigned char toPBC[256];
unsigned char cover_bits[elementCount][bitBlockSize];
unsigned char *pTempBlock;

float alpha = 0.3;
float blockComplex = 0;


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

float calcComplexity(unsigned char toCGC[bitBlockSize][bitBlockSize]){
	int	n = 0, p = 0;
	//Below i calc the change from bit to bit horiz then vert
	int horizChangeCount = 0, vertChangeCount = 0;
	for (p = 0; p < 8; p++) {
		n = 0;
		for (; n < 8; n++){ 
			if (toCGC[n][p] = !toCGC[n + 1][p]) { horizChangeCount++; }
		}
	}
	printf("Horizontal change count is: %d\n", horizChangeCount);

	n = 0;
	for (; n < 8; n++) {
		p = 0;
		for (; p < 8; p++) {
			if (toCGC[n][p] = !toCGC[n][p+1]) {vertChangeCount++; }
		}
	}
	printf("Vertical change count is: %d\n", vertChangeCount);
	blockComplex = (float)(vertChangeCount + horizChangeCount) / 112.0;
	printf("Block complexity: %lf\n", blockComplex);

	return blockComplex;
}

float convertToCGC(unsigned char cover_bits[elementCount][bitBlockSize]) {
	int	n = 0, p = 0;
	//convert each byte into CGC  
	for (; n < 8; n++) {
		p = 1;
		toCGC[n][0] = cover_bits[n][0];
		for (; p < 8; p++) {
			toCGC[n][p] = cover_bits[n][p] ^ cover_bits[n][p - 1];
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
float getBlock(unsigned char *pSrcData) {
	int i = 0, m = 0;
	pTempBlock = pSrcData;

	printf("Print PCB of 8x8 block\n");

	//change bytes to bits of 8x8  I just grab sequentially.
	for (; i < 8; i++) {
		unsigned char currentChar = *pTempBlock;

		int k = 0;
		for (; k < 8; k++) {
			unsigned char x = (unsigned char)currentChar;//clean copy of char to work with
			unsigned char y = x << k; //remove unwanted higher bits by shifting bit we want to MSB
			unsigned char z = y >> 7;//then shift the bit we want all the way down to LSB
			cover_bits[m][k] = z; //then store out wanted bit to our storage array
			printf("%d-", cover_bits[m][k]);
		}

		printf("Value: %x, Address: %p\n", *pTempBlock, (void *)pTempBlock);
		printf("\n");
		pTempBlock++;
		m++;
	}

	// test to see if I got the correct bits
	//printf("%d-", cover_bits[7][0]);
	//printf("%d-", cover_bits[7][1]);
	//printf("%d-", cover_bits[7][2]);
	//printf("%d-", cover_bits[7][3]);
	//printf("%d-", cover_bits[7][4]);
	//printf("%d-", cover_bits[7][5]);
	//printf("%d-", cover_bits[7][6]);
	//printf("%d-", cover_bits[7][7]);
	//printf("\n");

	return convertToCGC(cover_bits);
}




  // Main function in LSB Steg
  // Parameters are used to indicate the input file and available options
void main(int argc, char *argv[])
{
	int x;

	if (argc < 3 || argc > 4)
	{
		printHelp();
		return;
	}

	// get the number of bits to use for data hiding or data extracting
	// if not specified, default to one
	if (argc == 4)
	{
		// the range for gNumLSB is 1 - 7;  if gNumLSB == 0, then the mask would be 0xFF and the
		// shift value would be 8, leaving the target unmodified during embedding or extracting
		// if gNumLSB == 8, then the source would completely replace the target
		gNumLSB = *(argv[3]) - 48;
		if (gNumLSB < 1 || gNumLSB > 7)
		{
			gNumLSB = 1;
			printf("The number specified for LSB was invalid, using the default value of '1'.\n\n");
		}
		gMask = 256 - (int)pow(2, gNumLSB);
		gShift = 8 - gNumLSB;
	}

	// read the source file
	pSrcFile = readFile(argv[1], &srcFileSize);
	if (pSrcFile == NULL) return;

	// Set up pointers to various parts of the source file
	pSrcFileHdr = (BITMAPFILEHEADER *)pSrcFile;
	pSrcInfoHdr = (BITMAPINFOHEADER *)(pSrcFile + sizeof(BITMAPFILEHEADER));

	// pointer to first RGB color palette, follows file header and bitmap header
	pSrcColorTable = (RGBQUAD *)(pSrcFile + sizeof(BITMAPFILEHEADER) + pSrcInfoHdr->biSize);

	// file header indicates where image data begins
	pSrcData = pSrcFile + pSrcFileHdr->bfOffBits;

	pSrcBlock = pSrcData;
	printf("Size of file: %ld\n", pSrcFileHdr->bfSize);

	int sizeOfData = pSrcFileHdr->bfSize - pSrcFileHdr->bfOffBits;
	
	int iterate = sizeOfData - (sizeOfData % 8);
	printf("Size of data: %ld\n", sizeOfData);

	/*Here is where I start the loop for grabbing bits and checking for complexity and 
	embed if complex enough.*/
	int n = 0;
	for (; n < iterate;) {
		
		//if block complex enuff then embed from here
		//because we still on the block we working on
		if (getBlock(pSrcBlock) > alpha) { 
			printf("Complex enough!!!!\n\n");
		}
		//if block not complex enuff then move on to next block
		else {
			printf("Not complex enough!!!!\n\n");
		}
		pSrcBlock = pSrcBlock + 8;
		n = n + 8;
		printf("%d, %d\n", n, sizeOfData);
	}
	
	 //for debugging purposes, show file info on the screen
	//displayFileInfo(argv[1], pSrcFileHdr, pSrcInfoHdr, pSrcColorTable, pSrcData);


	// read the target file
	pTgtFile = readFile(argv[2], &tgtFileSize);
	if (pTgtFile == NULL) return;

	// Set up pointers to various parts of file
	pTgtFileHdr = (BITMAPFILEHEADER *)pTgtFile;
	pTgtInfoHdr = (BITMAPINFOHEADER *)(pTgtFile + sizeof(BITMAPFILEHEADER));
	pTgtColorTable = (RGBQUAD *)(pTgtFile + sizeof(BITMAPFILEHEADER) + pTgtInfoHdr->biSize);
	pTgtData = pTgtFile + pTgtFileHdr->bfOffBits;

	// for debugging purposes, show file info on the screen
	//displayFileInfo(argv[2], pTgtFileHdr, pTgtInfoHdr, pTgtColorTable, pTgtData);

	// write the file to disk
	//x = writeFile(pTgtFile, pTgtFileHdr->bfSize, argv[2]);
	printf("Pointer value to start of data: %x, At Address: %p\n", *pSrcData, (void *)pSrcData);
	printf("Pointer value to end of data: %x, At Address: %p\n", *pSrcBlock, (void *)pSrcBlock);
	printf("%d\n", pSrcFileHdr->bfOffBits);
	return;
} // main



