// stdafx.h : 자주 사용하지만 자주 변경되지는 않는
// 표준 시스템 포함 파일 및 프로젝트 관련 포함 파일이
// 들어 있는 포함 파일입니다.

#pragma once

#include <msclr/marshal.h>
#include <msclr/marshal_cppstd.h>
#include <Windows.h>

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <codecapi.h>
#include <atlbase.h>

#include <wincodec.h>
#include <dxgi.h>
#include <d3d11.h>
#include <d2d1_3.h>
#include <d2d1effects_2.h>
#include <d2d1helper.h>

#define SAFE_RELEASE(x)										if ( x ) x->Release (); x = nullptr;