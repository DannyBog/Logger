#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <dxgi1_6.h>

#ifdef _DEBUG
#define Assert(Cond) do { if (!(Cond)) __debugbreak(); } while (0)
#else
#define Assert(Cond) (void)(Cond)
#endif
#define HR(hr) do { HRESULT _hr = (hr); Assert(SUCCEEDED(_hr)); } while (0)

#include "bog\bog_types.h"
#include "bog\bog_stringw.h"
#include "audio_capture.c"
#include "video_capture.c"
#include "encoder.c"

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "evr.lib")
#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "strmiids.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "windowsapp.lib")
#pragma comment(lib, "wmcodecdspuuid.lib")

#define APP_NAME		L"Logger"

#define WM_APP_CLICKED				(WM_USER + 0)
#define WM_APP_RUNNING				(WM_USER + 1)
#define CMD_KEYS					(WM_USER + 2)
#define CMD_CLIPBOARD_TEXT			(WM_USER + 3)
#define CMD_CLIPBOARD_FILES			(WM_USER + 4)
#define CMD_RECORD					(WM_USER + 5)
#define CMD_EXIT					(WM_USER + 6)
#define ID_RECORD_SHORTCUT			(WM_USER + 7)
#define ID_KEYS_SHORTCUT			(WM_USER + 8)
#define ID_CLIPBOARD_TEXT_SHORTCUT	(WM_USER + 9)
#define ID_CLIPBOARD_FILES_SHORTCUT	(WM_USER + 10)

#define LOGS_PATH		L"%APPDATA%\\Logger"
#define MAX_CLIPBOARD_SIZE 65536

#define AUDIO_CAPTURE_TIMER    1
#define AUDIO_CAPTURE_INTERVAL 100 // msec

#define VIDEO_UPDATE_TIMER     2
#define VIDEO_UPDATE_INTERVAL  100 // msec

#define AUDIO_CAPTURE_BUFFER_DURATION_100NS (10 * 1000 * 1000)

static encoder gEncoder;
static LARGE_INTEGER gTickFreq;

static void AddTrayIcon(HWND hWindow, HICON hIcon) {
	NOTIFYICONDATAW nid = {
		.cbSize = sizeof(nid),
		.hWnd = hWindow,
		.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP,
		.uCallbackMessage = WM_APP_CLICKED,
		.hIcon = hIcon
	};
	
	BOGStringCopyToIndexW(nid.szTip, APP_NAME, ARRAYSIZE(nid.szTip));
	Shell_NotifyIconW(NIM_ADD, &nid);
}

static void RemoveTrayIcon(HWND hWindow) {
	NOTIFYICONDATAW nid = {
		.cbSize = sizeof(nid),
		.hWnd = hWindow
	};
	
	Shell_NotifyIconW(NIM_DELETE, &nid);
}

static void UpdateTrayIcon(HWND hWindow, HICON hIcon) {
	NOTIFYICONDATAW nid = {
		.cbSize = sizeof(nid),
		.hWnd = hWindow,
		.uFlags = NIF_ICON,
		.hIcon = hIcon
	};
	
	Shell_NotifyIconW(NIM_MODIFY, &nid);
}

static void ShowTrayIcon(HWND hWindow, HMENU hMenu) {
	POINT pt;
	GetCursorPos(&pt);
					
	SetForegroundWindow(hWindow);
	TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_BOTTOMALIGN, pt.x, pt.y, 0, hWindow, 0);
	PostMessage(hWindow, WM_NULL, 0, 0);
}

static void ShowTrayMessage(HWND hWindow, u32 info, wchar_t *msg) {
	NOTIFYICONDATAW nid = {
		.cbSize = sizeof(nid),
		.hWnd = hWindow,
		.uFlags = NIF_INFO,
		.dwInfoFlags = info
	};
	
	BOGStringCopyToIndexW(nid.szInfo, msg, ARRAYSIZE(nid.szInfo));
	BOGStringCopyToIndexW(nid.szInfoTitle, APP_NAME, ARRAYSIZE(nid.szInfoTitle));
	Shell_NotifyIconW(NIM_MODIFY, &nid);
}

