#ifndef VIDEO_CAPTURE_H
#define VIDEO_CAPTURE_H

#include <d3d11.h>
#include <roapi.h>
#include <initguid.h>

DEFINE_GUID(IID_IClosable,
			0x30d5a829, 0x7fa4, 0x4026, 0x83, 0xbb, 0xd7, 0x5b, 0xae, 0x4e, 0xa9, 0x9e);
DEFINE_GUID(IID_IGraphicsCaptureSession2,
			0x2c39ae40, 0x7d2e, 0x5044, 0x80, 0x4e, 0x8b, 0x67, 0x99, 0xd4, 0xcf, 0x9e);
DEFINE_GUID(IID_IGraphicsCaptureSession3, 
			0xf2cdd966, 0x22ae, 0x5ea1, 0x95, 0x96, 0x3a, 0x28, 0x93, 0x44, 0xc3, 0xbe);
DEFINE_GUID(IID_IGraphicsCaptureItemInterop,
			0x3628e81b, 0x3cac, 0x4c60, 0xb7, 0xf4, 0x23, 0xce, 0x0e, 0x0c, 0x33, 0x56);
DEFINE_GUID(IID_IGraphicsCaptureItem,
			0x79c3f95b, 0x31f7, 0x4ec2, 0xa4, 0x64, 0x63, 0x2e, 0xf5, 0xd3, 0x07, 0x60);
DEFINE_GUID(IID_IGraphicsCaptureItemHandler,
			0xe9c610c0, 0xa68c, 0x5bd9, 0x80, 0x21, 0x85, 0x89, 0x34, 0x6e, 0xee, 0xe2);
DEFINE_GUID(IID_IDirect3D11CaptureFramePoolStatics,
			0x7784056a, 0x67aa, 0x4d53, 0xae, 0x54, 0x10, 0x88, 0xd5, 0xa8, 0xca, 0x21);
DEFINE_GUID(IID_IDirect3D11CaptureFramePoolHandler,
			0x51a947f7, 0x79cf, 0x5a3e, 0xa3, 0xa5, 0x12, 0x89, 0xcf, 0xa6, 0xdf, 0xe8);
DEFINE_GUID(IID_IDirect3DDxgiInterfaceAccess,
			0xa9b3d012, 0x3df2, 0x4ee3, 0xb8, 0xd1, 0x86, 0x95, 0xf4, 0x57, 0xd3, 0xc1);

typedef void CaptureFrameCallback(ID3D11Texture2D *texture, RECT rect, UINT64 time);

typedef struct IClosable							IClosable;
typedef struct IGraphicsCaptureSession2				IGraphicsCaptureSession2;
typedef struct IGraphicsCaptureSession3				IGraphicsCaptureSession3;
typedef struct IGraphicsCaptureItemInterop			IGraphicsCaptureItemInterop;
typedef struct IDirect3D11CaptureFramePoolStatics	IDirect3D11CaptureFramePoolStatics;
typedef struct IDirect3DDevice						IDirect3DDevice;
typedef struct IGraphicsCaptureItem					IGraphicsCaptureItem;
typedef struct IDirect3D11CaptureFrame				IDirect3D11CaptureFrame;
typedef struct IDirect3D11CaptureFramePool			IDirect3D11CaptureFramePool;
typedef struct IGraphicsCaptureSession				IGraphicsCaptureSession;
typedef struct IDirect3DSurface						IDirect3DSurface;
typedef struct IDirect3DDxgiInterfaceAccess			IDirect3DDxgiInterfaceAccess;
typedef struct ITypedEventHandler { struct ITypedEventHandlerVtbl *vtbl; } ITypedEventHandler;

typedef IInspectable IDispatcherQueueController;

#define VTBL(name) struct name { struct name ## Vtbl *vtbl; }
VTBL(IClosable);
VTBL(IGraphicsCaptureSession);
VTBL(IGraphicsCaptureSession2);
VTBL(IGraphicsCaptureSession3);
VTBL(IGraphicsCaptureItemInterop);
VTBL(IGraphicsCaptureItem);
VTBL(IDirect3D11CaptureFramePoolStatics);
VTBL(IDirect3D11CaptureFramePool);
VTBL(IDirect3D11CaptureFrame);
VTBL(IDirect3DDevice);
VTBL(IDirect3DSurface);
VTBL(IDirect3DDxgiInterfaceAccess);
#undef VTBL

