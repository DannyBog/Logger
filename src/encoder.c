#include "encoder.h"

static HRESULT STDMETHODCALLTYPE EncoderQueryInterface(IMFAsyncCallback *this, REFIID riid,
													   void **object) {
	if (!object) return E_POINTER;
	
	if (IsEqualGUID(&IID_IUnknown, riid) || IsEqualGUID(&IID_IMFAsyncCallback, riid)) {
		*object = this;
		return S_OK;
	}
	
	return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE EncoderAddRef(IMFAsyncCallback *this) {
	return 1;
}

static ULONG STDMETHODCALLTYPE EncoderRelease(IMFAsyncCallback *this) {
	return 1;
}

static HRESULT STDMETHODCALLTYPE EncoderGetParameters(IMFAsyncCallback *this, DWORD *flags,
													  DWORD* queue) {
	*flags = 0;
	*queue = MFASYNC_CALLBACK_QUEUE_MULTITHREADED;
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE EncoderVideoInvoke(IMFAsyncCallback *this, IMFAsyncResult *result) {
	IUnknown *object;
	IMFSample *sample;

	IMFAsyncResult_GetObject(result, &object);
	IUnknown_QueryInterface(object, &IID_IMFSample, (void *) &sample);
	IUnknown_Release(object);
	// keep Sample object reference count incremented to reuse for new frame submission

	encoder *e = CONTAINING_RECORD(this, encoder, videoSampleCallback);
	InterlockedIncrement(&e->videoCount);

	return S_OK;
}

static HRESULT STDMETHODCALLTYPE EncoderAudioInvoke(IMFAsyncCallback *this, IMFAsyncResult *result) {
	IUnknown *object;
	IMFSample *sample;

	IMFAsyncResult_GetObject(result, &object);
	IUnknown_QueryInterface(object, &IID_IMFSample, (void *) &sample);
	IUnknown_Release(object);
	// keep Sample object reference count incremented to reuse for new sample submission

	encoder *e = CONTAINING_RECORD(this, encoder, audioSampleCallback);
	InterlockedIncrement(&e->audioCount);
	WakeByAddressSingle(&e->audioCount);

	return S_OK;
}

static void EncoderInit(encoder *e) {
	MFStartup(MF_VERSION, MFSTARTUP_LITE);
	e->videoSampleCallback.lpVtbl = &EncoderVideoSampleCallbackVtbl;
	e->audioSampleCallback.lpVtbl = &EncoderAudioSampleCallbackVtbl;
}

#pragma warning(push)
#pragma warning(disable:4456)
static bool EncoderStart(encoder *e, ID3D11Device *device, wchar_t *fileName, encoder_config *config) {
	UINT token;
	IMFDXGIDeviceManager *manager;
	MFCreateDXGIDeviceManager(&token, &manager);
	IMFDXGIDeviceManager_ResetDevice(manager, (IUnknown *) device, token);
	
	ID3D11DeviceContext *context;
	ID3D11Device_GetImmediateContext(device, &context);
	
	ID3D11ComputeShader *resizeShader;
	ID3D11ComputeShader *convertShader;
	ID3D11Device_CreateComputeShader(device, ResizeShaderBytes, sizeof(ResizeShaderBytes), 0,
									 &resizeShader);
	ID3D11Device_CreateComputeShader(device, ConvertShaderBytes, sizeof(ConvertShaderBytes), 0,
									 &convertShader);
	
	// must be multiple of 2, round upwards
	DWORD width = (config->width + 1) & ~1;
	DWORD height = (config->height + 1) & ~1;
	
	BOOL result = FALSE;
	IMFSinkWriter *writer = 0;
	IMFTransform *resampler = 0;
	HRESULT hr;
	
	e->videoStreamIndex = -1;
	e->audioStreamIndex = -1;
	
	const GUID *container, *codec, *mediaFormatYUV;
	UINT32 profile;
	DXGI_FORMAT formatYUV, formatY, formatUV;
	
	mediaFormatYUV = &MFVideoFormat_NV12;
	formatYUV = DXGI_FORMAT_NV12;
	formatY = DXGI_FORMAT_R8_UINT;
	formatUV = DXGI_FORMAT_R8G8_UINT;
	
	container = &MFTranscodeContainerType_MPEG4;
	codec = &MFVideoFormat_H264;
	profile = eAVEncH264VProfile_High;
	
	// output file
	{
		IMFAttributes *attributes;
		MFCreateAttributes(&attributes, 4);
		IMFAttributes_SetUINT32(attributes, &MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, false);
		IMFAttributes_SetUnknown(attributes, &MF_SINK_WRITER_D3D_MANAGER, (IUnknown *) manager);
		IMFAttributes_SetUINT32(attributes, &MF_SINK_WRITER_DISABLE_THROTTLING, true);
		IMFAttributes_SetGUID(attributes, &MF_TRANSCODE_CONTAINERTYPE, container);

		hr = MFCreateSinkWriterFromURL(fileName, 0, attributes, &writer);
		IMFAttributes_Release(attributes);

		if (hr != S_OK) {
			MessageBoxW(0, L"Cannot create output mp4 file!", L"Error", MB_ICONERROR);
			goto bail;
		}
	}

	// video output type
	{
		IMFMediaType *type;
		MFCreateMediaType(&type);

		IMFMediaType_SetGUID(type, &MF_MT_MAJOR_TYPE, &MFMediaType_Video);
		IMFMediaType_SetGUID(type, &MF_MT_SUBTYPE, codec);
		IMFMediaType_SetUINT32(type, &MF_MT_MPEG2_PROFILE, profile);
		IMFMediaType_SetUINT32(type, &MF_MT_VIDEO_PRIMARIES, MFVideoPrimaries_BT709);
		IMFMediaType_SetUINT32(type, &MF_MT_YUV_MATRIX, MFVideoTransferMatrix_BT709);
		IMFMediaType_SetUINT32(type, &MF_MT_TRANSFER_FUNCTION, MFVideoTransFunc_709);
		IMFMediaType_SetUINT32(type, &MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
		IMFMediaType_SetUINT64(type, &MF_MT_FRAME_RATE,
							   MFT64(config->framerateNum, config->framerateDen));
		IMFMediaType_SetUINT64(type, &MF_MT_FRAME_SIZE, MFT64(width, height));
		IMFMediaType_SetUINT32(type, &MF_MT_AVG_BITRATE, AUDIO_BITRATE * 1000);

		hr = IMFSinkWriter_AddStream(writer, type, (DWORD *) &e->videoStreamIndex);
		IMFMediaType_Release(type);

		if (hr != S_OK) {
			MessageBoxW(0, L"Cannot configure video encoder!", L"Error", MB_ICONERROR);
			goto bail;
		}
	}

	// video input type, NV12 or P010 format
	{
		IMFMediaType *type;
		MFCreateMediaType(&type);
		IMFMediaType_SetGUID(type, &MF_MT_MAJOR_TYPE, &MFMediaType_Video);
		IMFMediaType_SetGUID(type, &MF_MT_SUBTYPE, mediaFormatYUV);
		IMFMediaType_SetUINT32(type, &MF_MT_VIDEO_PRIMARIES, MFVideoPrimaries_BT709);
		IMFMediaType_SetUINT32(type, &MF_MT_YUV_MATRIX, MFVideoTransferMatrix_BT709);
		IMFMediaType_SetUINT32(type, &MF_MT_TRANSFER_FUNCTION, MFVideoTransFunc_709);
		IMFMediaType_SetUINT32(type, &MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
		IMFMediaType_SetUINT64(type, &MF_MT_FRAME_RATE,
							   MFT64(config->framerateNum, config->framerateDen));
		IMFMediaType_SetUINT64(type, &MF_MT_FRAME_SIZE, MFT64(width, height));

		hr = IMFSinkWriter_SetInputMediaType(writer, e->videoStreamIndex, type, 0);
		IMFMediaType_Release(type);

		if (hr != S_OK) {
			MessageBoxW(0, L"Cannot configure video encoder input!", L"Error", MB_ICONERROR);
			goto bail;
		}
	}

	// video encoder parameters
	{
		ICodecAPI *codec;
		IMFSinkWriter_GetServiceForStream(writer, 0, &GUID_NULL, &IID_ICodecAPI, (void *) &codec);

		// VBR rate control
		VARIANT rateControl = {.vt = VT_UI4, .ulVal = eAVEncCommonRateControlMode_UnconstrainedVBR};
		ICodecAPI_SetValue(codec, &CODECAPI_AVEncCommonRateControlMode, &rateControl);

		// VBR bitrate to use, some MFT encoders override MF_MT_AVG_BITRATE setting with this one
		VARIANT bitrate = {.vt = VT_UI4, .ulVal = AUDIO_BITRATE * 1000};
		ICodecAPI_SetValue(codec, &CODECAPI_AVEncCommonMeanBitRate, &bitrate);

		// set GOP size to 4 seconds
		VARIANT gopSize = {.vt = VT_UI4, .ulVal = MUL_DIV_ROUND_UP(4, config->framerateNum,
																   config->framerateDen)};
		ICodecAPI_SetValue(codec, &CODECAPI_AVEncMPVGOPSize, &gopSize);

		// disable low latency, for higher quality & better performance
		VARIANT lowLatency = {.vt = VT_BOOL, .boolVal = VARIANT_FALSE};
		ICodecAPI_SetValue(codec, &CODECAPI_AVLowLatencyMode, &lowLatency);

		// enable 2 B-frames, for better compression
		VARIANT bFrames = {.vt = VT_UI4, .ulVal = 2};
		ICodecAPI_SetValue(codec, &CODECAPI_AVEncMPVDefaultBPictureCount, &bFrames);

		ICodecAPI_Release(codec);
	}
	
	if (config->audioFormat) {
		CoCreateInstance(&CLSID_CResamplerMediaObject, 0, CLSCTX_INPROC_SERVER, &IID_IMFTransform,
						 (void* ) &resampler);

		// audio resampler input
		{
			IMFMediaType *type;
			MFCreateMediaType(&type);
			MFInitMediaTypeFromWaveFormatEx(type, config->audioFormat,
											sizeof(*config->audioFormat) + config->audioFormat->cbSize);
			IMFTransform_SetInputType(resampler, 0, type, 0);
			IMFMediaType_Release(type);
		}

		// audio resampler output
		{
			WAVEFORMATEX format = {
				.wFormatTag = WAVE_FORMAT_PCM,
				.nChannels = AUDIO_CHANNELS,
				.nSamplesPerSec = AUDIO_SAMPLERATE,
				.wBitsPerSample = sizeof(s16) * 8
			};
			format.nBlockAlign = format.nChannels * format.wBitsPerSample / 8;
			format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;

			IMFMediaType *type;
			MFCreateMediaType(&type);
			MFInitMediaTypeFromWaveFormatEx(type, &format, sizeof(format));
			IMFTransform_SetOutputType(resampler, 0, type, 0);
			IMFMediaType_Release(type);
		}

		IMFTransform_ProcessMessage(resampler, MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0);

		// audio output type
		{
			const GUID* codec = &MFAudioFormat_FLAC;

			IMFMediaType *type;
			MFCreateMediaType(&type);
			IMFMediaType_SetGUID(type, &MF_MT_MAJOR_TYPE, &MFMediaType_Audio);
			IMFMediaType_SetGUID(type, &MF_MT_SUBTYPE, codec);
			IMFMediaType_SetUINT32(type, &MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
			IMFMediaType_SetUINT32(type, &MF_MT_AUDIO_SAMPLES_PER_SECOND, AUDIO_SAMPLERATE);
			IMFMediaType_SetUINT32(type, &MF_MT_AUDIO_NUM_CHANNELS, AUDIO_CHANNELS);
			
			hr = IMFSinkWriter_AddStream(writer, type, (DWORD *) &e->audioStreamIndex);
			IMFMediaType_Release(type);

			if (hr != S_OK) {
				MessageBoxW(0, L"Cannot configure audio encoder output!", L"Error", MB_ICONERROR);
				goto bail;
			}
		}

		// audio input type
		{
			IMFMediaType *type;
			MFCreateMediaType(&type);
			IMFMediaType_SetGUID(type, &MF_MT_MAJOR_TYPE, &MFMediaType_Audio);
			IMFMediaType_SetGUID(type, &MF_MT_SUBTYPE, &MFAudioFormat_PCM);
			IMFMediaType_SetUINT32(type, &MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
			IMFMediaType_SetUINT32(type, &MF_MT_AUDIO_SAMPLES_PER_SECOND, AUDIO_SAMPLERATE);
			IMFMediaType_SetUINT32(type, &MF_MT_AUDIO_NUM_CHANNELS, AUDIO_CHANNELS);

			hr = IMFSinkWriter_SetInputMediaType(writer, e->audioStreamIndex, type, 0);
			IMFMediaType_Release(type);

			if (hr != S_OK) {
				MessageBoxW(0, L"Cannot configure audio encoder input!", L"Error", MB_ICONERROR);
				goto bail;
			}
		}
	}
	
	hr = IMFSinkWriter_BeginWriting(writer);
	
	if (hr != S_OK) {
		MessageBoxW(0, L"Cannot start writing to mp4 file!", L"Error", MB_ICONERROR);
		goto bail;
	}

	// video texture/buffers/samples
	{
		// RGB input texture
		{
			D3D11_TEXTURE2D_DESC textureDesc = {
				.Width = width,
				.Height = height,
				.MipLevels = 1,
				.ArraySize = 1,
				.Format = DXGI_FORMAT_B8G8R8A8_UNORM,
				.SampleDesc = {1, 0},
				.Usage = D3D11_USAGE_DEFAULT,
				.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE
			};
			ID3D11Device_CreateTexture2D(device, &textureDesc, 0, &e->inputTexture);
			ID3D11Device_CreateRenderTargetView(device, (ID3D11Resource *) e->inputTexture, 0,
												&e->inputRenderTarget);
			ID3D11Device_CreateShaderResourceView(device, (ID3D11Resource *) e->inputTexture, 0,
												  &e->resizeInputView);

			f32 black[] = {0, 0, 0, 0};
			ID3D11DeviceContext_ClearRenderTargetView(context, e->inputRenderTarget, black);
		}

		// RGB resized texture
		// no resizing needed, use input texture as input to converter shader directly
		ID3D11ShaderResourceView_AddRef(e->resizeInputView);
		e->convertInputView = e->resizeInputView;
		e->resizedTexture = 0;
			
		// YUV converted texture
		{
			u32 size;
			MFCalculateImageSize(mediaFormatYUV, width, height, &size);

			D3D11_TEXTURE2D_DESC textureDesc = {
				.Width = width,
				.Height = height,
				.MipLevels = 1,
				.ArraySize = 1,
				.Format = formatYUV,
				.SampleDesc = {1, 0},
				.Usage = D3D11_USAGE_DEFAULT,
				.BindFlags = D3D11_BIND_UNORDERED_ACCESS
			};

			D3D11_UNORDERED_ACCESS_VIEW_DESC viewY = {
				.Format = formatY,
				.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D
			};

			D3D11_UNORDERED_ACCESS_VIEW_DESC viewUV = {
				.Format = formatUV,
				.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D,
			};

			// create samples each referencing individual element of texture array
			for (u32 i = 0; i < ENCODER_VIDEO_BUFFER_COUNT; ++i) {
				IMFSample *videoSample;
				IMFMediaBuffer *buffer;

				ID3D11Texture2D *texture;
				ID3D11Device_CreateTexture2D(device, &textureDesc, 0, &texture);
				ID3D11Device_CreateUnorderedAccessView(device, (ID3D11Resource *) texture, &viewY,
													   &e->convertOutputViewY[i]);
				ID3D11Device_CreateUnorderedAccessView(device, (ID3D11Resource *) texture, &viewUV,
													   &e->convertOutputViewUV[i]);
				MFCreateVideoSampleFromSurface(0, &videoSample);
				MFCreateDXGISurfaceBuffer(&IID_ID3D11Texture2D, (IUnknown *) texture, 0, false,
										  &buffer);
				IMFMediaBuffer_SetCurrentLength(buffer, size);
				IMFSample_AddBuffer(videoSample, buffer);
				IMFMediaBuffer_Release(buffer);
				
				e->convertTexture[i] = texture;
				e->videoSample[i] = videoSample;
			}
		}

		e->width = width;
		e->height = height;
		e->framerateNum = config->framerateNum;
		e->framerateDen = config->framerateDen;
		e->videoDiscontinuity = false;
		e->videoLastTime = 0x8000000000000000ULL; // some large time in future
		e->videoIndex = 0;
		e->videoCount = ENCODER_VIDEO_BUFFER_COUNT;
	}

	if (e->audioStreamIndex >= 0) {
		// resampler input buffer/sample
		{
			IMFSample *sample;
			IMFMediaBuffer *buffer;

			MFCreateSample(&sample);
			MFCreateMemoryBuffer(config->audioFormat->nAvgBytesPerSec, &buffer);
			IMFSample_AddBuffer(sample, buffer);

			e->audioInputSample = sample;
			e->audioInputBuffer = buffer;
		}

		// resampler output & audio encoding input buffer/samples
		for (u32 i = 0; i < ENCODER_AUDIO_BUFFER_COUNT; ++i) {
			IMFSample *sample;
			IMFMediaBuffer *buffer;
			IMFTrackedSample *tracked;

			MFCreateTrackedSample(&tracked);
			IMFTrackedSample_QueryInterface(tracked, &IID_IMFSample, (void *) &sample);
			MFCreateMemoryBuffer(AUDIO_SAMPLERATE * AUDIO_CHANNELS * sizeof(s16), &buffer);
			IMFSample_AddBuffer(sample, buffer);
			IMFMediaBuffer_Release(buffer);
			IMFTrackedSample_Release(tracked);

			e->audioSample[i] = sample;
		}

		e->audioFrameSize = config->audioFormat->nBlockAlign;
		e->audioSampleRate = config->audioFormat->nSamplesPerSec;
		e->resampler = resampler;
		e->audioIndex = 0;
		e->audioCount = ENCODER_AUDIO_BUFFER_COUNT;
	}

	// constant buffer for RGB to YUV conversion
	{
		f32 rangeY, offsetY;
		f32 rangeUV, offsetUV;

		if (formatYUV == DXGI_FORMAT_NV12) {
			// Y=[16..235], UV=[16..240]
			rangeY = 219.f;
			offsetY = 16.5f;
			rangeUV = 224.f;
			offsetUV = 128.5f;
		} else { // FormatYUV == DXGI_FORMAT_P010
			// Y=[64..940], UV=[64..960]
			// mutiplied by 64, because 10-bit values are positioned at top of 16-bit used for texture
			// storage format
			rangeY = 876.f * 64.f;
			offsetY = 64.5f * 64.f;
			rangeUV = 896.f * 64.f;
			offsetUV = 512.5f * 64.f;
		}

		// BT.709 - https://en.wikipedia.org/wiki/YCbCr#ITU-R_BT.709_conversion
		f32 convertMtx[3][4] =
		{
			{  0.2126f * rangeY,   0.7152f * rangeY,   0.0722f * rangeY,  offsetY  },
			{ -0.1146f * rangeUV, -0.3854f * rangeUV,  0.5f    * rangeUV, offsetUV },
			{  0.5f    * rangeUV, -0.4542f * rangeUV, -0.0458f * rangeUV, offsetUV },
		};

		D3D11_BUFFER_DESC desc = {
			.ByteWidth = sizeof(convertMtx),
			.Usage = D3D11_USAGE_IMMUTABLE,
			.BindFlags = D3D11_BIND_CONSTANT_BUFFER,
		};

		D3D11_SUBRESOURCE_DATA data = {
			.pSysMem = convertMtx,
		};

		ID3D11Device_CreateBuffer(device, &desc, &data, &e->convertBuffer);
	}
	
	ID3D11Device_AddRef(device);
	ID3D11DeviceContext_AddRef(context);
	ID3D11ComputeShader_AddRef(resizeShader);
	ID3D11ComputeShader_AddRef(convertShader);
	e->context = context;
	e->device = device;
	e->resizeShader = resizeShader;
	e->convertShader = convertShader;

	e->startTime = 0;
	e->writer = writer;
	writer = 0;
	resampler = 0;
	result = TRUE;
	
bail:
	if (resampler) IMFTransform_Release(resampler);
	
	if (writer) {
		IMFSinkWriter_Release(writer);
		DeleteFileW(fileName);
	}
	
	ID3D11ComputeShader_Release(convertShader);
	ID3D11ComputeShader_Release(resizeShader);
	ID3D11DeviceContext_Release(context);
	IMFDXGIDeviceManager_Release(manager);
	
	return result;
}
#pragma warning(pop)

static void EncoderStop(encoder *e) {
	if (e->audioStreamIndex >= 0) {
		IMFTransform_ProcessMessage(e->resampler, MFT_MESSAGE_COMMAND_DRAIN, 0);
		EncoderOutputAudioSamples(e);
		IMFTransform_Release(e->resampler);
	}
	
	IMFSinkWriter_Finalize(e->writer);
	IMFSinkWriter_Release(e->writer);
	
	if (e->audioStreamIndex >= 0) {
		for (int i = 0; i < ENCODER_AUDIO_BUFFER_COUNT; ++i) {
			IMFSample_Release(e->audioSample[i]);
		}
		
		IMFSample_Release(e->audioInputSample);
		IMFMediaBuffer_Release(e->audioInputBuffer);
	}
	
	for (int i = 0; i < ENCODER_VIDEO_BUFFER_COUNT; ++i) {
		ID3D11UnorderedAccessView_Release(e->convertOutputViewY[i]);
		ID3D11UnorderedAccessView_Release(e->convertOutputViewUV[i]);
		ID3D11Texture2D_Release(e->convertTexture[i]);
		IMFSample_Release(e->videoSample[i]);
	}
	
	ID3D11ShaderResourceView_Release(e->convertInputView);
	if (e->resizedTexture) {
		ID3D11UnorderedAccessView_Release(e->resizeOutputView);
		ID3D11Texture2D_Release(e->resizedTexture);
		e->resizedTexture = 0;
	}

	ID3D11RenderTargetView_Release(e->inputRenderTarget);
	ID3D11ShaderResourceView_Release(e->resizeInputView);
	ID3D11Texture2D_Release(e->inputTexture);

	ID3D11Buffer_Release(e->convertBuffer);
	ID3D11ComputeShader_Release(e->resizeShader);
	ID3D11ComputeShader_Release(e->convertShader);
	ID3D11DeviceContext_Release(e->context);
	ID3D11Device_Release(e->device);
}

static void EncoderOutputAudioSamples(encoder *e) {
	for (;;) {
		// we don't want to drop any audio frames, so wait for available sample/buffer
		LONG count = e->audioCount;
		while (!count) {
			LONG zero = 0;
			WaitOnAddress(&e->audioCount, &zero, sizeof(LONG), INFINITE);
			count = e->audioCount;
		}

		DWORD index = e->audioIndex;
		IMFSample* sample = e->audioSample[index];

		DWORD status;
		MFT_OUTPUT_DATA_BUFFER output = {.dwStreamID = 0, .pSample = sample};
		HRESULT hr = IMFTransform_ProcessOutput(e->resampler, 0, 1, &output, &status);
		if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT) break;
		
		e->audioIndex = (index + 1) % ENCODER_AUDIO_BUFFER_COUNT;
		InterlockedDecrement(&e->audioCount);

		IMFTrackedSample *tracked;
		IMFSample_QueryInterface(sample, &IID_IMFTrackedSample, (void *) &tracked);
		IMFTrackedSample_SetAllocator(tracked, &e->audioSampleCallback, (IUnknown *) tracked);

		IMFSinkWriter_WriteSample(e->writer, e->audioStreamIndex, sample);

		IMFSample_Release(sample);
		IMFTrackedSample_Release(tracked);
	}
}

static bool EncoderNewFrame(encoder *e, ID3D11Texture2D *texture, RECT rect, u64 time, u64 timePeriod) {
	e->videoLastTime = time;

	if (!e->videoCount) {
		// dropped frame
		LONGLONG timestamp = MFllMulDiv(time - e->startTime, MF_UNITS_PER_SECOND, timePeriod, 0);
		IMFSinkWriter_SendStreamTick(e->writer, e->videoStreamIndex, timestamp);
		e->videoDiscontinuity = true;
		
		return false;
	}
	
	DWORD index = e->videoIndex;
	e->videoIndex = (index + 1) % ENCODER_VIDEO_BUFFER_COUNT;
	InterlockedDecrement(&e->videoCount);

	IMFSample *sample = e->videoSample[index];
	ID3D11DeviceContext *context = e->context;

	// copy to input texture
	{
		D3D11_BOX box = {
			.left = rect.left,
			.top = rect.top,
			.right = rect.right,
			.bottom = rect.bottom,
			.front = 0,
			.back = 1
		};

		ID3D11DeviceContext_CopySubresourceRegion(context, (ID3D11Resource *) e->inputTexture,
												  0, 0, 0, 0, (ID3D11Resource *) texture, 0, &box);
	}

	// convert to YUV
	{
		ID3D11DeviceContext_ClearState(context);
		// input
		ID3D11DeviceContext_CSSetConstantBuffers(context, 0, 1, &e->convertBuffer);
		ID3D11DeviceContext_CSSetShaderResources(context, 0, 1, &e->convertInputView);
		// output
		ID3D11UnorderedAccessView *views[] = {
			e->convertOutputViewY[index],
			e->convertOutputViewUV[index]
		};
		ID3D11DeviceContext_CSSetUnorderedAccessViews(context, 0, _countof(views), views, 0);
		// shader
		ID3D11DeviceContext_CSSetShader(context, e->convertShader, 0, 0);
		ID3D11DeviceContext_Dispatch(context, (e->width / 2 + 15) / 16,
									 (e->height / 2 + 7) / 8, 1);
	}

	// setup input time & duration
	if (!e->startTime) e->startTime = time;
	IMFSample_SetSampleDuration(sample, MFllMulDiv(e->framerateDen, MF_UNITS_PER_SECOND,
												   e->framerateNum, 0));
	IMFSample_SetSampleTime(sample, MFllMulDiv(time - e->startTime, MF_UNITS_PER_SECOND,
											   timePeriod, 0));

	if (e->videoDiscontinuity) {
		IMFSample_SetUINT32(sample, &MFSampleExtension_Discontinuity, true);
		e->videoDiscontinuity = false;
	} else {
		// don't care about success or no, we just don't want this attribute set at all
		IMFSample_DeleteItem(sample, &MFSampleExtension_Discontinuity);
	}

	IMFTrackedSample *tracked;
	IMFSample_QueryInterface(sample, &IID_IMFTrackedSample, (void *) &tracked);
	IMFTrackedSample_SetAllocator(tracked, &e->videoSampleCallback, 0);
	IMFTrackedSample_Release(tracked);

	// submit to encoder which will happen in background
	IMFSinkWriter_WriteSample(e->writer, e->videoStreamIndex, sample);

	IMFSample_Release(sample);
	
	return true;
}

static void EncoderNewSamples(encoder *e, void *samples, DWORD videoCount, u64 time, u64 timePeriod) {
	IMFSample *audioSample = e->audioInputSample;
	IMFMediaBuffer *buffer = e->audioInputBuffer;
	
	BYTE *bufferData;
	DWORD maxLength;
	IMFMediaBuffer_Lock(buffer, &bufferData, &maxLength, 0);
	
	DWORD bufferSize = videoCount * e->audioFrameSize;
	
	if (samples) {
		CopyMemory(bufferData, samples, bufferSize);
	} else {
		ZeroMemory(bufferData, bufferSize);
	}
	
	IMFMediaBuffer_Unlock(buffer);
	IMFMediaBuffer_SetCurrentLength(buffer, bufferSize);
	
	// setup input time & duration
	IMFSample_SetSampleDuration(audioSample, MFllMulDiv(videoCount, MF_UNITS_PER_SECOND,
														e->audioSampleRate, 0));
	IMFSample_SetSampleTime(audioSample, MFllMulDiv(time - e->startTime, MF_UNITS_PER_SECOND,
													timePeriod, 0));
	
	IMFTransform_ProcessInput(e->resampler, 0, audioSample, 0);
	EncoderOutputAudioSamples(e);
}

static void EncoderUpdate(encoder *e, u64 time, u64 timePeriod) {
	if (time - e->videoLastTime >= timePeriod) {
		e->videoLastTime = time;
		LONGLONG timestamp = MFllMulDiv(time - e->startTime, MF_UNITS_PER_SECOND, timePeriod, 0);
		IMFSinkWriter_SendStreamTick(e->writer, e->videoStreamIndex, timestamp);
		e->videoDiscontinuity = true;
	}
}
