/*
	out123: audio output interface

	copyright ?-2015 by the mpg123 project,
	free software under the terms of the LGPL 2.1

	see COPYING and AUTHORS files in distribution or http://mpg123.org
	initially written as audio.h by Michael Hipp, reworked into out123 API
	by Thomas Orgis
*/

/* API TODO:
	- rename audio_output_t to out123_handle
	- ensure that mpg123_encsize() is not used when deciding not to rely on
	  libmpg123; easy enough to reimplement. Should not have been a function
	  from the beginning!
	- Consider zero-copy mode. Ideally, one asks the buffer process for
	  a memory block to write to and avoids any copying.
	  Only trouble is that a desired size might not fit in one piece into
	  the ringbuffer.
	  Same goes for audio driver backends, some might offer the buffers to
	  the client. But this is post-release of the first incarnation:
	  The library should exist and provide functionality first before
	  spending time on irrelevant optimizations.
*/

#ifndef _OUT123_H_
#define _OUT123_H_

/** \defgroup out123_api
 *  This is out123, a library focused on continuous playback of audio streams
 *  via various platform-specific output methods. It glosses over details of
 *  the native APIs to give an interface close to simply writing data to a
 *  file. There might be the option to tune details like buffer (period) sizes
 *  and the number of them on the device side in future, but the focus of the
 *  library is to ease the use case of just getting that raw audio data out
 *  there, without interruptions.
 *
 *  The basic idea is to create a handle with out123_new() and open a certain
 *  output device (using a certain driver module, possibly build-time defaults)
 *  with out123_open(). Now, you can query the output device for supported
 *  encodings for given rate and channel count with out123_get_encodings() and
 *  decide what to use for actually starting playback with out123_start().
 *
 *  Then, you just need to provide (interleaved pcm) data for playback with
 *  out123_play(), which will block when the device's buffers are full. You get
 *  your timing from that (instead of callbacks). If your program does the
 *  production of the audio data just a little bit faster than the playback,
 *  causing out123_play() to block ever so briefly, you're fine.
 *
 *  You stop playback with out123_stop(), or just close the device and driver
 *  via out123_close(), or even just decide to drop it all and do out123_del()
 *  right away when you're done.
 *
 *  There are other functions for specific needs, but the basic idea should be
 *  covered by the above.
 @{
 */

/*
	This includes the mpg123 header, but does only use definitions from there,
	no symbols of the actual mpg123 library.
	Actually, I have to think about that. The string memory management could be
	useful for out123_drivers(), but somehow I feel we should get by without
	linking in an MPEG decoder library. Unless: Offer transparent decoding of
	MP3 directly to the output. Hm, then, WAV and friends should also be
	supported. This widens the scope of the library. Does it also actually
	increase the usefulness?

	Oh, and another point is that the string stuff is optional in libmpg123
	anyway. So, this option of transparently decoding things in the background
	is a separate point from easing the creation of strings with lists of
	driver modules.

	While it might be nice to have a very minimal program to play MPEG data,
	I think the purity of having the output library just deal with PCM data
	should not be given up lightly.

	A more sensible feature should be a WAV header parser to enable a client
	program to auto-configure output format.
*/
#include "mpg123.h"
#include "module.h"

struct audio_output_struct;
typedef struct audio_output_struct audio_output_t;

/** Enumeration of codes for the parameters that it is possible to set/get. */
enum out123_parms
{
	OUT123_FLAGS = 1 /**< integer, various flags, see enum out123_flags */
,	OUT123_PRELOAD /**< float, fraction of buffer to fill before playback */
,	OUT123_GAIN    /**< integer, output device gain (module-specific) */
,	OUT123_VERBOSE /**< integer, verbosity to stderr, >= 0 */
/** float, length of device buffer in seconds
 *  This might be ignored, might have only a loose relation to actual
 *  buffer sizes and latency, depending on output driver. Try to tune
 *  this before opening a device if you want to influcence latency or reduce
 *  dropouts.
 */
,	OUT123_DEVICEBUFFER
};

