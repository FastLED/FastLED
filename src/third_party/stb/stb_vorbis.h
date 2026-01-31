// stb_vorbis.h - Header for stb_vorbis audio decoder
//
// This file contains the API declarations extracted from stb_vorbis.cpp.hpp.
// It provides the public interface without including the implementation.
//
// For the implementation, include stb_vorbis.cpp.hpp (which is included
// via the unity build system in _build.hpp).
//
// Original: https://github.com/nothings/stb/blob/master/stb_vorbis.c
// Author: Sean Barrett - http://nothings.org/stb_vorbis/
// License: MIT or Public Domain

// Use same guard as original stb_vorbis to prevent type redefinition
#ifndef FL_STB_VORBIS_INCLUDE_FL_STB_VORBIS_H
#define FL_STB_VORBIS_INCLUDE_FL_STB_VORBIS_H

// Ogg Vorbis audio decoder - v1.22 - public domain
// http://nothings.org/stb_vorbis/
//
// Original version written by Sean Barrett in 2007.
//
// Originally sponsored by RAD Game Tools. Seeking implementation
// sponsored by Phillip Bennefall, Marc Andersen, Aaron Baker,
// Elias Software, Aras Pranckevicius, and Sean Barrett.
//
// LICENSE
//
//   See end of file for license information.
//
// Limitations:
//
//   - floor 0 not supported (used in old ogg vorbis files pre-2004)
//   - lossless sample-truncation at beginning ignored
//   - cannot concatenate multiple vorbis streams
//   - sample positions are 32-bit, limiting seekable 192Khz
//       files to around 6 hours (Ogg supports 64-bit)
//
// Feature contributors:
//    Dougall Johnson (sample-exact seeking)
//
// Bugfix/warning contributors:
//    Terje Mathisen     Niklas Frykholm     Andy Hill
//    Casey Muratori     John Bolton         Gargaj
//    Laurent Gomila     Marc LeBlanc        Ronny Chevalier
//    Bernhard Wodo      Evan Balster        github:alxprd
//    Tom Beaumont       Ingo Leitgeb        Nicolas Guillemot
//    Phillip Bennefall  Rohit               Thiago Goulart
//    github:manxorist   Saga Musix          github:infatum
//    Timur Gagiev       Maxwell Koo         Peter Waller
//    github:audinowho   Dougall Johnson     David Reid
//    github:Clownacy    Pedro J. Estebanez  Remi Verschelde
//    AnthoFoxo          github:morlat       Gabriel Ravier
//
// Partial history:
//    1.22    - 2021-07-11 - various small fixes
//    1.21    - 2021-07-02 - fix bug for files with no comments
//    1.20    - 2020-07-11 - several small fixes
//    1.19    - 2020-02-05 - warnings
//    1.18    - 2020-02-02 - fix seek bugs; parse header comments; misc warnings etc.
//    1.17    - 2019-07-08 - fix CVE-2019-13217..CVE-2019-13223 (by ForAllSecure)
//    1.16    - 2019-03-04 - fix warnings
//    1.15    - 2019-02-07 - explicit failure if Ogg Skeleton data is found
//    1.14    - 2018-02-11 - delete bogus dealloca usage
//    1.13    - 2018-01-29 - fix truncation of last frame (hopefully)
//    1.12    - 2017-11-21 - limit residue begin/end to blocksize/2 to avoid large temp allocs in bad/corrupt files
//    1.11    - 2017-07-23 - fix MinGW compilation
//    1.10    - 2017-03-03 - more robust seeking; fix negative ilog(); clear error in open_memory
//    1.09    - 2016-04-04 - back out 'truncation of last frame' fix from previous version
//    1.08    - 2016-04-02 - warnings; setup memory leaks; truncation of last frame
//    1.07    - 2015-01-16 - fixes for crashes on invalid files; warning fixes; const
//    1.06    - 2015-08-31 - full, correct support for seeking API (Dougall Johnson)
//                           some crash fixes when out of memory or with corrupt files
//                           fix some inappropriately signed shifts
//    1.05    - 2015-04-19 - don't define __forceinline if it's redundant
//    1.04    - 2014-08-27 - fix missing const-correct case in API
//    1.03    - 2014-08-07 - warning fixes
//    1.02    - 2014-07-09 - declare qsort comparison as explicitly _cdecl in Windows
//    1.01    - 2014-06-18 - fix stb_vorbis_get_samples_float (interleaved was correct)
//    1.0     - 2014-05-26 - fix memory leaks; fix warnings; fix bugs in >2-channel;
//                           (API change) report sample rate for decode-full-file funcs
//
// See end of file for full version history.