static HRESULT STDMETHODCALLTYPE CaptureQueryInterface(ITypedEventHandler *this, const GUID *riid,
													   void **object);
static ULONG STDMETHODCALLTYPE CaptureAddRef(ITypedEventHandler *this);
static ULONG STDMETHODCALLTYPE CaptureRelease(ITypedEventHandler *this);
static HRESULT STDMETHODCALLTYPE CaptureOnFrame(ITypedEventHandler *this, IInspectable *sender,
												IInspectable *args);

#define IUnknownParent(_type) \
	HRESULT(STDMETHODCALLTYPE *QueryInterface)(_type *self, const GUID *riid, void **object);	\
	ULONG(STDMETHODCALLTYPE *AddRef)(_type *self);												\
	ULONG(STDMETHODCALLTYPE *Release)(_type *self)

#define IInspectableParent(_type)				\
	IUnknownParent(_type);						\
	void *getIids;								\
	void *getRuntimeClassName;					\
	void *getTrustLevel

struct ITypedEventHandlerVtbl {
	IUnknownParent(ITypedEventHandler);
	HRESULT(STDMETHODCALLTYPE *invoke)(ITypedEventHandler *this, IInspectable *sender,
									   IInspectable *args);
};

static struct ITypedEventHandlerVtbl CaptureFrameHandlerVtbl = {
	&CaptureQueryInterface,
	&CaptureAddRef,
	&CaptureRelease,
	&CaptureOnFrame
};

struct IClosableVtbl {
	IInspectableParent(IClosable);
	HRESULT(STDMETHODCALLTYPE *Close)(IClosable *this);
};

struct IGraphicsCaptureSessionVtbl {
	IInspectableParent(IGraphicsCaptureSession);
	HRESULT(STDMETHODCALLTYPE *StartCapture)(IGraphicsCaptureSession *this);
};

struct IGraphicsCaptureSession2Vtbl {
	IInspectableParent(IGraphicsCaptureSession2);
	HRESULT(STDMETHODCALLTYPE *get_IsCursorCaptureEnabled)(IGraphicsCaptureSession2 *this, char *value);
	HRESULT(STDMETHODCALLTYPE *put_IsCursorCaptureEnabled)(IGraphicsCaptureSession2 *this, char value);
};

struct IGraphicsCaptureSession3Vtbl {
	IInspectableParent(IGraphicsCaptureSession3);
	HRESULT(STDMETHODCALLTYPE *get_IsBorderRequired)(IGraphicsCaptureSession3 *this, char *value);
	HRESULT(STDMETHODCALLTYPE *put_IsBorderRequired)(IGraphicsCaptureSession3 *this, char value);
};

struct IGraphicsCaptureItemInteropVtbl {
	IUnknownParent(IGraphicsCaptureItemInterop);
	HRESULT(STDMETHODCALLTYPE *CreateForWindow)(IGraphicsCaptureItemInterop *this, HWND window,
												const GUID *riid, void **result);
	HRESULT(STDMETHODCALLTYPE *CreateForMonitor)(IGraphicsCaptureItemInterop *this, HMONITOR monitor,
												 const GUID *riid, void **result);
};

struct IGraphicsCaptureItemVtbl {
	IInspectableParent(IGraphicsCaptureItem);
	
	void *getDisplayName;
	HRESULT(STDMETHODCALLTYPE *GetSize)(IGraphicsCaptureItem *this, SIZE *size);
	HRESULT(STDMETHODCALLTYPE *AddClosed)(IGraphicsCaptureItem *this, ITypedEventHandler *handler,
										  UINT64 *token);
	HRESULT(STDMETHODCALLTYPE *RemoveClosed)(IGraphicsCaptureItem *this, UINT64 token);
};

struct IDirect3D11CaptureFramePoolStaticsVtbl {
	IInspectableParent(IDirect3D11CaptureFramePoolStatics);
	HRESULT(STDMETHODCALLTYPE *Create)(IDirect3D11CaptureFramePoolStatics *this,
									   IDirect3DDevice *device, DXGI_FORMAT pixelFormat,
									   INT32 numberOfBuffers, SIZE size,
									   IDirect3D11CaptureFramePool **result);
};

