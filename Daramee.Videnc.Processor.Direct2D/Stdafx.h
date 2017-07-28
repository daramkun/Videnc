// stdafx.h : 자주 사용하지만 자주 변경되지는 않는
// 표준 시스템 포함 파일 및 프로젝트 관련 포함 파일이
// 들어 있는 포함 파일입니다.

#pragma once

#include <msclr/marshal.h>
#include <msclr/marshal_cppstd.h>
#include <Windows.h>
#include <atlbase.h>

#include <wincodec.h>
#include <dxgi.h>
#include <d3d11.h>
#include <d2d1_3.h>
#include <d2d1effects_2.h>
#include <d2d1helper.h>

#using <System.Drawing.dll>

#define SAFE_RELEASE(x)										if ( x ) x->Release (); x = nullptr;

using namespace System;

static inline D2D1_POINT_2F ConvertPoint ( System::Drawing::PointF point )
{
	return D2D1::Point2F ( point.X, point.Y );
}

static inline D2D1_RECT_F ConvertRect ( System::Drawing::RectangleF rect )
{
	return D2D1::RectF ( rect.X, rect.Y, rect.X + rect.Width, rect.Y + rect.Height );
}

static inline void CheckRectangle ( Daramee::Videnc::MPEG4Streamer ^ streamer, System::Drawing::RectangleF % rect )
{
	if ( rect.X == -1 && rect.Y == -1 && rect.Width == -1 && rect.Height == -1 )
	{
		rect.X = 0;
		rect.Y = 0;
		rect.Width = ( float ) streamer->InputMediaInformations.VideoWidth;
		rect.Height = ( float ) streamer->InputMediaInformations.VideoHeight;
	}
}

static inline void SetValueTo ( System::Collections::Generic::Dictionary<System::String ^, System::Object ^> ^ target,
	System::String ^ key, System::Object ^ value, bool revalue )
{
	if ( target->ContainsKey ( key ) )
	{
		if ( revalue )
			target [ key ] = value;
	}
	else target->Add ( key, value );
}