static bool EnableHotKeys(HWND hWindow) {
	bool result = true;
	
	result = result && RegisterHotKey(hWindow, ID_RECORD_SHORTCUT,
									  MOD_ALT | MOD_CONTROL | MOD_SHIFT, 'Z');
	result = result && RegisterHotKey(hWindow, ID_KEYS_SHORTCUT,
									  MOD_ALT | MOD_CONTROL | MOD_SHIFT, 'X');
	result = result && RegisterHotKey(hWindow, ID_CLIPBOARD_TEXT_SHORTCUT,
									  MOD_ALT | MOD_CONTROL | MOD_SHIFT, 'C');
	result = result && RegisterHotKey(hWindow, ID_CLIPBOARD_FILES_SHORTCUT,
									  MOD_ALT | MOD_CONTROL | MOD_SHIFT, 'V');
	
	return result;
}

wchar_t * ParseVirtualKey(wchar_t key) {
	wchar_t *result;
    
	switch (key) {
		case VK_BACK: {
			result = L"[BACKSPACE]";
		} break;
		
		case VK_TAB: {
			result = L"[TAB]";
		} break;
		
		case VK_RETURN: {
			result = L"[ENTER]\r\n";
		} break;
			
		case VK_PAUSE: {
			result = L"[PAUSE]";
		} break;
			
		case VK_CAPITAL: {
			result = L"[CAPS LOCK]";
		} break;
		
		case VK_ESCAPE: {
			result = L"[ESC]";
		} break;
			
		case VK_PRIOR: {
			result = L"[PAGE UP]";
		} break;
		
		case VK_NEXT: {
			result = L"[PAGE DOWN]";
		} break;
		
		case VK_END: {
			result = L"[END]";
		} break;
		
		case VK_HOME: {
			result = L"[HOME]";
		} break;
			
		case VK_LEFT: {
			result = L"[LEFT]";
		} break;
		
		case VK_UP: {
			result = L"[UP]";
		} break;
		
		case VK_RIGHT: {
			result = L"[RIGHT]";
		} break;
		
		case VK_DOWN: {
			result = L"[DOWN]";
		} break;
		
		case VK_SNAPSHOT: {
			result = L"[PRINT SCREEN]";
		} break;
		
		case VK_INSERT: {
			result = L"[INSERT]";
		} break;
			
		case VK_DELETE: {
			result = L"[DELETE]";
		} break;
		
		case VK_LWIN: {
			result = L"[LWIN]";
		} break;
		
		case VK_RWIN: {
			result = L"[RWIN]";
		} break;
		
		case VK_APPS: {
			result = L"[APP]";
		} break;
		
		case VK_F1: {
			result = L"[F1]";
		} break;
		
		case VK_F2: {
			result = L"[F2]";
		} break;
		
		case VK_F3: {
			result = L"[F3]";
		} break;
		
		case VK_F4: {
			result = L"[F4]";
		} break;
		
		case VK_F5: {
			result = L"[F5]";
		} break;
		
		case VK_F6: {
			result = L"[F6]";
		} break;
		
		case VK_F7: {
			result = L"[F7]";
		} break;
		
		case VK_F8: {
			result = L"[F8]";
		} break;
		
		case VK_F9: {
			result = L"[F9]";
		} break;
		
		case VK_F10: {
			result = L"[F10]";
		} break;
		
		case VK_F11: {
			result = L"[F11]";
		} break;
		
		case VK_F12: {
			result = L"[F12]";
		} break;
		
		case VK_NUMLOCK: {
			result = L"[NUM LOCK]";
		} break;
		
		case VK_SCROLL: {
			result = L"[SCROLL LOCK]";
		} break;
		
		case VK_LSHIFT: {
			result = L"[LSHIFT]";
		} break;
		
		case VK_RSHIFT: {
			result = L"[RSHIFT]";
		} break;
		
		case VK_LCONTROL: {
			result = L"[LCONTROL]";
		} break;
		
		case VK_RCONTROL: {
			result = L"[RCONTROL]";
		} break;
		
		case VK_LMENU: {
			result = L"[LALT]";
		} break;
		
		case VK_RMENU: {
			result = L"[RALT]";
		} break;
		
		default: result = 0;
	}
    
	return result;
}

