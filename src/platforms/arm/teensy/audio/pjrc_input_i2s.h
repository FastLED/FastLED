/* Audio Library for Teensy 3.X
 * Copyright (c) 2014, Paul Stoffregen, paul@pjrc.com
 *
 * Development of this audio library was funded by PJRC.COM, LLC by sales of
 * Teensy and Audio Adaptor boards.  Please support PJRC's efforts to develop
 * open source software by purchasing Teensy or other PJRC products.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice, development funding notice, and this permission
 * notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef _pjrc_input_i2s_h_
#define _pjrc_input_i2s_h_

// IWYU pragma: private

#include "platforms/arm/teensy/audio/pjrc_audio_stream.h"

namespace fl {
namespace platforms {
namespace teensy {

class PjrcAudioInputI2S : public AudioStream
{
public:
	PjrcAudioInputI2S(void) : AudioStream(0, nullptr) { begin(); }
	virtual void update(void);
	void begin(void);
protected:
	PjrcAudioInputI2S(int dummy): AudioStream(0, nullptr) {} // Used only by PjrcAudioInputI2Sslave.
	static void isr(void);
	static void isr1(void);
	static void isr2(void);
};

class PjrcAudioInputI2Sslave : public PjrcAudioInputI2S
{
public:
	PjrcAudioInputI2Sslave(void) : PjrcAudioInputI2S(0) { begin(); }
	void begin(void);
};

} // namespace teensy
} // namespace platforms
} // namespace fl

#endif
