#include "audio_capture.h"

DEFINE_GUID(CLSID_MMDeviceEnumerator,
			0xbcde0395, 0xe52f, 0x467c, 0x8e, 0x3d, 0xc4, 0x57, 0x92, 0x91, 0x69, 0x2e);
DEFINE_GUID(IID_IMMDeviceEnumerator,
			0xa95664d2, 0x9614, 0x4f35, 0xa7, 0x46, 0xde, 0x8d, 0xb6, 0x36, 0x17, 0xe6);
DEFINE_GUID(IID_IAudioClient,
			0x1cb9ad4c, 0xdbfa, 0x4c32, 0xb1, 0x78, 0xc2, 0xf5, 0x68, 0xa7, 0x03, 0xb2);
DEFINE_GUID(IID_IAudioCaptureClient,
			0xc8adbd64, 0xe71e, 0x48a0, 0xa4, 0xde, 0x18, 0x5c, 0x39, 0x5c, 0xd3, 0x17);
DEFINE_GUID(IID_IAudioRenderClient,
			0xf294acfc, 0x3146, 0x4483, 0xa7, 0xbf, 0xad, 0xdc, 0xa7, 0xc2, 0x60, 0xe2);

static bool AudioCaptureStart(audio_capture *ac, u64 duration_100ns) {
	bool result = false;
	
	IMMDeviceEnumerator *enumerator;
	CoCreateInstance(&CLSID_MMDeviceEnumerator, 0, CLSCTX_ALL, &IID_IMMDeviceEnumerator,
					 (void *) &enumerator);
	
	IMMDevice *device;
	if (IMMDeviceEnumerator_GetDefaultAudioEndpoint(enumerator, eRender, eConsole, &device) == S_OK) {
		// setup playback for slience, otherwise loopback recording does not provide any data if
		// nothing is playing
		{
			REFERENCE_TIME length = 10 * 1000 * 1000; // 1 second in 100 nsec units
			
			IAudioClient *client;
			IMMDevice_Activate(device, &IID_IAudioClient, CLSCTX_ALL, 0, (void *) &client);
			
			WAVEFORMATEX *format;
			IAudioClient_GetMixFormat(client, &format);
			IAudioClient_Initialize(client, AUDCLNT_SHAREMODE_SHARED, 0, length, 0, format, 0);
			
			IAudioRenderClient *render;
			IAudioClient_GetService(client, &IID_IAudioRenderClient, &render);
			
			BYTE *buffer;
			IAudioRenderClient_GetBuffer(render, format->nSamplesPerSec, &buffer);
			IAudioRenderClient_ReleaseBuffer(render, format->nSamplesPerSec, AUDCLNT_BUFFERFLAGS_SILENT);
			IAudioRenderClient_Release(render);
			CoTaskMemFree(format);
			
			IAudioClient_Start(client);
			ac->playClient = client;
		}
		
		// loopback recording
		{
			IAudioClient *client;
			IMMDevice_Activate(device, &IID_IAudioClient, CLSCTX_ALL, 0, (void *) &client);
			
			WAVEFORMATEX *format;
			IAudioClient_GetMixFormat(client, &format);
			IAudioClient_Initialize(client, AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK,
									duration_100ns, 0, format, 0);
			IAudioClient_GetService(client, &IID_IAudioCaptureClient, (void *) &ac->capture);
			IAudioClient_Start(client);
			
			LARGE_INTEGER start;
			QueryPerformanceCounter(&start);
			ac->startQpc = start.QuadPart;
			
			LARGE_INTEGER freq;
			QueryPerformanceFrequency(&freq);
			ac->freq = freq.QuadPart;
			
			ac->captureClient = client;
			ac->format = format;
			ac->startPos = 0;
			ac->useDeviceTimestamp = true;
			ac->firstTime = true;
		}
		
		result = true;
		IMMDevice_Release(device);
	}

	IMMDeviceEnumerator_Release(enumerator);
	
	return result;
}

static void AudioCaptureStop(audio_capture *ac) {
	CoTaskMemFree(ac->format);
	IAudioCaptureClient_Release(ac->capture);
	IAudioClient_Release(ac->playClient);
	IAudioClient_Release(ac->captureClient);
}

static void AudioCaptureFlush(audio_capture *ac) {
	IAudioClient_Stop(ac->playClient);
	IAudioClient_Stop(ac->captureClient);
}

static bool AudioCaptureGetData(audio_capture *ac, audio_capture_data *acd, u64 expectedTimestamp) {
	uint32_t frames;
	if (FAILED(IAudioCaptureClient_GetNextPacketSize(ac->capture, &frames)) || !frames) {
		return false;
	}
	
	BYTE *buffer;
	DWORD flags;
	uint64_t position; // in frames from start of stream
	uint64_t timestamp; // in 100nsec units, global QPC time
	if (FAILED(IAudioCaptureClient_GetBuffer(ac->capture, &buffer, &frames, &flags, &position,
											 &timestamp))) {
		return false;
	}
	
	if (ac->firstTime) {
		// first time we check if device timestamp is resonable -
		// not more than 500 msec away from expected
		if (expectedTimestamp) {
			int64_t delta = 1000 * (expectedTimestamp - timestamp);
			int64_t maxDelta = 500 * ac->freq;
			
			if (delta < -maxDelta || delta > +maxDelta) {
				ac->useDeviceTimestamp = false;
			} else if (flags & AUDCLNT_BUFFERFLAGS_TIMESTAMP_ERROR) {
				ac->useDeviceTimestamp = false;
			}
			
			ac->startPos = position;
		}
		ac->firstTime = false;
	}
	
	if (ac->useDeviceTimestamp) {
		acd->time = MFllMulDiv(timestamp, ac->freq, MF_UNITS_PER_SECOND, 0);
	} else {
		acd->time = ac->startQpc + MFllMulDiv(position - ac->startPos, ac->freq,
													ac->format->nSamplesPerSec, 0);
	}

	acd->samples = buffer;
	acd->count = frames;
	return true;
}

static void AudioCaptureReleaseData(audio_capture *ac, audio_capture_data *acd) {
	IAudioCaptureClient_ReleaseBuffer(ac->capture, (uint32_t) acd->count);
}