static void GetTimestamp(wchar_t *time) {
	SYSTEMTIME st;
	GetLocalTime(&st);
	
	time[0] = L'[';
	time[1] = st.wDay / 10 + L'0';
	time[2] = st.wDay % 10 + L'0';
	time[3] = L'-';
	time[4] = st.wMonth / 10 + L'0';
	time[5] = st.wMonth % 10 + L'0';
	time[6] = L'-';
	time[7] = st.wYear / 1000 + L'0';
	time[8] = (st.wYear / 100) % 10 + L'0';
	time[9] = (st.wYear / 10) % 10 + L'0';
	time[10] = st.wYear % 10 + L'0';
	time[11] = L' ';
	time[12] = st.wHour / 10 + L'0';
	time[13] = st.wHour % 10 + L'0';
	time[14] = L':';
	time[15] = st.wMinute / 10 + L'0';
	time[16] = st.wMinute % 10 + L'0';
	time[17] = L':';
	time[18] = st.wSecond / 10 + L'0';
	time[19] = st.wSecond % 10 + L'0';
	time[20] = L']';
	time[21] = L'\0';
}

static void ExpandPath(wchar_t *path) {
	wchar_t result[MAX_PATH] = {0};
	
	BOGStringCopyToCharW(result, path, L'\\');
	ExpandEnvironmentStringsW(result, result, MAX_PATH);
	BOGStringCatFromCharW(result, path, L'\\');
	
	BOGStringCopyW(path, result);
}

static void AppendFile(wchar_t *fileName, wchar_t *buffer, bool newLine, bool timestamp) {
	HANDLE hFile = CreateFileW(fileName, FILE_APPEND_DATA, FILE_SHARE_WRITE, 0, OPEN_ALWAYS,
							   FILE_ATTRIBUTE_NORMAL, 0);
    
	if (hFile != INVALID_HANDLE_VALUE) {
		char text[MAX_CLIPBOARD_SIZE];
		uint32_t textSize = sizeof(text);
		DWORD bufferSize = WideCharToMultiByte(CP_UTF8, 0, buffer, -1, 0, 0, 0, 0);
		
		if (bufferSize <= textSize) {
			if (timestamp) {
				wchar_t time[23];
				GetTimestamp(time);
				DWORD timeSize = WideCharToMultiByte(CP_UTF8, 0, time, -1, 0, 0, 0, 0);
				
				time[21] = L' ';
				time[22] = L'\0';
				
				WideCharToMultiByte(CP_UTF8, 0, time, -1, text, timeSize, 0, 0);
				WideCharToMultiByte(CP_UTF8, 0, buffer, -1, text + timeSize, bufferSize, 0, 0);
				WriteFile(hFile, text, timeSize + bufferSize - 1, 0, 0);
			} else {
				s32 characters = (s32) BOGStringLengthW(buffer);
				WideCharToMultiByte(CP_UTF8, 0, buffer, characters, text, bufferSize, 0, 0);
				WriteFile(hFile, text, bufferSize - 1, 0, 0);
			}
		
			if (newLine) WriteFile(hFile, "\r\n", 2, 0, 0);
		}
	}
	
	CloseHandle(hFile);
}

