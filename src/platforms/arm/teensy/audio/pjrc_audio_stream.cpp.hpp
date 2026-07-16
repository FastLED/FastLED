/* Teensyduino Core Library
 * http://www.pjrc.com/teensy/
 * Copyright (c) 2017 PJRC.COM, LLC.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * 1. The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * 2. If the Software is incorporated into a build system that allows
 * selection among a list of target devices, then similar target
 * devices manufactured by PJRC.COM must be included in the list of
 * target devices and selectable in the same manner.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

// IWYU pragma: private


// IWYU pragma: begin_keep
#include <Arduino.h>
// IWYU pragma: end_keep
#include "platforms/arm/teensy/audio/pjrc_audio_stream.h"
#include "fl/stl/cstring.h"

namespace fl { namespace platforms { namespace teensy {

#if defined(__IMXRT1062__)
constexpr u32 kMaxAudioMemory = 229376;
#else
constexpr u32 kMaxAudioMemory = 229376;
#endif

constexpr u32 kNumMasks = ((kMaxAudioMemory / AUDIO_BLOCK_SAMPLES / 2) + 31) / 32;

struct AudioStreamState {
    audio_block_t* memory_pool = nullptr;
    u32 memory_pool_available_mask[kNumMasks] = {};
    u16 memory_pool_first_mask = 0;
    u16 cpu_cycles_total = 0;
    u16 cpu_cycles_total_max = 0;
    u16 memory_used = 0;
    u16 memory_used_max = 0;
    AudioConnection* unused = nullptr;
    bool update_scheduled = false;
    AudioStream* first_update = nullptr;
};

static AudioStreamState& streamState() {
    return Singleton<AudioStreamState>::instance();
}

AudioConnection*& AudioStream::unusedConnections() { return streamState().unused; }
AudioStream*& AudioStream::firstUpdate() { return streamState().first_update; }
audio_block_t*& AudioStream::memoryPool() { return streamState().memory_pool; }
u32* AudioStream::memoryPoolAvailableMask() {
    return streamState().memory_pool_available_mask;
}
u16& AudioStream::memoryPoolFirstMask() { return streamState().memory_pool_first_mask; }
bool& AudioStream::updateScheduled() { return streamState().update_scheduled; }
u16 AudioStream::cpuCyclesTotal() { return streamState().cpu_cycles_total; }
u16 AudioStream::cpuCyclesTotalMax() { return streamState().cpu_cycles_total_max; }
u16 AudioStream::memoryUsed() { return streamState().memory_used; }
u16 AudioStream::memoryUsedMax() { return streamState().memory_used_max; }
void AudioStream::resetCpuCyclesTotalMax() {
    streamState().cpu_cycles_total_max = streamState().cpu_cycles_total;
}
void AudioStream::resetMemoryUsedMax() {
    streamState().memory_used_max = streamState().memory_used;
}

void software_isr(void);


// Set up the pool of audio data blocks
// placing them all onto the free list
FLASHMEM void AudioStream::initialize_memory(audio_block_t *data, uint num)
{
	uint i;
	uint maxnum = kMaxAudioMemory / AUDIO_BLOCK_SAMPLES / 2;

	//Serial.println("AudioStream initialize_memory");
	//delay(10);
	if (num > maxnum) num = maxnum;
	__disable_irq();
	memoryPool() = data;
	memoryPoolFirstMask() = 0;
	for (i=0; i < kNumMasks; i++) {
		memoryPoolAvailableMask()[i] = 0;
	}
	for (i=0; i < num; i++) {
		memoryPoolAvailableMask()[i >> 5] |= (1 << (i & 0x1F));
	}
	for (i=0; i < num; i++) {
		data[i].memory_pool_index = i;
	}
	if (updateScheduled() == false) {
		// if no hardware I/O has taken responsibility for update,
		// start a timer which will call update_all() at the correct rate
		// The timer must remain alive for the lifetime of the audio stream.
		// ok bare allocation: PJRC's IntervalTimer has no ownership wrapper.
		IntervalTimer *timer = new IntervalTimer(); // ok bare allocation: PJRC timer owns its lifetime
		if (timer) {
			float usec = 1e6 * AUDIO_BLOCK_SAMPLES / AUDIO_SAMPLE_RATE_EXACT;
			timer->begin(update_all, usec);
			update_setup();
		}
	}
	__enable_irq();
}

// Allocate 1 audio data block.  If successful
// the caller is the only owner of this new block
audio_block_t * AudioStream::allocate(void)
{
	u32 n, index, avail;
	u32 *p, *end;
	audio_block_t *block;
	u32 used;

	p = memoryPoolAvailableMask();
	end = p + kNumMasks;
	__disable_irq();
	index = memoryPoolFirstMask();
	p += index;
	while (1) {
		if (p >= end) {
			__enable_irq();
			//Serial.println("alloc:null");
			return nullptr;
		}
		avail = *p;
		if (avail) break;
		index++;
		p++;
	}
	n = __builtin_clz(avail);
	avail &= ~(0x80000000 >> n);
	*p = avail;
	if (!avail) index++;
	memoryPoolFirstMask() = index;
	used = memoryUsed() + 1;
	streamState().memory_used = used;
	__enable_irq();
	index = p - memoryPoolAvailableMask();
	block = memoryPool() + ((index << 5) + (31 - n));
	block->ref_count = 1;
	if (used > memoryUsedMax()) streamState().memory_used_max = used;
	//Serial.print("alloc:");
	//Serial.println((uint32_t)block, HEX);
	return block;
}

// Release ownership of a data block.  If no
// other streams have ownership, the block is
// returned to the free pool
void AudioStream::release(audio_block_t *block)
{
	// The caller owns the block and must pass a valid pointer.
	u32 mask = (0x80000000 >> (31 - (block->memory_pool_index & 0x1F)));
	u32 index = block->memory_pool_index >> 5;

	__disable_irq();
	if (block->ref_count > 1) {
		block->ref_count--;
	} else {
		//Serial.print("reles:");
		//Serial.println((uint32_t)block, HEX);
		memoryPoolAvailableMask()[index] |= mask;
		if (index < memoryPoolFirstMask()) memoryPoolFirstMask() = index;
		streamState().memory_used--;
	}
	__enable_irq();
}

// Transmit an audio data block
// to all streams that connect to an output.  The block
// becomes owned by all the recepients, but also is still
// owned by this object.  Normally, a block must be released
// by the caller after it's transmitted.  This allows the
// caller to transmit to same block to more than 1 output,
// and then release it once after all transmit calls.
void AudioStream::transmit(audio_block_t *block, u8 index)
{
	for (AudioConnection *c = destination_list; c != nullptr; c = c->next_dest) {
		if (c->src_index == index) {
			if (c->dst->inputQueue[c->dest_index] == nullptr) {
				c->dst->inputQueue[c->dest_index] = block;
				block->ref_count++;
			}
		}
	}
}


// Receive block from an input.  The block's data
// may be shared with other streams, so it must not be written
audio_block_t * AudioStream::receiveReadOnly(uint index)
{
	audio_block_t *in;

	if (index >= num_inputs) return nullptr;
	in = inputQueue[index];
	inputQueue[index] = nullptr;
	return in;
}

// Receive block from an input.  The block will not
// be shared, so its contents may be changed.
audio_block_t * AudioStream::receiveWritable(uint index)
{
	audio_block_t *in, *p;

	if (index >= num_inputs) return nullptr;
	in = inputQueue[index];
	inputQueue[index] = nullptr;
	if (in && in->ref_count > 1) {
		p = allocate();
		if (p) fl::memcpy(p->data, in->data, sizeof(p->data));
		in->ref_count--;
		in = p;
	}
	return in;
}

/**************************************************************************************/
// Constructor with no parameters: leave unconnected
AudioConnection::AudioConnection()
	: src(nullptr), dst(nullptr),
	  src_index(0), dest_index(0),
	  isConnected(false)

