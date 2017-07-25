#include "Videnc.hpp"

#include <d2d1_1.h>
#include <d2d1_1helper.h>
#include <d2d1effects_2.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wincodec.h>
#include <atlbase.h>
#include <atlconv.h>

#pragma comment ( lib, "d2d1.lib" )
#pragma comment ( lib, "d3d11.lib" )
#pragma comment ( lib, "dxgi.lib" )
#pragma comment ( lib, "dxguid.lib" )
#pragma comment ( lib, "WindowsCodecs.lib" )

#include <map>

#define SAFE_RELEASE(x)										if ( x ) x->Release (); x = nullptr;

////////////////////////////////////////////////////////////////////////////////////////////////////

ID2D1Factory * d2d1Factory;
IDXGIDevice1 * dxgiDevice;
ID2D1Device * d2d1Device;
ID2D1DeviceContext * d2d1DeviceContext;
ID2D1Bitmap1 * d2d1CopyOnlyBitmap;
IWICImagingFactory * wicFactory;
std::vector<VideoFilter> videoFilters;
std::map<std::string, ID2D1Bitmap*> cachedBitmaps;

////////////////////////////////////////////////////////////////////////////////////////////////////

IWICBitmap* __create_bitmap ( IWICImagingFactory * wicFactory, std::string & filename )
{
	USES_CONVERSION;

	CComPtr<IWICBitmapDecoder> decoder;
	wicFactory->CreateDecoderFromFilename ( A2W ( filename.c_str () ), 0, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder );

	CComPtr<IWICBitmapFrameDecode> frameDecode;
	if ( FAILED ( decoder->GetFrame ( 0, &frameDecode ) ) )
		return nullptr;

	CComPtr<IWICFormatConverter> converter;
	if ( FAILED ( wicFactory->CreateFormatConverter ( &converter ) ) )
		return nullptr;

	if ( FAILED ( converter->Initialize ( frameDecode, GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, 0, 0.0, WICBitmapPaletteTypeCustom ) ) )
		return nullptr;

	IWICBitmap * bitmap;
	wicFactory->CreateBitmapFromSource ( converter, WICBitmapCacheOnDemand, &bitmap );

	return bitmap;
}

bool __is_match_time_range ( ULONGLONG time, std::vector<VideoFilterTimeline> & timeline )
{
	return ( time >= timeline [ 0 ].time && time <= timeline [ timeline.size () - 1 ].time );
}

void __get_optimal_options ( ULONGLONG time, std::vector<VideoFilterTimeline> & timeline, VideoFilterTimeline & start, VideoFilterTimeline & end )
{
	size_t size = timeline.size () - 1;
	for ( size_t i = 0; i < size; ++i )
	{
		VideoFilterTimeline & current = timeline [ i ];
		VideoFilterTimeline & next = timeline [ i + 1 ];

		if ( time >= current.time && time <= next.time )
		{
			start = current;
			end = next;
			return;
		}
	}
}

float __weight_float ( ULONGLONG start, ULONGLONG end, ULONGLONG current, float sv, float ev )
{
	float weight = ( end - current ) / ( float ) ( end - start );
	return ev - ( ( ev - sv ) * weight );
}

D2D1_POINT_2F __weight_point ( ULONGLONG start, ULONGLONG end, ULONGLONG current, D2D1_POINT_2F & sv, D2D1_POINT_2F & ev )
{
	return D2D1::Point2F ( __weight_float ( start, end, current, sv.x, ev.x ), __weight_float ( start, end, current, sv.y, ev.y ) );
}

D2D1_SIZE_U __weight_size ( ULONGLONG start, ULONGLONG end, ULONGLONG current, D2D1_SIZE_U & sv, D2D1_SIZE_U & ev )
{
	return D2D1::SizeU ( ( UINT32 ) __weight_float ( start, end, current, sv.width, ev.width ), ( UINT32 ) __weight_float ( start, end, current, sv.height, ev.height ) );
}

