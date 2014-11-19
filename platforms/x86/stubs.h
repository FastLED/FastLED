#ifndef __STUB_H__
#define __STUB_H__
#define NO_TIME(x,y,z) 0
#define NS(_NS) _NS

template<int CYCLES>  inline void delaycycles() {}

// Class to ensure that a minimum amount of time has kicked since the last time run - and delay if not enough time has passed yet
// this should make sure that chipsets that have 
template<int WAIT> class CMinWait {
	uint16_t mLastMicros;
public:
	CMinWait() { mLastMicros = 0; }

	void wait() { 
	  /*uint16_t diff;
		do {
			diff = (micros() & 0xFFFF) - mLastMicros;			
			} while(diff < WAIT);*/
	}

	void mark() { /*mLastMicros = micros() & 0xFFFF;*/ }
};

#endif // __STUB_H__
