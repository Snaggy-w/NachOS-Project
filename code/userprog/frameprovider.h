#ifndef FRAMEPROVIDER_H
#define FRAMEPROVIDER_H

#include "bitmap.h"
#include <stdlib.h> // for srand, rand
#include <time.h> // for time

class FrameProvider {
    public:
        BitMap* mainMemMap;

        FrameProvider();

        ~FrameProvider();

        int GetEmptyFrame(bool randomAllocation);

        void ReleaseFrame(int physFrame);

        int NumAvailFrame();
};


#endif //FRAMEPROVIDER_H
