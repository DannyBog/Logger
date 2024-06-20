#include "video_capture.h"

static HRESULT STDMETHODCALLTYPE CaptureQueryInterface(ITypedEventHandler *this, const GUID *riid,
													   void **object) {
	if (!object) return E_POINTER;
	
	if (IsEqualGUID(riid, &IID_IGraphicsCaptureItemHandler) ||
		IsEqualGUID(riid, &IID_IDirect3D11CaptureFramePoolHandler) ||
		IsEqualGUID(riid, &IID_IAgileObject) ||
		IsEqualGUID(riid, &IID_IUnknown)) {
		*object = this;
		return S_OK;
	}
	
	return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE CaptureAddRef(ITypedEventHandler *this) {
	return 1;
}

static ULONG STDMETHODCALLTYPE CaptureRelease(ITypedEventHandler *this) {
	return 1;
}

static HRESULT STDMETHODCALLTYPE CaptureOnClosed(ITypedEventHandler *this, IInspectable *sender,
												 IInspectable *args) {
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE CaptureOnFrame(ITypedEventHandler *this, IInspectable *sender,
												  IInspectable *args) {
	video_capture *vc = CONTAINING_RECORD(this, video_capture, onFrameHandler);
	
	IDirect3D11CaptureFrame *frame;
	if (SUCCEEDED(vc->framePool->vtbl->TryGetNextFrame(vc->framePool, &frame)) && frame != NULL) {
		SIZE size;
		u64 time;
		frame->vtbl->GetSystemRelativeTime(frame, &time);
		frame->vtbl->GetContentSize(frame, &size);
		
		IDirect3DSurface *surface;
		frame->vtbl->GetSurface(frame, &surface);
		
		IDirect3DDxgiInterfaceAccess *access;
		surface->vtbl->QueryInterface(surface, &IID_IDirect3DDxgiInterfaceAccess, (void *) &access);
		surface->vtbl->Release(surface);
		
		ID3D11Texture2D *texture;
		access->vtbl->GetInterface(access, &IID_ID3D11Texture2D, (void *) &texture);
		access->vtbl->Release(access);
		
		D3D11_TEXTURE2D_DESC desc;
		ID3D11Texture2D_GetDesc(texture, &desc);
		
		vc->FrameCallback(texture, vc->rect, time);
		ID3D11Texture2D_Release(texture);
		frame->vtbl->Release(frame);
	}
	
	return S_OK;
}

static void CaptureInit(video_capture *vc, CaptureFrameCallback *FrameCallback) {
	RoInitialize(RO_INIT_SINGLETHREADED);
	
	RoGetActivationFactory(GraphicsCaptureItemName, &IID_IGraphicsCaptureItemInterop,
						   (void *) &vc->itemInterop);

	RoGetActivationFactory(Direct3D11CaptureFramePoolName, &IID_IDirect3D11CaptureFramePoolStatics,
						   (void*) &vc->framePoolStatics);
	
	vc->onFrameHandler.vtbl = &CaptureFrameHandlerVtbl;

	// create dispatcher queue that will call callbacks on main thread as part of message loop
	dispatcher_queue_options options = {
		.dwSize = sizeof(options),
		.threadType = DQTYPE_THREAD_CURRENT,
		.apartmentType = DQTAT_COM_NONE
	};

	// don't really care about object itself, as long as it is created on main thread
	IDispatcherQueueController *controller;
	CreateDispatcherQueueController(options, &controller);
	
	vc->FrameCallback = FrameCallback;
}

static bool CaptureCreateForMonitor(video_capture *vc, ID3D11Device *device, HMONITOR monitor,
									RECT *rect) {
	IDXGIDevice *dxgiDevice;
	ID3D11Device_QueryInterface(device, &IID_IDXGIDevice, (void *) &dxgiDevice);
	CreateDirect3D11DeviceFromDXGIDevice(dxgiDevice, (IInspectable **) &vc->device);
	IDXGIDevice_Release(dxgiDevice);
	
	IGraphicsCaptureItem *item;
	if (SUCCEEDED(vc->itemInterop->vtbl->CreateForMonitor(vc->itemInterop, monitor,
														  &IID_IGraphicsCaptureItem, (void *) &item))) {
		SIZE size;
		item->vtbl->GetSize(item, &size);
		
		IDirect3D11CaptureFramePool *framePool;
		if (SUCCEEDED(vc->framePoolStatics->vtbl->Create(vc->framePoolStatics, vc->device,
														 DXGI_FORMAT_B8G8R8A8_UNORM, 2, size,
														 &framePool))) {
			vc->item = item;
			vc->framePool = framePool;
			vc->onlyClientArea = false;
			vc->hWindow = 0;
			vc->currentSize = size;
			vc->rect = rect ? *rect : (RECT) {0, 0, size.cx, size.cy};
			return true;
		}
		
		item->vtbl->Release(item);
	}

	vc->device->vtbl->Release(vc->device);
	vc->device = 0;
	
	return false;
}

static void CaptureStart(video_capture *vc, bool withMouseCursor, bool withBorder) {
	IGraphicsCaptureSession *session;
	vc->framePool->vtbl->CreateCaptureSession(vc->framePool, vc->item, &session);

	IGraphicsCaptureSession2 *session2;
	if (SUCCEEDED(session->vtbl->QueryInterface(session, &IID_IGraphicsCaptureSession2,
												(void *) &session2))) {
		session2->vtbl->put_IsCursorCaptureEnabled(session2, (char) withMouseCursor);
		session2->vtbl->Release(session2);
	}
	
	IGraphicsCaptureSession3 *session3;
	if (SUCCEEDED(session->vtbl->QueryInterface(session, &IID_IGraphicsCaptureSession3,
												(void *) &session3))) {
		session3->vtbl->put_IsBorderRequired(session3, (char) withBorder);
		session3->vtbl->Release(session3);
	}

	vc->framePool->vtbl->AddFrameArrived(vc->framePool, &vc->onFrameHandler, &vc->onFrameToken);
	session->vtbl->StartCapture(session);

	vc->session = session;
}

static void CaptureStop(video_capture *vc) {
	if (vc->session) {
		IClosable *closable;

		vc->framePool->vtbl->RemoveFrameArrived(vc->framePool, vc->onFrameToken);
		vc->item->vtbl->RemoveClosed(vc->item, vc->onCloseToken);

		vc->session->vtbl->QueryInterface(vc->session, &IID_IClosable, (void *) &closable);
		closable->vtbl->Close(closable);
		closable->vtbl->Release(closable);

		vc->framePool->vtbl->QueryInterface(vc->framePool, &IID_IClosable, (void *) &closable);
		closable->vtbl->Close(closable);
		closable->vtbl->Release(closable);

		vc->session->vtbl->Release(vc->session);
		vc->session = 0;
	}

	if (vc->item) {
		vc->framePool->vtbl->Release(vc->framePool);
		vc->item->vtbl->Release(vc->item);
		vc->item = 0;
	}

	if (vc->device) {
		vc->device->vtbl->Release(vc->device);
		vc->device = 0;
	}
}