{
	// we are unused right now, so
	// link ourselves at the start of the unused list
	next_dest = AudioStream::unusedConnections();
	AudioStream::unusedConnections() = this;
}


// Destructor
AudioConnection::~AudioConnection()
{
	AudioConnection** pp;

	disconnect(); // disconnect ourselves: puts us on the unused list
	// Remove ourselves from the unused list
	pp = &AudioStream::unusedConnections();
	while (*pp && *pp != this)
		pp = &((*pp)->next_dest);
	if (*pp) // found ourselves
		*pp = next_dest; // remove ourselves from the unused list
}

/**************************************************************************************/
int AudioConnection::connect(void)
{
	int result = 1;
	AudioConnection *p;
	AudioConnection **pp;
	AudioStream* s;

	do
	{
		if (isConnected) // already connected
		{
			break;
		}

		if (!src || !dst) // Source or destination was destroyed.
		{
			result = 3;
			break;
		}

		if (dest_index >= dst->num_inputs) // input number too high
		{
			result = 2;
			break;
		}

		__disable_irq();

		// First check the destination's input isn't already in use
		s = AudioStream::firstUpdate(); // first AudioStream in the stream list
		while (s) // go through all AudioStream objects
		{
			p = s->destination_list;	// first patchCord in this stream's list
			while (p)
			{
				if (p->dst == dst && p->dest_index == dest_index) // same destination - it's in use!
				{
					__enable_irq();
					return 4;
				}
				p = p->next_dest;
			}
			s = s->next_update;
		}

		// Check we're on the unused list
		pp = &AudioStream::unusedConnections();
		while (*pp && *pp != this)
		{
			pp = &((*pp)->next_dest);
		}
		if (!*pp) // never found ourselves - fail
		{
			result = 5;
			break;
		}

		// Now try to add this connection to the source's destination list
		p = src->destination_list; // first AudioConnection
	if (p == nullptr)
		{
			src->destination_list = this;
		}
		else
		{
			while (p->next_dest)  // scan source Stream's connection list for duplicates
			{

				if (&p->src == &this->src && &p->dst == &this->dst
					&& p->src_index == this->src_index && p->dest_index == this->dest_index)
				{
					//Source and destination already connected through another connection, abort
					__enable_irq();
					return 6;
				}
				p = p->next_dest;
			}

			p->next_dest = this; // end of list, can link ourselves in
		}

		*pp = next_dest;  // remove ourselves from the unused list
		next_dest = nullptr; // we're last in the source's destination list

		src->numConnections++;
		src->active = true;

		dst->numConnections++;
		dst->active = true;

		isConnected = true;

		result = 0;
	} while (0);

	__enable_irq();

	return result;
}