//////////////////////////////////////////////////////////////////////////////
//
//  HEADER BEGINS HERE
//

// FastLED integration: use fl::numeric_limits instead of UINT_MAX
#include "fl/numeric_limits.h"
#include "fl/stl/stdint.h"

// FastLED integration: use fl::FILE* for file I/O abstraction
#ifndef FL_STB_VORBIS_NO_STDIO
#include "fl/stl/detail/file_io.h"
#endif

// FastLED integration: Disable stdio-based functions by default
// Embedded platforms don't have standard file I/O, and including <stdio.h>
// in headers is banned by the project linter. Users needing file-based
// decoding can define FL_STB_VORBIS_ENABLE_STDIO before including this header.
#ifndef FL_STB_VORBIS_ENABLE_STDIO
#define FL_STB_VORBIS_NO_STDIO 1
#endif

#if defined(FL_STB_VORBIS_NO_CRT) && !defined(FL_STB_VORBIS_NO_STDIO)
#define FL_STB_VORBIS_NO_STDIO 1
#endif

namespace fl {
namespace third_party {
namespace vorbis {

///////////   THREAD SAFETY

// Individual stb_vorbis* handles are not thread-safe; you cannot decode from
// them from multiple threads at the same time. However, you can have multiple
// stb_vorbis* handles and decode from them independently in multiple thrads.


///////////   MEMORY ALLOCATION

// normally stb_vorbis uses malloc() to allocate memory at startup,
// and alloca() to allocate temporary memory during a frame on the
// stack. (Memory consumption will depend on the amount of setup
// data in the file and how you set the compile flags for speed
// vs. size. In my test files the maximal-size usage is ~150KB.)
//
// You can modify the wrapper functions in the source (setup_malloc,
// setup_temp_malloc, temp_malloc) to change this behavior, or you
// can use a simpler allocation model: you pass in a buffer from
// which stb_vorbis will allocate _all_ its memory (including the
// temp memory). "open" may fail with a VORBIS_outofmem if you
// do not pass in enough data; there is no way to determine how
// much you do need except to succeed (at which point you can
// query get_info to find the exact amount required. yes I know
// this is lame).
//
// If you pass in a non-NULL buffer of the type below, allocation
// will occur from it as described above. Otherwise just pass NULL
// to use malloc()/alloca()

typedef struct
{
   char *alloc_buffer;
   int32_t alloc_buffer_length_in_bytes;
} stb_vorbis_alloc;


///////////   FUNCTIONS USEABLE WITH ALL INPUT MODES

typedef struct stb_vorbis stb_vorbis;

typedef struct
{
   uint32_t sample_rate;
   int32_t channels;

   uint32_t setup_memory_required;
   uint32_t setup_temp_memory_required;
   uint32_t temp_memory_required;

   int32_t max_frame_size;
} stb_vorbis_info;

typedef struct
{
   char *vendor;

   int32_t comment_list_length;
   char **comment_list;
} stb_vorbis_comment;

// get general information about the file
stb_vorbis_info stb_vorbis_get_info(stb_vorbis *f);

// get ogg comments
stb_vorbis_comment stb_vorbis_get_comment(stb_vorbis *f);

// get the last error detected (clears it, too)
int32_t stb_vorbis_get_error(stb_vorbis *f);

// close an ogg vorbis file and free all memory in use
void stb_vorbis_close(stb_vorbis *f);

// this function returns the offset (in samples) from the beginning of the
// file that will be returned by the next decode, if it is known, or -1
// otherwise. after a flush_pushdata() call, this may take a while before
// it becomes valid again.
// NOT WORKING YET after a seek with PULLDATA API
int32_t stb_vorbis_get_sample_offset(stb_vorbis *f);

// returns the current seek point within the file, or offset from the beginning
// of the memory buffer. In pushdata mode it returns 0.
uint32_t stb_vorbis_get_file_offset(stb_vorbis *f);

///////////   PUSHDATA API

#ifndef FL_STB_VORBIS_NO_PUSHDATA_API

// this API allows you to get blocks of data from any source and hand
// them to stb_vorbis. you have to buffer them; stb_vorbis will tell
// you how much it used, and you have to give it the rest next time;
// and stb_vorbis may not have enough data to work with and you will
// need to give it the same data again PLUS more. Note that the Vorbis
// specification does not bound the size of an individual frame.

stb_vorbis *stb_vorbis_open_pushdata(
         const unsigned char * datablock, int32_t datablock_length_in_bytes,
         int32_t *datablock_memory_consumed_in_bytes,
         int32_t *error,
         const stb_vorbis_alloc *alloc_buffer);
// create a vorbis decoder by passing in the initial data block containing
//    the ogg&vorbis headers (you don't need to do parse them, just provide
//    the first N bytes of the file--you're told if it's not enough, see below)
// on success, returns an stb_vorbis *, does not set error, returns the amount of
//    data parsed/consumed on this call in *datablock_memory_consumed_in_bytes;
// on failure, returns NULL on error and sets *error, does not change *datablock_memory_consumed
// if returns NULL and *error is VORBIS_need_more_data, then the input block was
//       incomplete and you need to pass in a larger block from the start of the file

int32_t stb_vorbis_decode_frame_pushdata(
         stb_vorbis *f,
         const unsigned char *datablock, int32_t datablock_length_in_bytes,
         int32_t *channels,             // place to write number of float * buffers
         float ***output,           // place to write float ** array of float * buffers
         int32_t *samples               // place to write number of output samples
     );
// decode a frame of audio sample data if possible from the passed-in data block
//
// return value: number of bytes we used from datablock
//
// possible cases:
//     0 bytes used, 0 samples output (need more data)
//     N bytes used, 0 samples output (resynching the stream, keep going)
//     N bytes used, M samples output (one frame of data)
// note that after opening a file, you will ALWAYS get one N-bytes,0-sample
// frame, because Vorbis always "discards" the first frame.
//
// Note that on resynch, stb_vorbis will rarely consume all of the buffer,
// instead only datablock_length_in_bytes-3 or less. This is because it wants
// to avoid missing parts of a page header if they cross a datablock boundary,
// without writing state-machiney code to record a partial detection.
//
// The number of channels returned are stored in *channels (which can be
// NULL--it is always the same as the number of channels reported by
// get_info). *output will contain an array of float* buffers, one per
// channel. In other words, (*output)[0][0] contains the first sample from
// the first channel, and (*output)[1][0] contains the first sample from
// the second channel.
//
// *output points into stb_vorbis's internal output buffer storage; these
// buffers are owned by stb_vorbis and application code should not free
// them or modify their contents. They are transient and will be overwritten
// once you ask for more data to get decoded, so be sure to grab any data
// you need before then.

void stb_vorbis_flush_pushdata(stb_vorbis *f);
// inform stb_vorbis that your next datablock will not be contiguous with
// previous ones (e.g. you've seeked in the data); future attempts to decode
// frames will cause stb_vorbis to resynchronize (as noted above), and
// once it sees a valid Ogg page (typically 4-8KB, as large as 64KB), it
// will begin decoding the _next_ frame.
//
// if you want to seek using pushdata, you need to seek in your file, then
// call stb_vorbis_flush_pushdata(), then start calling decoding, then once
// decoding is returning you data, call stb_vorbis_get_sample_offset, and
// if you don't like the result, seek your file again and repeat.
#endif


//////////   PULLING INPUT API

#ifndef FL_STB_VORBIS_NO_PULLDATA_API
// This API assumes stb_vorbis is allowed to pull data from a source--
// either a block of memory containing the _entire_ vorbis stream, or a
// fl::FILE * that you or it create, or possibly some other reading mechanism
// if you go modify the source to replace the fl::FILE * case with some kind
// of callback to your code. (But if you don't support seeking, you may
// just want to go ahead and use pushdata.)

#if !defined(FL_STB_VORBIS_NO_STDIO) && !defined(FL_STB_VORBIS_NO_INTEGER_CONVERSION)
int32_t stb_vorbis_decode_filename(const char *filename, int32_t *channels, int32_t *sample_rate, int16_t **output);
#endif
#if !defined(FL_STB_VORBIS_NO_INTEGER_CONVERSION)
int32_t stb_vorbis_decode_memory(const unsigned char *mem, int32_t len, int32_t *channels, int32_t *sample_rate, int16_t **output);
#endif
// decode an entire file and output the data interleaved into a malloc()ed
// buffer stored in *output. The return value is the number of samples
// decoded, or -1 if the file could not be opened or was not an ogg vorbis file.
// When you're done with it, just free() the pointer returned in *output.

stb_vorbis * stb_vorbis_open_memory(const unsigned char *data, int32_t len,
                                  int32_t *error, const stb_vorbis_alloc *alloc_buffer);
// create an ogg vorbis decoder from an ogg vorbis stream in memory (note
// this must be the entire stream!). on failure, returns NULL and sets *error

#ifndef FL_STB_VORBIS_NO_STDIO
stb_vorbis * stb_vorbis_open_filename(const char *filename,
                                  int32_t *error, const stb_vorbis_alloc *alloc_buffer);
// create an ogg vorbis decoder from a filename via fopen(). on failure,
// returns NULL and sets *error (possibly to VORBIS_file_open_failure).

stb_vorbis * stb_vorbis_open_file(fl::FILE *f, int32_t close_handle_on_close,
                                  int32_t *error, const stb_vorbis_alloc *alloc_buffer);
// create an ogg vorbis decoder from an open fl::FILE *, looking for a stream at
// the _current_ seek point (ftell). on failure, returns NULL and sets *error.
// note that stb_vorbis must "own" this stream; if you seek it in between
// calls to stb_vorbis, it will become confused. Moreover, if you attempt to
// perform stb_vorbis_seek_*() operations on this file, it will assume it
// owns the _entire_ rest of the file after the start point. Use the next
// function, stb_vorbis_open_file_section(), to limit it.

stb_vorbis * stb_vorbis_open_file_section(fl::FILE *f, int32_t close_handle_on_close,
                int32_t *error, const stb_vorbis_alloc *alloc_buffer, uint32_t len);
// create an ogg vorbis decoder from an open fl::FILE *, looking for a stream at
// the _current_ seek point (ftell); the stream will be of length 'len' bytes.
// on failure, returns NULL and sets *error. note that stb_vorbis must "own"
// this stream; if you seek it in between calls to stb_vorbis, it will become
// confused.
#endif

int32_t stb_vorbis_seek_frame(stb_vorbis *f, uint32_t sample_number);
int32_t stb_vorbis_seek(stb_vorbis *f, uint32_t sample_number);
// these functions seek in the Vorbis file to (approximately) 'sample_number'.
// after calling seek_frame(), the next call to get_frame_*() will include
// the specified sample. after calling stb_vorbis_seek(), the next call to
// stb_vorbis_get_samples_* will start with the specified sample. If you
// do not need to seek to EXACTLY the target sample when using get_samples_*,
// you can also use seek_frame().

int32_t stb_vorbis_seek_start(stb_vorbis *f);
// this function is equivalent to stb_vorbis_seek(f,0)

uint32_t stb_vorbis_stream_length_in_samples(stb_vorbis *f);
float    stb_vorbis_stream_length_in_seconds(stb_vorbis *f);
// these functions return the total length of the vorbis stream

int32_t stb_vorbis_get_frame_float(stb_vorbis *f, int32_t *channels, float ***output);
// decode the next frame and return the number of samples. the number of
// channels returned are stored in *channels (which can be NULL--it is always
// the same as the number of channels reported by get_info). *output will
// contain an array of float* buffers, one per channel. These outputs will
// be overwritten on the next call to stb_vorbis_get_frame_*.
//
// You generally should not intermix calls to stb_vorbis_get_frame_*()
// and stb_vorbis_get_samples_*(), since the latter calls the former.

#ifndef FL_STB_VORBIS_NO_INTEGER_CONVERSION
int32_t stb_vorbis_get_frame_short_interleaved(stb_vorbis *f, int32_t num_c, int16_t *buffer, int32_t num_shorts);
int32_t stb_vorbis_get_frame_short            (stb_vorbis *f, int32_t num_c, int16_t **buffer, int32_t num_samples);
#endif
// decode the next frame and return the number of *samples* per channel.
// Note that for interleaved data, you pass in the number of shorts (the
// size of your array), but the return value is the number of samples per
// channel, not the total number of samples.
//
// The data is coerced to the number of channels you request according to the
// channel coercion rules (see below). You must pass in the size of your
// buffer(s) so that stb_vorbis will not overwrite the end of the buffer.
// The maximum buffer size needed can be gotten from get_info(); however,
// the Vorbis I specification implies an absolute maximum of 4096 samples
// per channel.

// Channel coercion rules:
//    Let M be the number of channels requested, and N the number of channels present,
//    and Cn be the nth channel; let stereo L be the sum of all L and center channels,
//    and stereo R be the sum of all R and center channels (channel assignment from the
//    vorbis spec).
//        M    N       output
//        1    k      sum(Ck) for all k
//        2    *      stereo L, stereo R
//        k    l      k > l, the first l channels, then 0s
//        k    l      k <= l, the first k channels
//    Note that this is not _good_ surround etc. mixing at all! It's just so
//    you get something useful.

int32_t stb_vorbis_get_samples_float_interleaved(stb_vorbis *f, int32_t channels, float *buffer, int32_t num_floats);
int32_t stb_vorbis_get_samples_float(stb_vorbis *f, int32_t channels, float **buffer, int32_t num_samples);
// gets num_samples samples, not necessarily on a frame boundary--this requires
// buffering so you have to supply the buffers. DOES NOT APPLY THE COERCION RULES.
// Returns the number of samples stored per channel; it may be less than requested
// at the end of the file. If there are no more samples in the file, returns 0.

#ifndef FL_STB_VORBIS_NO_INTEGER_CONVERSION
int32_t stb_vorbis_get_samples_short_interleaved(stb_vorbis *f, int32_t channels, int16_t *buffer, int32_t num_shorts);
int32_t stb_vorbis_get_samples_short(stb_vorbis *f, int32_t channels, int16_t **buffer, int32_t num_samples);
#endif
// gets num_samples samples, not necessarily on a frame boundary--this requires
// buffering so you have to supply the buffers. Applies the coercion rules above
// to produce 'channels' channels. Returns the number of samples stored per channel;
// it may be less than requested at the end of the file. If there are no more
// samples in the file, returns 0.

#endif

////////   ERROR CODES

enum STBVorbisError
{
   VORBIS__no_error,

