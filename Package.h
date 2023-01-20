#define PACKAGE_BUFFER_SIZE 16

typedef struct Package {
	int seqNr;
	unsigned short checkSum;
	char column[PACKAGE_BUFFER_SIZE];
} Package;