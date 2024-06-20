@echo off

set "app=Logger"

if "%1" equ "debug" (
	set "cl=/MTd /Od /D_DEBUG /Zi /FC /RTC1 /Fd%app%.pdb /fsanitize=address"
	set "link=/DEBUG"
) else (
	set "cl=/GL /O1 /DNDEBUG /GS-"
	set "link=/LTCG /OPT:REF /OPT:ICF libvcruntime.lib"
)

set "warnings=/wd4100 /wd4706"

if not exist "%~dp0..\output" mkdir "%~dp0..\output"

pushd "%~dp0..\output"
fxc /nologo /T cs_5_0 /E Resize /O3 /WX /Fh "..\src\resize_shader.h" /Vn ResizeShaderBytes /Qstrip_reflect /Qstrip_debug /Qstrip_priv "..\src\shaders.hlsl"
fxc /nologo /T cs_5_0 /E Convert /O3 /WX /Fh "..\src\convert_shader.h" /Vn ConvertShaderBytes /Qstrip_reflect /Qstrip_debug /Qstrip_priv "..\src\shaders.hlsl"
cl /nologo /WX /W4 %warnings% /MP "..\src\main.c" /Fe"%app%" /link /INCREMENTAL:NO /MANIFEST:EMBED /MANIFESTINPUT:"..\res\%app%.manifest" /SUBSYSTEM:WINDOWS /FIXED /merge:_RDATA=.rdata

del *.obj >nul
popd