static void OverrideFile(wchar_t *fileName, wchar_t *key) {
	HANDLE hFile = CreateFileW(fileName, FILE_READ_DATA | FILE_WRITE_DATA,
							   FILE_SHARE_WRITE, 0, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
	wchar_t wbuffer[256] = {0};
	char buffer[256];
	
	if (hFile != INVALID_HANDLE_VALUE) {
		s32 pos = SetFilePointer(hFile, -1, 0, FILE_END);
		u32 characters = 0;
		
		while (pos >= 0) {
			ReadFile(hFile, &wbuffer[characters], 1, 0, 0);
			if (wbuffer[characters] == L'[') break;
			pos = SetFilePointer(hFile, -2, 0, FILE_CURRENT);
			characters++;
		}
		
		BOGStringReverseToIndexW(wbuffer, ++characters);
		if (!BOGStringContainsW(wbuffer, key + 1)) {
			CloseHandle(hFile);
			AppendFile(fileName, key, false, false);
			return;
		}
		
		if (BOGIsDigitW(wbuffer[1])) {
			for (u32 i = characters; i >= 1; --i) {
				if (BOGIsDigitW(wbuffer[i])) {
					s32 digit = wbuffer[i] - L'0';
					if (digit < 9) {
						wbuffer[i]++;
					} else {
						do {
							wbuffer[i--] = L'0';
							digit = wbuffer[i] - L'0';
						} while ((i >= 1) && (digit == 9));
						
						if (BOGIsDigitW(wbuffer[i])) wbuffer[i]++;
					}
					
					break;
				}
			}
			
			if (wbuffer[1] == L'0') {
				for (u32 j = characters; j >= 1; --j) {
					wbuffer[j] = wbuffer[j - 1];
				}
					
				wbuffer[1] = L'1';
				characters++;
			}
				
			WideCharToMultiByte(CP_UTF8, 0, wbuffer, characters, buffer, characters, 0, 0);
			SetFilePointer(hFile, -1, 0, FILE_CURRENT);
			WriteFile(hFile, &buffer, characters, 0, 0);
		} else {
			BOGStringCopyW(wbuffer, L"[2x ");
			BOGStringCatFromIndexW(wbuffer + 4, key, 1);
			
			characters = (s32) BOGStringLengthW(wbuffer);
			WideCharToMultiByte(CP_UTF8, 0, wbuffer, characters, buffer, characters, 0, 0);
			SetFilePointer(hFile, -1, 0, FILE_CURRENT);
			WriteFile(hFile, buffer, characters, 0, 0);
		}
	}
	
	CloseHandle(hFile);
}

static void CopyItemRecursivelyW(wchar_t *srcPath, wchar_t *dirPath) {
	wchar_t newSrc[MAX_PATH] = {0};
	wchar_t newDir[MAX_PATH] = {0};
	
	if (*srcPath == '%') {
		ExpandPath(srcPath);
	} else {
		BOGStringCopyW(newSrc, srcPath);
	}
	
	BOGStringCopyW(newDir, dirPath);
	
	if (GetFileAttributesW(newSrc) & FILE_ATTRIBUTE_DIRECTORY) {
		BOGStringCatW(newSrc, L"\\*");
		BOGStringCatW(newDir, L"\\");
		
		WIN32_FIND_DATAW fd;
		HANDLE file = FindFirstFileW(newSrc, &fd);
		
		wchar_t *p1 = newSrc;
		wchar_t *p2 = newDir;
		while (*p1 != '*') *p1++;
		while (*p2) *p2++;
		
		CreateDirectoryW(dirPath, 0);
		
		do {
			if (*fd.cFileName == L'.') continue;
			
			BOGStringCopyW(p1, fd.cFileName);
			BOGStringCopyW(p2, fd.cFileName);
			if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
				CreateDirectoryW(newDir, 0);
				CopyItemRecursivelyW(newSrc, newDir);
				*p1 = L'\0';
				*p2 = L'\0';
			} else {
				CopyFileW(newSrc, newDir, 0);
				*p1 = L'\0';
				*p2 = L'\0';
			}
		} while (FindNextFileW(file, &fd));
		
		FindClose(file);
	} else {
		CopyFileW(newSrc, newDir, 0);
	}
}

static void CreateDirectoryRecursivelyW(wchar_t *path) {
	wchar_t expandedPath[MAX_PATH] = {0};
	BOGStringCopyW(expandedPath, path);
	
	if (*expandedPath == '%') ExpandPath(expandedPath);
	
	wchar_t directory[MAX_PATH] = {0};
	for (u32 i = 0; i < BOGStringLengthW(expandedPath); ++i) {
		directory[i] += expandedPath[i];
		if (!expandedPath[i + 1] || expandedPath[i + 1] == '\\') {
			if (GetFileAttributesW(directory) == INVALID_FILE_ATTRIBUTES) {
				CreateDirectoryW(directory, 0);
			}
		}
	}
}

HKL GetInputLanguage() {
	HKL result = 0;
	
	HKEY hKey;
	RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Input\\Locales", 0, KEY_READ, &hKey);
	
	DWORD value;
	DWORD valueSize = sizeof(value);
	RegGetValueW(hKey, 0, L"InputLocale", RRF_RT_REG_DWORD, 0, &value, &valueSize);
	
	LANGID currentLangID = value & 0xFFFF;
	
	s32 layoutsNum = GetKeyboardLayoutList(0, 0);
	HKL layouts[256];
	GetKeyboardLayoutList(layoutsNum, layouts);
	
	for (udm i = 0; i < layoutsNum; ++i) {
		LANGID langID = (HandleToUlong(layouts[i]) & 0xFFFF);
		if (langID == currentLangID) {
			result = layouts[i];
			break;
		}
	}
	
	return result;
}

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
	if (nCode == HC_ACTION) {
		PKBDLLHOOKSTRUCT p = (PKBDLLHOOKSTRUCT) lParam;
		
		if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
			wchar_t *nonPrintableKey = ParseVirtualKey((wchar_t) p->vkCode);
			wchar_t *literalPath = LOGS_PATH"\\keys.txt";
			wchar_t path[MAX_PATH] = {0};
			
			CreateDirectoryRecursivelyW(LOGS_PATH);
			
			BOGStringCopyW(path, literalPath);
			if (*path == L'%') ExpandPath(path);
			
			if (nonPrintableKey) {
				OverrideFile(path, nonPrintableKey);
			} else {
				wchar_t key[2];
				
				u8 keyboardState[256] = {0};
				for (s16 i = 0; i < 256; ++i) {
					if (i == VK_CONTROL) continue;
					s16 state = GetKeyState(i);
					keyboardState[i] = (state >> 8) | (state & 1);
				}
				
				HKL lang = GetInputLanguage();
				s32 result = ToUnicodeEx(p->vkCode, p->scanCode, keyboardState, &key[0], 1, 0, lang);
				key[1] = L'\0';
				if (result > 0) AppendFile(path, key, false, false);
			}
		}
	}
	
	return CallNextHookEx(0, nCode, wParam, lParam);
}

