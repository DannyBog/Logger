#ifndef AUDIO_CAPTURE_H
#define AUDIO_CAPTURE_H

#include <audioclient.h>
#include <mmdeviceapi.h>
#include <mfapi.h>
#include <initguid.h>

// MF works with 100nsec units
#define MF_UNITS_PER_SECOND 10000000ULL

// interface

typedef struct {
	IAudioClient *playClient, *captureClient;
	IAudioCaptureClient *capture;
	WAVEFORMATEX *format;
	u64 startQpc, startPos, freq;
	bool useDeviceTimestamp, firstTime;
} audio_capture;

typedef struct {
	void *samples;
	udm count;
	u64 time; // compatible with QPC 
} audio_capture_data;

// make sure CoInitializeEx has been called before calling Start()
static bool AudioCaptureStart(audio_capture *ac, u64 duration_100ns);
static void AudioCaptureStop(audio_capture *ac);
static void AudioCaptureFlush(audio_capture *ac);

// expectedTimestamp is used only first time GetData() is called to detect abnormal device timestamps
static bool AudioCaptureGetData(audio_capture *ac, audio_capture_data *acd, u64 expectedTimestamp);
static void AudioCaptureReleaseData(audio_capture *ac, audio_capture_data *acd);

#endif //AUDIO_CAPTURE_H