D2D1_RECT_F __weight_rect ( ULONGLONG start, ULONGLONG end, ULONGLONG current, D2D1_RECT_F & sv, D2D1_RECT_F & ev )
{
	return D2D1::RectF (
		__weight_float ( start, end, current, sv.left, ev.left ),
		__weight_float ( start, end, current, sv.top, ev.top ),
		__weight_float ( start, end, current, sv.right, ev.right ),
		__weight_float ( start, end, current, sv.bottom, ev.bottom ) );
}

////////////////////////////////////////////////////////////////////////////////////////////////////

int begin_video_filter_processor ( std::vector<VideoFilter> & videoFilters, unsigned width, unsigned height )
{
	if ( FAILED ( D2D1CreateFactory ( D2D1_FACTORY_TYPE_MULTI_THREADED, &d2d1Factory ) ) )
		return 0xffffffcf;

	CComPtr<ID3D11Device> d3dDevice;
	if ( FAILED ( D3D11CreateDevice ( nullptr, D3D_DRIVER_TYPE_HARDWARE, 0, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
		nullptr, 0, D3D11_SDK_VERSION, &d3dDevice, nullptr, nullptr ) ) )
		return 0xffffffcd;

	if ( FAILED ( d3dDevice->QueryInterface<IDXGIDevice1> ( &dxgiDevice ) ) )
		return 0xffffffcc;

	if ( FAILED ( D2D1CreateDevice ( dxgiDevice, D2D1::CreationProperties ( D2D1_THREADING_MODE_MULTI_THREADED,
		D2D1_DEBUG_LEVEL_INFORMATION,
		D2D1_DEVICE_CONTEXT_OPTIONS_ENABLE_MULTITHREADED_OPTIMIZATIONS ), &d2d1Device ) ) )
		return 0xffffffcb;

	if ( FAILED ( d2d1Device->CreateDeviceContext ( D2D1_DEVICE_CONTEXT_OPTIONS_ENABLE_MULTITHREADED_OPTIMIZATIONS, &d2d1DeviceContext ) ) )
		return 0xffffffca;

	if ( FAILED ( d2d1DeviceContext->CreateBitmap ( D2D1::SizeU ( width, height ), nullptr, width * 4,
		D2D1::BitmapProperties1 ( D2D1_BITMAP_OPTIONS_CANNOT_DRAW | D2D1_BITMAP_OPTIONS_CPU_READ,
			D2D1::PixelFormat ( DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED ) ),
		&d2d1CopyOnlyBitmap ) ) )
		return 0xffffffc9;

	if ( CoCreateInstance ( CLSID_WICImagingFactory, 0, CLSCTX_INPROC_SERVER, IID_PPV_ARGS ( &wicFactory ) ) )
		return 0xffffffc8;

	::videoFilters = videoFilters;
	for ( auto i = videoFilters.begin (); i != videoFilters.end (); ++i )
	{
		if ( VideoFilterType_AddImage == ( *i ).filterType )
		{
			std::string filename = ( *i ).timeline [ 0 ].option.addImage.imageFilename;
			IWICBitmap * bitmap = __create_bitmap ( wicFactory, filename );
			
			if ( nullptr == bitmap )
				return 0xffffffc7;

			ID2D1Bitmap * d2d1Bitmap;
			if ( FAILED ( d2d1DeviceContext->CreateBitmapFromWicBitmap ( bitmap, &d2d1Bitmap ) ) )
				return 0xffffffc6;

			bitmap->Release ();

			cachedBitmaps.insert ( std::pair<std::string, ID2D1Bitmap*> ( filename, d2d1Bitmap ) );
		}
	}

	return 0;
}

void end_video_filter_processor ()
{
	for ( auto j = cachedBitmaps.begin (); j != cachedBitmaps.end (); ++j )
		( *j ).second->Release ();
	cachedBitmaps.clear ();

	SAFE_RELEASE ( wicFactory );
	SAFE_RELEASE ( d2d1CopyOnlyBitmap );
	SAFE_RELEASE ( d2d1DeviceContext );
	SAFE_RELEASE ( d2d1Device );
	SAFE_RELEASE ( dxgiDevice );
	SAFE_RELEASE ( d2d1Factory );
}

