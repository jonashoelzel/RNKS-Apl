// 16 -bit 1's complement
// https://tools.ietf.org/html/rfc1071#section-4 
//
#include <stdio.h>

unsigned short checksum(unsigned short *addr, unsigned int count)
{
	
	/* Compute Internet Checksum for "count" bytes
	*         beginning at location "addr".
	*/
	register long sum = 0;
	while (count > 1)  {
		/*  This is the inner loop */
		sum += *(unsigned short *)addr++;
		
		count -= 2;
	}
	/*  Add left-over byte, if any */
	if (count > 0)
		sum += *(unsigned char *)addr;

	/*  Fold 32-bit sum to 16 bits */
	while (sum >> 16)
		sum = (sum & 0xffff) + (sum >> 16);
	return (unsigned short)~sum;  //htons if network byte order is essential
}