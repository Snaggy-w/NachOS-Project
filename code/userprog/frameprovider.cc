/*
Routines to allocate physical frames to newly created processes and
to free physical frames in case a process finishes execution. 
*/

#include "frameprovider.h"
#include "system.h"


FrameProvider::FrameProvider(){
    this->mainMemMap = new BitMap(NumPhysPages);
    this->mainMemMap_sem = new Semaphore("Bit map semaphore", 1);
    srand(time(NULL));
}

FrameProvider::~FrameProvider() { 
    delete this->mainMemMap;
}

int FrameProvider::GetEmptyFrame(bool randomAllocation){
    mainMemMap_sem->P();
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
    mainMemMap_sem->V();
    return resultFrame;
}

void FrameProvider::ReleaseFrame(int physFrame) {
    mainMemMap_sem->P();
    mainMemMap->Clear(physFrame);
    mainMemMap_sem->V();
}

int FrameProvider::NumAvailFrame(){
    mainMemMap_sem->P();
    int res = mainMemMap->NumClear();
    mainMemMap_sem->V();
    return res;
}