static void StartRecording(HWND hWindow, ID3D11Device *device, video_capture *vc, audio_capture *ac) {
	wchar_t filename[22 + 5];
	GetTimestamp(filename);
	
	for (u32 i = 0; i < 22; ++i) {
		filename[i] = filename[i + 1];
	}
	
	filename[13] = L'-';
	filename[16] = L'-';
	filename[19] = L'\0';
	BOGStringCatW(filename, L".mp4\0");
	
	wchar_t *literalPath = LOGS_PATH"\\Recordings\\";
	wchar_t path[MAX_PATH] = {0};
	
	CreateDirectoryRecursivelyW(literalPath);
	
	BOGStringCopyW(path, literalPath);
	if (*path == L'%') ExpandPath(path);
	
	CreateDirectoryW(path, 0);
	BOGStringCatW(path, filename);
	
	encoder_config ec = {
		.width = vc->rect.right - vc->rect.left,
		.height = vc->rect.bottom - vc->rect.top,
		.framerateNum = 60,
		.framerateDen = 1
	};
	
	if (!AudioCaptureStart(ac, AUDIO_CAPTURE_BUFFER_DURATION_100NS)) {
		CaptureStop(vc);
		ID3D11Device_Release(device);
		return;
	}
	
	ec.audioFormat = ac->format;
	
	if (!EncoderStart(&gEncoder, device, path, &ec)) {
		AudioCaptureStop(ac);
		CaptureStop(vc);
		ID3D11Device_Release(device);
		return;
	}

	CaptureStart(vc, true, false);
	SetTimer(hWindow, AUDIO_CAPTURE_TIMER, AUDIO_CAPTURE_INTERVAL, 0);
	SetTimer(hWindow, VIDEO_UPDATE_TIMER, VIDEO_UPDATE_INTERVAL, 0);
	ID3D11Device_Release(device);
}

static void EncodeCapturedAudio(audio_capture *ac) {
	if (!gEncoder.startTime) return;
	
	audio_capture_data data;
	while (AudioCaptureGetData(ac, &data, gEncoder.startTime)) {
		u32 framesToEncode = (u32) data.count;
		if (data.time < gEncoder.startTime) {
			u32 sampleRate = ac->format->nSamplesPerSec;
			u32 bytesPerFrame = ac->format->nBlockAlign;

			// figure out how much time (100nsec units) and frame count to skip from current buffer
			u64 timeToSkip = gEncoder.startTime - data.time;
			u32 framesToSkip = (u32) ((timeToSkip * sampleRate - 1) / MF_UNITS_PER_SECOND + 1);
			if (framesToSkip < framesToEncode) {
				// need to skip part of captured data
				data.time += framesToSkip * MF_UNITS_PER_SECOND / sampleRate;
				framesToEncode -= framesToSkip;
				if (data.samples) data.samples = (BYTE *) data.samples + framesToSkip * bytesPerFrame;
			} else {
				// need to skip all of captured data
				framesToEncode = 0;
			}
		}
		if (framesToEncode) {
			EncoderNewSamples(&gEncoder, data.samples, framesToEncode, data.time, gTickFreq.QuadPart);
		}
		
		AudioCaptureReleaseData(ac, &data);
	}
}

static void StopRecording(HWND hwnd, audio_capture *ac, video_capture *vc) {
	KillTimer(hwnd, AUDIO_CAPTURE_TIMER);
	AudioCaptureFlush(ac);
	EncodeCapturedAudio(ac);
	AudioCaptureStop(ac);
	
	KillTimer(hwnd, VIDEO_UPDATE_TIMER);

	CaptureStop(vc);
	EncoderStop(&gEncoder);
	
	SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_HIDEWINDOW | SWP_NOMOVE | SWP_NOSIZE);
	SetWindowLongW(hwnd, GWL_EXSTYLE, 0);
}