   VORBIS_need_more_data=1,             // not a real error

   VORBIS_invalid_api_mixing,           // can't mix API modes
   VORBIS_outofmem,                     // not enough memory
   VORBIS_feature_not_supported,        // uses floor 0
   VORBIS_too_many_channels,            // FL_STB_VORBIS_MAX_CHANNELS is too small
   VORBIS_file_open_failure,            // fopen() failed
   VORBIS_seek_without_length,          // can't seek in unknown-length file

   VORBIS_unexpected_eof=10,            // file is truncated?
   VORBIS_seek_invalid,                 // seek past EOF

   // decoding errors (corrupt/invalid stream) -- you probably
   // don't care about the exact details of these

   // vorbis errors:
   VORBIS_invalid_setup=20,
   VORBIS_invalid_stream,

   // ogg errors:
   VORBIS_missing_capture_pattern=30,
   VORBIS_invalid_stream_structure_version,
   VORBIS_continued_packet_flag_invalid,
   VORBIS_incorrect_stream_serial_number,
   VORBIS_invalid_first_page,
   VORBIS_bad_packet_type,
   VORBIS_cant_find_last_page,
   VORBIS_seek_failed,
   VORBIS_ogg_skeleton_not_supported
};


} // namespace vorbis
} // namespace third_party
} // namespace fl

#endif // FL_STB_VORBIS_INCLUDE_FL_STB_VORBIS_H
