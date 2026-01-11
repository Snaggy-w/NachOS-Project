#include "helpers.h"
#include "machine.h"
#include "system.h"



bool Helpers::CopyStringFromUser(int userAddr, char *kernelBuff, int maxLen){
	
	if(kernelBuff == nullptr || maxLen <= 0)
		return false;

	int i = 0;
	int ch;

	while(i < maxLen - 1){
		// invalid access to user address
		if(!machine->ReadMem(userAddr + i, 1, &ch))
			return false;

		kernelBuff[i] = (char)ch;
		// terminate with null byte
		if(kernelBuff[i] == '\0')
			return true;

		i++;
	}
	
	// force nullbyte termination if the string does not contain one already after 255 bytes
	kernelBuff[i] = '\0';
	return true;

}