enum out123_flags
{
	AUDIO_OUT_HEADPHONES       = 0x01 /**< output to headphones (if supported) */
,	AUDIO_OUT_INTERNAL_SPEAKER = 0x02 /**< output to speaker (if supported) */
,	AUDIO_OUT_LINE_OUT         = 0x04 /**< output to line out (if supported) */
,	OUT123_QUIET               = 0x08 /**< no printouts to stdandard error */
/** When this is set (default), playback continues in a loop when the device
 *  does not consume all given data at once. This happens when encountering
 *  signals (like SIGSTOP, SIGCONT) that cause interruption of the underlying
 *  functions.
 */
,	OUT123_KEEP_PLAYING        = 0x10
};

/** Create a new output handle.
 *  This only allocates and initializes memory, so the only possible
 *  error condition is running out of memory.
 * \return pointer to new handle or NULL on error
 */
audio_output_t *out123_new(void);

/** Delete output handle.
 * If there is an output module/device open, it is closed as quickly
 * as possible. This might swallow audio that did not yet reach the
 * speakers.
 * TODO: Settle if out123_close() should do that, too, always requiring
 * out123_drain() when you do not want to loose anything.
 */
void out123_del(audio_output_t *ao);

/** Error code enumeration
 * API calls return a useful (positve) value or zero (OUT123_OK) on simple
 * success. A negative value (-1 == OUT123_ERR) usually indicates that some
 * error occured. Which one, that can be queried using out123_errcode()
 * and friends.
 */
enum out123_error
{
,	OUT123_ERR = -1 /**< generic alias for verbosity, always == -1 */
,	OUT123_OK  = 0  /**< just a name for zero, not going to change */
,	OUT123_DOOM /**< dazzled, out of memory */
,	OUT123_BAD_DRIVER_NAME /**< bad driver name given */
,	OUT123_BAD_DRIVER /**< unspecified issue loading a driver */
,	OUT123_NO_DRIVER /**< no driver loaded */
,	OUT123_NOT_LIVE /**< no active audio device */
,	OUT123_DEV_PLAY /**< some device playback error */
,	OUT123_DEV_OPEN /**< error opening device */
/** Some (really unexpected) error in buffer infrastructure. */	
,	OUT123_BUFFER_ERROR
,	OUT123_ERRCOUNT /**< placeholder for shaping arrays */
};

/** Get string representation of last encountered error in the
 *  context of given handle.
 *  \return error string
 */
const char* out123_strerror(audio_output_t *ao);

/** Get the plain errcode intead of a string.
 * \return error code recorded in handle or OUT123_BAD_HANDLE
 */
int out123_errcode(audio_output_t *ao);

/** Return the error string for a given error code.
 * \param errcode the integer error code
 */
const char* out123_plain_strerror(int errcode);

/** Set a desired output buffer size.
 *  This starts a separate process that handles the audio output, decoupling
 *  the latter from the main process with a memory buffer and saving you the
 *  burden to ensure sparing CPU cycles for actual playback.
 *  This is for applicatons that prefer continuous playback over small latency.
 *  In other words: The kind of applications that out123 is designed for.
 *  This routine always kills of any currently active audio output module /
 *  device, even if you just disable the buffer when there is no buffer.
 *
 *  Keep this in mind for memory-constrainted systems: Activating the
 *  buffer causes a fork of the calling process, doubling the virtual memory
 *  use. Depending on your operating system kernel's behaviour regarding
 *  memory overcommit, it might be wise to call out123_set_buffer() very
 *  early in your program before allocating lots of memory.
 *
 *  There _might_ be a change to threads in future, but for now this is
 *  classic fork with shared memory, working without any threading library.
 *  If your platform or build does not support that, you will always get an
 *  error on trying to set up a non-zero buffer (but the API call will be
 *  present).
 *
 *  Also, if you do intend to use this from a multithreaded program, think
 *  twice and make sure that your setup is happy with forking full-blown
 *  processes off threaded programs. Probably you are better off spawning a
 *  buffer thread yourself.
 *
 * \param buffer_size size (bytes) of a memory buffer for decoded audio,
 *    a value of zero disables the buffer.
 * \return 0 on success, MPG123_ERR on error
 */
int out123_set_buffer(audio_output_t *ao, size_t buffer_bytes);

/** Set a specific parameter, for a specific out123_handle, using a parameter 
 *  code chosen from the out123_parms enumeration, to the specified value.
 *  The parameters usually only change what happens on next out123_open, not
 *  incfluencing running operation.
 * \param code parameter code
 * \param value input value for integer parameters
 * \param fvalue input value for floating point parameters
 * \return 0 on success, MPG123_ERR on error.
 */