bool is_detected_video_processing_time ( ULONGLONG time )
{
	for ( auto i = videoFilters.begin (); i != videoFilters.end (); ++i )
	{
		if ( __is_match_time_range ( time, ( *i ).timeline ) )
			return true;
	}

	return false;
}

void process_video_filter ( ULONGLONG time, BYTE * pbBuffer, UINT width, UINT height )
{
	CComPtr<ID2D1Bitmap1> image, backBuffer;
	d2d1DeviceContext->CreateBitmap ( D2D1::SizeU ( width, height ), pbBuffer, width * 4,
		D2D1::BitmapProperties1 ( D2D1_BITMAP_OPTIONS_NONE, D2D1::PixelFormat ( DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED ) ),
		&image );
	d2d1DeviceContext->CreateBitmap ( D2D1::SizeU ( width, height ), pbBuffer, width * 4,
		D2D1::BitmapProperties1 ( D2D1_BITMAP_OPTIONS_TARGET, D2D1::PixelFormat ( DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED ) ),
		&backBuffer );

	d2d1DeviceContext->BeginDraw ();
	d2d1DeviceContext->SetTarget ( backBuffer );

	for ( auto i = videoFilters.begin (); i != videoFilters.end (); ++i )
	{
		auto item = ( *i );
		if ( __is_match_time_range ( time, item.timeline ) )
		{
			VideoFilterTimeline start, end;
			__get_optimal_options ( time, item.timeline, start, end );
			ULONGLONG & st = start.time, & et = end.time;
			VideoFilterOption & so = start.option, & eo = end.option;

			if ( VideoFilterType_Sharpen == item.filterType )
			{
				CComPtr<ID2D1Effect> sharpenEffect;
				d2d1DeviceContext->CreateEffect ( CLSID_D2D1Sharpen, &sharpenEffect );

				sharpenEffect->SetInput ( 0, image );
				sharpenEffect->SetValue ( D2D1_SHARPEN_PROP_SHARPNESS,
					__weight_float ( st, et, time, so.sharpen.sharpeness, eo.sharpen.sharpeness ) );
				sharpenEffect->SetValue ( D2D1_SHARPEN_PROP_THRESHOLD,
					__weight_float ( st, et, time, so.sharpen.threshold, eo.sharpen.threshold ) );
				
				D2D1_POINT_2F offset = __weight_point ( st, et, time,
					D2D1::Point2F ( so.sharpen.range.left, so.sharpen.range.top ),
					D2D1::Point2F ( eo.sharpen.range.left, eo.sharpen.range.top ) );
				D2D1_RECT_F rect = __weight_rect ( st, et, time,
					D2D1::RectF ( so.sharpen.range.left, so.sharpen.range.top, so.sharpen.range.right, so.sharpen.range.bottom ),
					D2D1::RectF ( eo.sharpen.range.left, eo.sharpen.range.top, eo.sharpen.range.right, eo.sharpen.range.bottom ) );

				d2d1DeviceContext->DrawImage ( sharpenEffect, offset, rect );
			}
			else if ( VideoFilterType_Blur == item.filterType )
			{
				CComPtr<ID2D1Effect> blurEffect;
				d2d1DeviceContext->CreateEffect ( CLSID_D2D1GaussianBlur, &blurEffect );

				blurEffect->SetInput ( 0, image );
				blurEffect->SetValue ( D2D1_GAUSSIANBLUR_PROP_STANDARD_DEVIATION,
					__weight_float ( st, et, time, so.blur.deviation, eo.blur.deviation ) );

				D2D1_POINT_2F offset = __weight_point ( st, et, time,
					D2D1::Point2F ( so.blur.range.left, so.blur.range.top ),
					D2D1::Point2F ( eo.blur.range.left, eo.blur.range.top ) );
				D2D1_RECT_F rect = __weight_rect ( st, et, time,
					D2D1::RectF ( so.blur.range.left, so.blur.range.top, so.blur.range.right, so.blur.range.bottom ),
					D2D1::RectF ( eo.blur.range.left, eo.blur.range.top, eo.blur.range.right, eo.blur.range.bottom ) );

				d2d1DeviceContext->DrawImage ( blurEffect, offset, rect );
			}
			else if ( VideoFilterType_Grayscale == item.filterType )
			{
				CComPtr<ID2D1Effect> grayscaleEffect;
				d2d1DeviceContext->CreateEffect ( CLSID_D2D1Grayscale, &grayscaleEffect );

				grayscaleEffect->SetInput ( 0, image );

				D2D1_POINT_2F offset = __weight_point ( st, et, time,
					D2D1::Point2F ( so.grayscale.range.left, so.grayscale.range.top ),
					D2D1::Point2F ( eo.grayscale.range.left, eo.grayscale.range.top ) );
				D2D1_RECT_F rect = __weight_rect ( st, et, time,
					D2D1::RectF ( so.grayscale.range.left, so.grayscale.range.top, so.grayscale.range.right, so.grayscale.range.bottom ),
					D2D1::RectF ( eo.grayscale.range.left, eo.grayscale.range.top, eo.grayscale.range.right, eo.grayscale.range.bottom ) );

				d2d1DeviceContext->DrawImage ( grayscaleEffect, offset, rect );
			}
			else if ( VideoFilterType_Invert == item.filterType )
			{
				CComPtr<ID2D1Effect> invertEffect;
				d2d1DeviceContext->CreateEffect ( CLSID_D2D1Invert, &invertEffect );

				invertEffect->SetInput ( 0, image );

				D2D1_POINT_2F offset = __weight_point ( st, et, time,
					D2D1::Point2F ( so.invert.range.left, so.invert.range.top ),
					D2D1::Point2F ( eo.invert.range.left, eo.invert.range.top ) );
				D2D1_RECT_F rect = __weight_rect ( st, et, time,
					D2D1::RectF ( so.invert.range.left, so.invert.range.top, so.invert.range.right, so.invert.range.bottom ),
					D2D1::RectF ( eo.invert.range.left, eo.invert.range.top, eo.invert.range.right, eo.invert.range.bottom ) );

				d2d1DeviceContext->DrawImage ( invertEffect, offset, rect );
			}
			else if ( VideoFilterType_Sepia == item.filterType )
			{
				CComPtr<ID2D1Effect> sepiaEffect;
				d2d1DeviceContext->CreateEffect ( CLSID_D2D1Sepia, &sepiaEffect );

				sepiaEffect->SetInput ( 0, image );
				sepiaEffect->SetValue ( D2D1_SEPIA_PROP_INTENSITY,
					__weight_float ( st, et, time, so.sepia.intensity, eo.sepia.intensity ) );

				D2D1_POINT_2F offset = __weight_point ( st, et, time,
					D2D1::Point2F ( so.sepia.range.left, so.sepia.range.top ),
					D2D1::Point2F ( eo.sepia.range.left, eo.sepia.range.top ) );
				D2D1_RECT_F rect = __weight_rect ( st, et, time,
					D2D1::RectF ( so.sepia.range.left, so.sepia.range.top, so.sepia.range.right, so.sepia.range.bottom ),
					D2D1::RectF ( eo.sepia.range.left, eo.sepia.range.top, eo.sepia.range.right, eo.sepia.range.bottom ) );

				d2d1DeviceContext->DrawImage ( sepiaEffect, offset, rect );
			}
			else if ( VideoFilterType_AddImage == item.filterType )
			{
				D2D1_POINT_2F offset = __weight_point ( st, et, time,
					D2D1::Point2F ( so.addImage.position.x, so.addImage.position.y ),
					D2D1::Point2F ( eo.addImage.position.x, eo.addImage.position.y ) );
				d2d1DeviceContext->DrawImage ( cachedBitmaps [ std::string ( so.addImage.imageFilename ) ], offset );
			}
		}
	}

	d2d1DeviceContext->EndDraw ();

	D2D1_MAPPED_RECT mappedRect;
	d2d1CopyOnlyBitmap->CopyFromBitmap ( nullptr, backBuffer, nullptr );

	d2d1CopyOnlyBitmap->Map ( D2D1_MAP_OPTIONS_READ, &mappedRect );

	RtlCopyMemory ( pbBuffer, mappedRect.bits, width * height * 4 );

	d2d1CopyOnlyBitmap->Unmap ();
}