static ID3D11Device * CreateDevice() {
	IDXGIAdapter *adapter = 0;
	
	IDXGIFactory *factory;
	if (SUCCEEDED(CreateDXGIFactory(&IID_IDXGIFactory, (void **) &factory))) {
		IDXGIFactory6 *factory6;
		if (SUCCEEDED(IDXGIFactory_QueryInterface(factory, &IID_IDXGIFactory6, (void **) &factory6))) {
			DXGI_GPU_PREFERENCE preference = DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE;
			if (FAILED(IDXGIFactory6_EnumAdapterByGpuPreference(factory6, 0, preference,
																&IID_IDXGIAdapter, &adapter))) {
				// just to be safe
				adapter = 0;
			}
			IDXGIFactory6_Release(factory6);
		}
		IDXGIFactory_Release(factory);
	}
	
	ID3D11Device *device;
	
	UINT flags = 0;
	
#ifdef _DEBUG
	flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
	
	// if adapter is selected then driver type must be unknown
	D3D_DRIVER_TYPE driver = adapter ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE;
	if (FAILED(D3D11CreateDevice(adapter, driver, 0, flags,
								 (D3D_FEATURE_LEVEL[]) {D3D_FEATURE_LEVEL_11_0}, 1, D3D11_SDK_VERSION,
								 &device, 0, 0))) {
		device = NULL;
	}
	if (adapter) IDXGIAdapter_Release(adapter);
	
#ifdef _DEBUG
	ID3D11InfoQueue *info;
	ID3D11Device_QueryInterface(device, &IID_ID3D11InfoQueue, &info);
	ID3D11InfoQueue_SetBreakOnSeverity(info, D3D11_MESSAGE_SEVERITY_CORRUPTION, true);
	ID3D11InfoQueue_SetBreakOnSeverity(info, D3D11_MESSAGE_SEVERITY_ERROR, true);
	ID3D11InfoQueue_Release(info);
#endif
	
	return device;
}

static void CaptureScreen(HWND hWindow, video_capture *vc, audio_capture *ac) {
	POINT mouse;
	GetCursorPos(&mouse);
	
	HMONITOR hMonitor = MonitorFromPoint(mouse, MONITOR_DEFAULTTONULL);
	hMonitor;
	
	ID3D11Device *device = CreateDevice();
	device;
	SYSTEMTIME st;
	GetLocalTime(&st);
	CaptureCreateForMonitor(vc, device, hMonitor, 0);
	StartRecording(hWindow, device, vc, ac);
}

