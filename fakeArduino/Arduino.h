#ifndef __ARDUINO__H_
#define __ARDUINO__H_

#include <sys/time.h>
#include <stdio.h>
#include <unistd.h>

typedef unsigned char byte;
typedef unsigned char RwReg;
typedef unsigned char RoReg;
typedef bool boolean;

inline unsigned long micros(void)
{
  struct timeval current;
  gettimeofday(&current, NULL);
  return (current.tv_sec * 1000000L) + current.tv_usec;
}


inline unsigned long millis(void)
{
  struct timeval current;
  gettimeofday(&current, NULL);
  return (current.tv_sec * 1000L) + (current.tv_usec/1000L);
}

inline void delay(unsigned long d) 
{
  unsigned long start = millis() + d;
  while(millis() < start);
}


#define F_CPU 1000000L
#endif //__ARDUINO__H_
