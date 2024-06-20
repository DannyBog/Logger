#ifndef ENCODER_H
#define ENCODER_H

#include <d3d11.h>
#include <evr.h>
#include <codecapi.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wmcodecdsp.h>

#include "resize_shader.h"
#include "convert_shader.h"

#define ENCODER_VIDEO_BUFFER_COUNT 8
#define ENCODER_AUDIO_BUFFER_COUNT 16
#define MF_UNITS_PER_SECOND 10000000ULL

#define AUDIO_BITRATE 8000
#define AUDIO_CHANNELS 2
#define AUDIO_SAMPLERATE 48000

#define MFT64(high, low) (((u64) high << 32) | (low))
#define MUL_DIV_ROUND_UP(x, num, den) (((x) * (num) - 1) / (den) + 1)

static HRESULT STDMETHODCALLTYPE EncoderQueryInterface(IMFAsyncCallback *this, REFIID riid,
													   void **object);
static ULONG STDMETHODCALLTYPE EncoderAddRef(IMFAsyncCallback *this);
static ULONG STDMETHODCALLTYPE EncoderRelease(IMFAsyncCallback *this);
static HRESULT STDMETHODCALLTYPE EncoderGetParameters(IMFAsyncCallback *this, DWORD *flags,
													  DWORD *queue);
static HRESULT STDMETHODCALLTYPE EncoderVideoInvoke(IMFAsyncCallback *this, IMFAsyncResult *result);
static HRESULT STDMETHODCALLTYPE EncoderAudioInvoke(IMFAsyncCallback *this, IMFAsyncResult *Result);

static IMFAsyncCallbackVtbl EncoderVideoSampleCallbackVtbl = {
	&EncoderQueryInterface,
	&EncoderAddRef,
	&EncoderRelease,
	&EncoderGetParameters,
	&EncoderVideoInvoke
};

static IMFAsyncCallbackVtbl EncoderAudioSampleCallbackVtbl = {
	&EncoderQueryInterface,
	&EncoderAddRef,
	&EncoderRelease,
	&EncoderGetParameters,
	&EncoderAudioInvoke
};

typedef struct {
	DWORD width;  // width of video output
	DWORD height; // height of video output
	DWORD framerateNum; // video output framerate numerator
	DWORD framerateDen; // video output framerate denumerator
	u64 startTime;   // time in QPC ticks since first call of NewFrame

	IMFAsyncCallback videoSampleCallback;
	IMFAsyncCallback audioSampleCallback;
	ID3D11Device *device;
	ID3D11DeviceContext *context;
	IMFSinkWriter *writer;
	s32 videoStreamIndex;
	s32 audioStreamIndex;

	ID3D11ComputeShader *resizeShader;
	ID3D11ComputeShader *convertShader;
	ID3D11Buffer *convertBuffer;

	// RGB input texture
	ID3D11Texture2D *inputTexture;
	ID3D11RenderTargetView *inputRenderTarget;
	ID3D11ShaderResourceView *resizeInputView;

	// RGB resized texture
	ID3D11Texture2D *resizedTexture;
	ID3D11ShaderResourceView *convertInputView;
	ID3D11UnorderedAccessView *resizeOutputView;

	// NV12 converted texture
	ID3D11Texture2D				*convertTexture[ENCODER_VIDEO_BUFFER_COUNT];
	ID3D11UnorderedAccessView	*convertOutputViewY[ENCODER_VIDEO_BUFFER_COUNT];
	ID3D11UnorderedAccessView	*convertOutputViewUV[ENCODER_VIDEO_BUFFER_COUNT];
	IMFSample					*videoSample[ENCODER_VIDEO_BUFFER_COUNT];

	bool videoDiscontinuity;
	u64  videoLastTime;
	DWORD  videoIndex; // next index to use
	LONG   videoCount; // how many samples are currently available to use

	IMFTransform	*resampler;
	IMFSample		*audioSample[ENCODER_AUDIO_BUFFER_COUNT];
	IMFSample		*audioInputSample;
	IMFMediaBuffer	*audioInputBuffer;
	DWORD			audioFrameSize;
	DWORD			audioSampleRate;
	DWORD			audioIndex; // next index to use
	LONG			audioCount; // how many samples are currently available to use
	
	u64 nextEncode;
} encoder;

typedef struct {
	DWORD width, height;
	DWORD framerateNum, framerateDen;
	WAVEFORMATEX *audioFormat;
} encoder_config;

static void EncoderInit(encoder *e);
static bool EncoderStart(encoder *e, ID3D11Device *device, wchar_t *fileName, encoder_config *config);
static void EncoderStop(encoder *e);

static bool EncoderNewFrame(encoder *e, ID3D11Texture2D *texture, RECT rect, u64 time, u64 timePeriod);
static void EncoderNewSamples(encoder *e, void *samples, DWORD videoCount, u64 time, u64 timePeriod);
static void EncoderOutputAudioSamples(encoder *e);
static void EncoderUpdate(encoder *e, u64 time, u64 timePeriod);

#endif //ENCODER_H
