#ifndef __AudioFrameWriter__
#define __AudioFrameWriter__

#include <stdio.h>
#include <stdint.h>
#include <pthread.h>

#if HAVE_ZLIB
#include <zlib.h>
#else
#define gzFile FILE *
#define gzopen fopen
#define gzfread fread
#define gzfwrite fwrite
#define gzclose fclose
#endif

#define audio_v1_header 0x0100EDFE
#define audio_v1_footer 0x0100ADDE

struct headerAudio
{
	struct timeval ts;
	uint32_t channelCount;      /* 2-16 */
	uint32_t frameCount;        /* Number of samples per channel, in this buffer. */
	uint32_t sampleDepth;       /* 16 or 32 */
	uint32_t bufferLengthBytes; /* Size of the complete audio buffer, for all channels and framcounts. */
	uint8_t  *ptr;              /* Audio data */
	uint32_t footer;            /* See audio_v1_footer */
}__attribute__((packed));
#define headerAudioSizePre  (sizeof(struct headerAudio) - sizeof(uint32_t) - sizeof(uint8_t *))
#define headerAudioPost (sizeof(uint32_t))

#endif