int out123_param( audio_output_t *ao, enum out123_parms code
,                 long value, double fvalue );

/** Get a specific parameter, for a specific out123_handle, using a parameter
 *  code chosen from the out123_parms enumeration, to the specified value.
 * \param code parameter code
 * \param value output address for integer parameters
 * \param fvalue output address for floating point parameters
 * \return 0 on success, MPG123_ERR on error (bad parameter name or bad handle).
 */
audio_output_t *out123_getparam( audio_output_t *ao, enum out123_parms code
,                                long *ret_value, double *ret_fvalue );

/** Copy parameters from another out123_handle.
 * \param from_ao the handle to copy parameters from
 */
int audio_output_t *out123_param_from( audio_output_t *ao
                                       audio_output_t* from_ao );

/** Get list of driver modules reachable in system.
 *  The client is responsible for freeing the memory of the return value!
 * \return comma-separated list of drivers in one freshly allocated string
 */
char * out123_drivers(void);

/** Get list of driver modules that actually can be loaded
 *   (library dependencies met).
 *   The client is responsible for freeing the memory of the return value!
 * \return comma-separated list of drivers in one freshly allocated string
 */
char * out123_supported_drivers(void);

/** Open an output device with a certain driver
 *  Note: Opening means that the driver code is loaded and the desired
 *  device name recorded, possibly tested for availability or tentatively
 *  opened. After out123_open(), you can ask for supported encodings
 *  and then really open the device for playback with out123_start().
 * \param driver (comma-separated list of) output driver name(s to try),
 *               NULL for default (stdout for file-based drivers)
 * \param device device name to open, NULL for default
 * \return 0 on success, -1 on error.
 */
int out123_open(audio_output_t *ao, const char* driver, const char* device);

/** Close the current output device and driver. */
void out123_close(audio_output_t *ao);

/** Get supported audio encodings for given rate and channel count,
 *  for the currently openend audio device.
 *  Usually, a wider range of rates is supported, but the number
 *  of sample encodings is limited, as is the number of channels.
 *  So you can call this with some standard rate and hope that the
 *  returned encodings work also for others, with the tested channel
 *  count.
 *  The return value of -1 on some encountered error conveniently also
 *  does not match any defined format (only 15 bits used for encodings,
 *  so this would even work with 16 bit integers).
 *  This implies out123_stop() to enter query mode.
 * \param rate sampling rate
 * \param channels number of channels
 * \return supported encodings combined with bitwise or, to be checked
 *         against your favourite bitmask, -1 on error
 */
int out123_get_encodings(audio_output_t *ao, int channels, long rate);

/** Get size of one PCM sample with given encoding.
 *  This is a macro that might belong into a header file separate from
 *  both mpg123.h and out123.h. No need to have those generic definitions
 *  tied to library code.
 *  Also: Thomas really likes the ?: operator.
 * \param encoding the encoding (mpg123_enc_enum value)
 */
#define out123_samplesize(enc) ( \
	(enc) & MPG123_ENC_8 \
	?	1 \
	:	( (enc) & MPG123_ENC_16 \
		?	2 \
		:	( (enc) & MPG123_ENC_24 \
			?	3 \
			:	( ((enc) & MPG123_ENC_32 || (enc) == MPG123_ENC_FLOAT_32) \
				?	4 \
				:	( (enc) == MPG123_ENC_FLOAT_64 \
					?	8 \
					:	0 \
)	)	)	)	)

/** Start playback with a certain output format
 *  It might be a good idea to have audio data handy to feed after this
 *  returns with success.
 * \param encoding sample encoding (values matching libmpg123 API)
 * \param channels number of channels (1 or 2, usually)
 * \param rate sampling rate
 * \return 0 on success, negative on error (bad format, usually)
 */
int out123_start( audio_output_t *ao
,                 int encoding, int channels, long rate );

/** Pause playback
 *  Interrupt playback without waiting for the data pushed to the optional
 *  buffer being sent to the audio device.
 *
 *  It keeps the audio device openend, able to resume. Occuring
 *  underruns at the device layer should be silently dealt by the driver.
 *  The nice way to stop feeding the device is to call out123_stop() and
 *  then later out123_start() again.
 *
 *  But feel free to try out123_pause() and out123_continue() instead.
 *  At least announcing the pause is nicer than just stopping to feed audio
 *  data for some time, although it might have the same effect, again,
 *  deopending on the selected driver.
 */
