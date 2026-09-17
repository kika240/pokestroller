#include <stdio.h>
void dumpArrayToFile(void* array, size_t size, char* fileName){
	FILE* fileToWrite = fopen(fileName, "wb");
	if (!fileToWrite) {
		perror(fileName);
		return;
	}
	if (fwrite(array, 1, size, fileToWrite) != size) perror(fileName);
	if (fclose(fileToWrite) != 0) perror(fileName);
}