static void OnCaptureFrame(ID3D11Texture2D *texture, RECT rect, u64 time) {
	bool doEncode = true;
	DWORD limitFramerate = 60;
	if (time * limitFramerate < gEncoder.nextEncode) {
		doEncode = false;
	} else {
		if (!gEncoder.nextEncode) gEncoder.nextEncode = time * limitFramerate;
		gEncoder.nextEncode += gTickFreq.QuadPart;
	}
	
	if (doEncode) EncoderNewFrame(&gEncoder, texture, rect, time, gTickFreq.QuadPart);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	LRESULT result = 0;
	
	static u32 WM_TASKBARCREATED;
	static HMENU hMenu;
	static HHOOK hKeyboardHook;
	static HWND clipboardViewer;
	static bool record, logText, logFiles;
	
	static video_capture vc;
	static audio_capture ac;
	static EXECUTION_STATE recordingState;
	
	switch (msg) {
		case WM_CREATE: {
			if (!EnableHotKeys(hwnd)) {
				MessageBoxW(0,
							L"Cannot register wcap keyboard shortcuts. \n"
							"Some other application might already use shorcuts. \n"
							"Please check & adjust the settings!",
							APP_NAME, MB_ICONEXCLAMATION);
			}
			
			WM_TASKBARCREATED = RegisterWindowMessageW(L"TaskbarCreated");
			AddTrayIcon(hwnd, 0);
			
			HMENU hSubMenu = CreatePopupMenu();
			AppendMenu(hSubMenu, MF_STRING, CMD_CLIPBOARD_TEXT, "Log Text");
			AppendMenu(hSubMenu, MF_STRING, CMD_CLIPBOARD_FILES, "Log Files");
			
			hMenu = CreatePopupMenu();
			AppendMenu(hMenu, MF_STRING, CMD_RECORD, "Screen Record");
			AppendMenu(hMenu, MF_SEPARATOR, 0, 0);
			AppendMenu(hMenu, MF_STRING, CMD_KEYS, "Log Keys");
			AppendMenu(hMenu, MF_SEPARATOR, 0, 0);
			AppendMenu(hMenu, MF_STRING | MF_POPUP, (UINT_PTR) hSubMenu, "Clipboard");
			AppendMenu(hMenu, MF_SEPARATOR, 0, 0);
			AppendMenu(hMenu, MF_STRING, CMD_EXIT, "Exit");
			
			clipboardViewer = SetClipboardViewer(hwnd);
			
			QueryPerformanceFrequency(&gTickFreq);
			
			CoInitializeEx(0, COINIT_APARTMENTTHREADED);
			CaptureInit(&vc, OnCaptureFrame);
			EncoderInit(&gEncoder);
		} break;
		
		case WM_APP_CLICKED: {
			switch (LOWORD(lParam)) {
				case WM_RBUTTONDOWN:
				case WM_CONTEXTMENU: {
					ShowTrayIcon(hwnd, hMenu);
				} break;
			}
		} break;
		
		case WM_APP_RUNNING: {
			ShowTrayMessage(hwnd, NIIF_INFO, APP_NAME" is already running");
		} break;
			
		case WM_COMMAND: {
			switch (LOWORD(wParam)) {
				case CMD_RECORD: {
					u32 state = GetMenuState(hMenu, CMD_RECORD, MF_BYCOMMAND);
					switch (state) {
						case MF_CHECKED: {
							record = false;
							SetThreadExecutionState(recordingState);
							StopRecording(hwnd, &ac, &vc);
							CheckMenuItem(hMenu, CMD_RECORD, MF_UNCHECKED);
						} break;
						
						case MF_UNCHECKED: {
							CaptureScreen(hwnd, &vc, &ac);
							recordingState = SetThreadExecutionState(ES_CONTINUOUS |
																	 ES_DISPLAY_REQUIRED);
							record = true;
							CheckMenuItem(hMenu, CMD_RECORD, MF_CHECKED);
						} break;
					}
				} break;
				
				case CMD_KEYS: {
					u32 state = GetMenuState(hMenu, CMD_KEYS, MF_BYCOMMAND);
					switch (state) {
						case MF_CHECKED: {
							UnhookWindowsHookEx(hKeyboardHook);
							CheckMenuItem(hMenu, CMD_KEYS, MF_UNCHECKED);
						} break;
						
						case MF_UNCHECKED: {
							hKeyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc,
															 0, 0);
							CheckMenuItem(hMenu, CMD_KEYS, MF_CHECKED);
						} break;
					}
				} break;
				
				case CMD_CLIPBOARD_TEXT: {
					u32 state = GetMenuState(hMenu, CMD_CLIPBOARD_TEXT, MF_BYCOMMAND);
					switch (state) {
						case MF_CHECKED: {
							logText = false;
							CheckMenuItem(hMenu, CMD_CLIPBOARD_TEXT, MF_UNCHECKED);
						} break;
						
						case MF_UNCHECKED: {
							logText = true;
							CheckMenuItem(hMenu, CMD_CLIPBOARD_TEXT, MF_CHECKED);
						} break;
					}
				} break;
				
				case CMD_CLIPBOARD_FILES: {
					u32 state = GetMenuState(hMenu, CMD_CLIPBOARD_FILES, MF_BYCOMMAND);
					switch (state) {
						case MF_CHECKED: {
							logFiles = false;
							CheckMenuItem(hMenu, CMD_CLIPBOARD_FILES, MF_UNCHECKED);
						} break;
						
						case MF_UNCHECKED: {
							logFiles = true;
							CheckMenuItem(hMenu, CMD_CLIPBOARD_FILES, MF_CHECKED);
						} break;
					}
				} break;
				
				case CMD_EXIT: {
					DestroyWindow(hwnd);
				} break;
			}
		} break;
		
		case WM_DRAWCLIPBOARD: {
			if (OpenClipboard(0)) {
				u32 format = EnumClipboardFormats(0);
				while (format) {
					switch (format) {
						case CF_UNICODETEXT: {
							if (logText) {
								HANDLE hData = GetClipboardData(CF_UNICODETEXT);
								wchar_t *data = (wchar_t *) GlobalLock(hData);
								wchar_t *literalPath = LOGS_PATH"\\clipboard.txt";
								wchar_t path[MAX_PATH] = {0};
								
								CreateDirectoryRecursivelyW(LOGS_PATH);
								
								BOGStringCopyW(path, literalPath);
								if (*path == L'%') ExpandPath(path);
								
								AppendFile(path, data, true, true);
								GlobalUnlock(hData);
							}
						} break;
						
						case CF_HDROP: {
							if (logFiles) {
								HANDLE hData = GetClipboardData(CF_HDROP);
								u32 files = DragQueryFileW((HDROP) hData, 0xFFFFFFFF, 0, 0);
								wchar_t *literalPath = LOGS_PATH"\\Clipboard\\";
								wchar_t path[MAX_PATH] = {0};
								
								CreateDirectoryRecursivelyW(literalPath);
								
								BOGStringCopyW(path, literalPath);
								if (*path == L'%') ExpandPath(path);
								
								for (u32 i = 0; i < files; ++i) {
									wchar_t srcFilePath[MAX_PATH] = {0};
									wchar_t dstFilePath[MAX_PATH] = {0};
									
									BOGStringCopyW(dstFilePath, path);
									
									DragQueryFileW((HDROP) hData, i, srcFilePath, MAX_PATH);
									wchar_t *fileName = PathFindFileNameW(srcFilePath);
									
									BOGStringCatW(dstFilePath, fileName);
									CopyItemRecursivelyW(srcFilePath, dstFilePath);
								}
							}
						} break;
					}
					
					format = EnumClipboardFormats(format);
				}
				
				CloseClipboard();
			}
		} break;
			
		case WM_HOTKEY: {
			switch (wParam) {
				case ID_RECORD_SHORTCUT: {
					SendMessage(hwnd, WM_COMMAND, MAKEWPARAM(CMD_RECORD, 0), 0);
				} break;
				
				case ID_KEYS_SHORTCUT: {
					SendMessage(hwnd, WM_COMMAND, MAKEWPARAM(CMD_KEYS, 0), 0);
				} break;
				
				case ID_CLIPBOARD_TEXT_SHORTCUT: {
					SendMessage(hwnd, WM_COMMAND, MAKEWPARAM(CMD_CLIPBOARD_TEXT, 0), 0);
				} break;
				
				case ID_CLIPBOARD_FILES_SHORTCUT: {
					SendMessage(hwnd, WM_COMMAND, MAKEWPARAM(CMD_CLIPBOARD_FILES, 0), 0);
				} break;
			}
		} break;
			
		case WM_TIMER: {
			if (record) {
				switch (wParam) {
					case AUDIO_CAPTURE_TIMER: {
						EncodeCapturedAudio(&ac);
					} break;
						
					case VIDEO_UPDATE_TIMER: {
						LARGE_INTEGER time;
						QueryPerformanceCounter(&time);
						EncoderUpdate(&gEncoder, time.QuadPart, gTickFreq.QuadPart);
					} break;
				}
			}
		} break;
		
		case WM_DESTROY: {
			ChangeClipboardChain(hwnd, clipboardViewer);
			RemoveTrayIcon(hwnd);
			PostQuitMessage(0);
		} break;
		
		default: {
			if (msg == WM_TASKBARCREATED) AddTrayIcon(hwnd, 0);
			result = DefWindowProc(hwnd, msg, wParam, lParam);
		}
	}
	
	return result;
}

#ifdef _DEBUG
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR pCmdLine, int nCmdShow) {
#else
void WinMainCRTStartup() {
#endif
	WNDCLASSW wc = {
		.lpfnWndProc = WndProc,
		.hInstance = GetModuleHandleW(0),
		.lpszClassName = APP_NAME
	};
	
	HWND existing = FindWindowW(wc.lpszClassName, 0);
	if (existing) {
		PostMessageW(existing, WM_APP_RUNNING, 0, 0);
		ExitProcess(0);
	}
	
	RegisterClassW(&wc);
	
	HWND hWindow = CreateWindowW(wc.lpszClassName, wc.lpszClassName,
								 WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
								 CW_USEDEFAULT, 0, 0, wc.hInstance, 0);
	
	if (hWindow) {
		MSG msg;
		while (GetMessageW(&msg, 0, 0, 0)) {
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}
	}
	
	ExitProcess(0);
}
