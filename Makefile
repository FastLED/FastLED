.PHONEY: all
.PHONEY: clean

CPPFLAGS=-I./fakeArduino/ -g
#LDFLAGS=-lFastLED

OBJS = colorutils.o hsv2rgb.o FastLED.o wiring.o power_mgt.o noise.o lib8tion.o lib8tion.o colorpalettes.o lib8tion.o main.o

all: main

main : $(OBJS)
	$(CXX)  -o $@ $^ 


clean:
	 rm -f $(OBJS) main