int AudioConnection::connect(AudioStream &source, u8 sourceOutput,
		AudioStream &destination, u8 destinationInput)
{
	int result = 1;

	if (!isConnected)
	{
		src = &source;
		dst = &destination;
		src_index = sourceOutput;
		dest_index = destinationInput;

		result = connect();
	}
	return result;
}

int AudioConnection::disconnect(void)
{
	AudioConnection *p;

	if (!isConnected) return 1;
	if (dest_index >= dst->num_inputs) return 2; // should never happen!
	__disable_irq();

	// Remove destination from source list
	p = src->destination_list;
	if (p == nullptr) {
//>>> PAH re-enable the IRQ
		__enable_irq();
		return 3;
	} else if (p == this) {
		if (p->next_dest) {
			src->destination_list = next_dest;
		} else {
			src->destination_list = nullptr;
		}
	} else {
		while (p)
		{
			if (p->next_dest == this) // found the parent of the disconnecting object
			{
				p-> next_dest = this->next_dest; // skip parent's link past us
				break;
			}
			else
				p = p->next_dest; // carry on down the list
		}
	}
//>>> PAH release the audio buffer properly
	//Remove possible pending src block from destination
	if(dst->inputQueue[dest_index] != nullptr) {
		AudioStream::release(dst->inputQueue[dest_index]);
		// release() re-enables the IRQ. Need it to be disabled a little longer
		__disable_irq();
		dst->inputQueue[dest_index] = nullptr;
	}

	//Check if the disconnected AudioStream objects should still be active
	src->numConnections--;
	if (src->numConnections == 0) {
		src->active = false;
	}

	dst->numConnections--;
	if (dst->numConnections == 0) {
		dst->active = false;
	}

	isConnected = false;
	next_dest = AudioStream::unusedConnections();
	AudioStream::unusedConnections() = this;

	__enable_irq();

	return 0;
}


// When an object has taken responsibility for calling update_all()
// at each block interval (approx 2.9ms), this variable is set to
// true.  Objects that are capable of calling update_all(), typically
// input and output based on interrupts, must check this variable in
// their constructors.
bool AudioStream::update_setup(void)
{
	if (updateScheduled()) return false;
	attachInterruptVector(IRQ_SOFTWARE, software_isr);
	NVIC_SET_PRIORITY(IRQ_SOFTWARE, 208); // 255 = lowest priority
	NVIC_ENABLE_IRQ(IRQ_SOFTWARE);
	updateScheduled() = true;
	return true;
}

void AudioStream::update_stop(void)
{
	NVIC_DISABLE_IRQ(IRQ_SOFTWARE);
	updateScheduled() = false;
}

void software_isr(void) // AudioStream::update_all()
{
	AudioStream *p;

	u32 totalcycles = ARM_DWT_CYCCNT;
	//digitalWriteFast(2, HIGH);
	for (p = AudioStream::firstUpdate(); p; p = p->next_update) {
		if (p->active) {
		u32 cycles = ARM_DWT_CYCCNT;
			p->update();
			// TODO: traverse inputQueueArray and release
			// any input blocks that weren't consumed?
			cycles = (ARM_DWT_CYCCNT - cycles) >> 6;
			p->cpu_cycles = cycles;
			if (cycles > p->cpu_cycles_max) p->cpu_cycles_max = cycles;
		}
	}
	//digitalWriteFast(2, LOW);
	totalcycles = (ARM_DWT_CYCCNT - totalcycles) >> 6;
	streamState().cpu_cycles_total = totalcycles;
	if (totalcycles > streamState().cpu_cycles_total_max)
		streamState().cpu_cycles_total_max = totalcycles;

	asm("DSB");
}

} } } // namespace fl::platforms::teensy
