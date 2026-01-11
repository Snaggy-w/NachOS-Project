#ifndef HELPERS_H
#define HELPERS_H

/*
 * A helpers class that contains utilities could be used by the kernel accross the board
 */ 

// similar to ext4 where a filename is max 255 bytes
#define MAX_FILENAME_LEN 256

class Helpers {
	public:
		// copies a nullbyte terminated string from user to a kernel buffer
		static bool CopyStringFromUser(int userAddr, char *kernelBuff, int maxLen);
};


#endif // HELPERS_H
