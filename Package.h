//#ifndef BUFFER_SIZE
#define BUFFER_SIZE 2048
//#endif // !BUFFER_SIZE

typedef struct Package {
	int seqNr;
	int checkSum;
	char column[BUFFER_SIZE];
} Package;