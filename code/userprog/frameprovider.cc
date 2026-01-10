/*
Routines to allocate physical frames to newly created processes and
to free physical frames in case a process finishes execution. 
*/

#include "frameprovider.h"

//#define NumPhysPages 128
//mainMemory + (N x PageSize)


FrameProvider::FrameProvider(){
    this->mainMemMap = new BitMap(NumPhysPages);
    srand(time(NULL));
}

FrameProvider::~FrameProvider() { 
    delete this->mainMemMap;
}

int FrameProvider::GetEmptyFrame(bool randomAllocation){
    if(mainMemMap->NumClear() == 0) return -1;
    int resultFrame;
    if(randomAllocation){
        while(true){
            int i = rand() % NumPhysPages;
            if(!mainMemMap->Test(i)){
                mainMemMap->Mark(i);
                resultFrame = i;
                break;
            }
        }
    }
    else {
        resultFrame = mainMemMap->Find();
    }
    bzero(machine->mainMemory + (resultFrame * PageSize), PageSize);
    return resultFrame;
}

void FrameProvider::ReleaseFrame(int physFrame) {
    mainMemMap->Clear(physFrame);
}

int FrameProvider::NumAvailFrame(){
    return mainMemMap->NumClear();
}