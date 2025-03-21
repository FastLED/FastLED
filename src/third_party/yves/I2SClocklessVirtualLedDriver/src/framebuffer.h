#pragma once

//#include "_pixelslib.h"
#define _NB_FRAME 2

class frameBuffer
{

public:
Pixel * frames[_NB_FRAME];
uint8_t displayframe;
uint8_t writingframe;
uint16_t frame_size;
bool _framecopy;
frameBuffer()
{
    _framecopy=true;
}
frameBuffer(uint16_t size)
{
    writingframe=0;
    displayframe=1;
    _framecopy=false;
    /*
    * we create the frames
    * to add the logic if the memory is not enough
    */
   frame_size = size;
  // printf("size:%d\n",frame_size);
   //frames[0]=frame;
    for(int i=0;i<_NB_FRAME;i++)
    {
        frames[i] = (Pixel *)calloc(frame_size*sizeof(Pixel),1);
        if(!frames[i])
         printf("no memoory 2\n");
    }
}
frameBuffer(Pixel * frame,uint16_t size)
{
    writingframe=0;
    displayframe=1;
    _framecopy=false;
    /*
    * we create the frames
    * to add the logic if the memory is not enough
    */
   frame_size = size;
  // printf("size:%d\n",frame_size);
   frames[0]=frame;
    for(int i=1;i<_NB_FRAME;i++)
    {
        frames[i] = (Pixel *)calloc(frame_size*sizeof(Pixel),1);
        if(!frames[i])
         printf("no memoory 2\n");
    }
}
    
    Pixel &operator[](int i)
    {
        if(_framecopy)
        return *(frames[0]+i);
        else
        return *(frames[writingframe]+i);
    }
    uint8_t * getFrametoDisplay()
    {
       // uint8_t  * tmp= (uint8_t *)frames[1];
       if(_framecopy)
       {
       memcpy((uint8_t *)frames[1],(uint8_t *)frames[0],frame_size*sizeof(Pixel));
        return (uint8_t *)frames[1];
       }
       else
       {
        writingframe=(writingframe+1)%2;
        displayframe=(displayframe+1)%2;
        return (uint8_t *)frames[displayframe];
       }
    }

void setCopyFunction(bool copy)
{
    _framecopy=copy;
}




};