void out123_pause(audio_output_t *ao);

/** Continue playback
 *  The counterpart to out123_pause(). Announce to the driver that playback
 *  shall continue.
 *
 *  Playback might not resume immediately if the optional buffer is configured
 *  To wait for a minimum fill. You can force playback of the last scrap with
 *  out123_drain(), or just by feeding more data with out123_play(), which
 *  will trigger out123_continue(), too.
 */
void out123_continue(audio_output_t *ao);

/** Stop playback.
 *  This waits for pending audio data to drain to the speakers.
 *  You might want to call out123_drop() before stopping if you want
 *  to end things right away.
 */
void out123_stop(audio_output_t *ao);

/** Hand over data for playback and wait in case audio device is busy.
 *  This survives non-fatal signals like SIGSTOP/SIGCONT and keeps on
 *  playing until the buffer is done with if the flag
 *  OUT123_PLAY_OVER_SIGNALS ist set (default). So, per default, if
 *  you provided a byte count divisible by the PCM frame size, it is an
 *  error when less bytes than given are played.
 *  To be sure if an error occured, check out123_errcode().
 *  Also note that it is no accident that the buffer parameter is not marked
 *  as constant. Some output drivers might need to do things like swap
 *  byte order. This is done in-place instead of wasting memory on yet
 *  another copy. 
 * \param buffer pointer to raw audio data to be played
 * \param bytes number of bytes to read from the buffer
 * \return number of bytes played (might be less than given, even zero)
 */
size_t out123_play( audio_output_t *ao
                  , void *buffer, size_t bytes );

/** Drop any buffered data, making next provided data play right away.
 *  This is different from out123_pause() in that it doesn't imply
 *  an actual pause in playback. You are expected to play something,
 *  but feel free to call out123_stop() afterwards instead for a quicker
 *  exit than the implied out123_drain().
 */
void out123_drop(audio_output_t *ao);

/** Drain the output, waiting until all data went to the hardware.
  * This does not imply out123_stop(). You might continue handing in
  * new data after that (after you enforced a buffer underrun ...).
  * This might involve only the optional buffer process, or the
  * buffers on the audio driver side, too.
  */
void out123_drain(audio_output_t *ao);

/** Get an indication of how many bytes reside in the optional buffer.
 * This might get extended to tell the number of bytes queued up in the
 * audio backend, too.
 * \return number of bytes in out123 library buffer, -1 on error.
 */
long out123_buffered(audio_output_t *ao);

/* Pausing stuff relates mostly to the buffer process, but should work
   properly with direct output, too. There are drivers that don't like
   the client app just stopping to provide data. */


#if 0 /* Old stuff following. */


/* ------ Declarations from "audio.c" ------ */

audio_output_t* open_output_module( const char* name );
void close_output_module( audio_output_t* ao );
const char* audio_encoding_name(const int encoding, const int longer);

int init_output(audio_output_t **ao);
void exit_output(audio_output_t *ao, int rude);
int flush_output(audio_output_t *ao, unsigned char *bytes, size_t count);
int open_output(audio_output_t *ao);
void close_output(audio_output_t *ao );
int reset_output(audio_output_t *ao);
void output_pause(audio_output_t *ao);  /* Prepare output for inactivity. */
void output_unpause(audio_output_t *ao); /* Reactivate output (buffer process). */

int audio_enc_name2code(const char* name);
void audio_enclist(char** list); /* Make a string of encoding names. */

int audio_format_support(audio_output_t *ao, long rate, int channels);
void audio_prepare_format(audio_output_t *ao, int rate, int channels, int encoding);
const char* audio_module_name(audio_output_t *ao);
const char* audio_device_name(audio_output_t *ao);

int audio_reset(audio_output_t *ao, long rate, int channels, int format);
void audio_drain(audio_output_t *ao);
long audio_buffered_bytes(audio_output_t *ao);
void audio_start(audio_output_t *ao);
void audio_stop(audio_output_t *ao);
void audio_drop(audio_output_t *ao);
void audio_continue(audio_output_t *ao);
void audio_fixme_wake_buffer(audio_output_t *ao);

#endif

/* @} */

#endif