struct IDirect3D11CaptureFramePoolVtbl {
	IInspectableParent(IDirect3D11CaptureFramePool);
	HRESULT(STDMETHODCALLTYPE *Recreate)(IDirect3D11CaptureFramePool *this, IDirect3DDevice *device,
										 DXGI_FORMAT pixelFormat, INT32 numberOfBuffers, SIZE size);
	HRESULT(STDMETHODCALLTYPE *TryGetNextFrame)(IDirect3D11CaptureFramePool *this,
												IDirect3D11CaptureFrame **result);
	HRESULT(STDMETHODCALLTYPE *AddFrameArrived)(IDirect3D11CaptureFramePool *this,
												 ITypedEventHandler *handler, UINT64 *token);
	HRESULT(STDMETHODCALLTYPE *RemoveFrameArrived)(IDirect3D11CaptureFramePool *this, UINT64 token);
	HRESULT(STDMETHODCALLTYPE *CreateCaptureSession)(IDirect3D11CaptureFramePool *this,
													 IGraphicsCaptureItem *item,
													 IGraphicsCaptureSession **result);
};

struct IDirect3D11CaptureFrameVtbl {
	IInspectableParent(IDirect3D11CaptureFrame);
	HRESULT(STDMETHODCALLTYPE *GetSurface)(IDirect3D11CaptureFrame *this, IDirect3DSurface **value);
	HRESULT(STDMETHODCALLTYPE *GetSystemRelativeTime)(IDirect3D11CaptureFrame* this, UINT64 *value);
	HRESULT(STDMETHODCALLTYPE *GetContentSize)(IDirect3D11CaptureFrame *this, SIZE *size);
};

struct IDirect3DDeviceVtbl {
	IInspectableParent(IDirect3DDevice);
	void *trim;
};

struct IDirect3DSurfaceVtbl {
	IInspectableParent(IDirect3DSurface);
	void *get_Description;
};

struct IDirect3DDxgiInterfaceAccessVtbl {
	IUnknownParent(IDirect3DDxgiInterfaceAccess);
	HRESULT(STDMETHODCALLTYPE *GetInterface)(IDirect3DDxgiInterfaceAccess *this, const GUID *riid,
											 void **object);
};

typedef struct {
	DWORD flags;
	DWORD length;
	DWORD padding1;
	DWORD padding2;
	LPCWCHAR ptr;
} static_hstring;

#define STATIC_HSTRING(name, str) \
	static HSTRING name = (HSTRING) &(static_hstring){1, sizeof(str) - 1, 0, 0, L## str}
STATIC_HSTRING(GraphicsCaptureItemName,			"Windows.Graphics.Capture.GraphicsCaptureItem"		);
STATIC_HSTRING(Direct3D11CaptureFramePoolName,	"Windows.Graphics.Capture.Direct3D11CaptureFramePool");
#undef STATIC_HSTRING

typedef enum {
	DQTAT_COM_NONE = 0,
	DQTAT_COM_ASTA = 1,
	DQTAT_COM_STA = 2
} dispatcher_queue_apartment_type;

typedef enum {
	DQTYPE_THREAD_DEDICATED = 1,
	DQTYPE_THREAD_CURRENT = 2
} dispatcher_queue_thread_type;

typedef struct {
	DWORD dwSize;
	dispatcher_queue_apartment_type threadType;
	dispatcher_queue_thread_type apartmentType;
} dispatcher_queue_options;

extern __declspec(dllimport) \
HRESULT WINAPI CreateDispatcherQueueController(dispatcher_queue_options, IDispatcherQueueController **);
extern __declspec(dllimport) \
HRESULT WINAPI CreateDirect3D11DeviceFromDXGIDevice(IDXGIDevice *, IInspectable **);

typedef struct {
	IGraphicsCaptureItemInterop *itemInterop;
	IDirect3D11CaptureFramePoolStatics *framePoolStatics;
	IDirect3DDevice *device;
	IGraphicsCaptureItem *item;
	IDirect3D11CaptureFramePool *framePool;
	IGraphicsCaptureSession *session;
	ITypedEventHandler onFrameHandler;
	u64 onCloseToken, onFrameToken;
	RECT rect;
	SIZE currentSize;
	bool onlyClientArea;
	HWND hWindow;
	CaptureFrameCallback *FrameCallback;
} video_capture;

static void CaptureInit(video_capture *vc, CaptureFrameCallback *FrameCallback);

static bool CaptureCreateForMonitor(video_capture *vc, ID3D11Device *device, HMONITOR monitor,
									RECT *rect);
static void CaptureStart(video_capture *vc, bool withMouseCursor, bool withBorder);
static void CaptureStop(video_capture *vc);

#endif //VIDEO_CAPTURE_H
