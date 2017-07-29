#include "Stdafx.h"

#include "Videnc.h"
#include "Direct2DUtility.h"

inline Daramee::Videnc::ProcessorTimeline::ProcessorTimeline ()
{
	option = gcnew Dictionary<String^, Object^> ();
}

inline Daramee::Videnc::ProcessorItem::ProcessorItem ()
{
	timeline = gcnew List<ProcessorTimeline^> ();
}

inline Daramee::Videnc::Processor::Processor ( bool hardwareProcessing )
{
	Items = nullptr;

	videoProcessors = gcnew Dictionary<String^, IVideoProcessor^>;
	audioProcessors = gcnew Dictionary<String^, IAudioProcessor^>;

	CComPtr<IWICImagingFactory> tempWICFactory;
	if ( FAILED ( CoCreateInstance ( CLSID_WICImagingFactory, 0, CLSCTX_INPROC_SERVER, IID_PPV_ARGS ( &tempWICFactory ) ) ) )
		throw gcnew PlatformNotSupportedException ( "Cannot Create WIC Factory object." );
	wicFactory = tempWICFactory.Detach ();

	CComPtr<ID2D1Factory6> tempD2DFactory;
	D2D1_FACTORY_OPTIONS options;
	options.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
	if ( FAILED ( D2D1CreateFactory ( D2D1_FACTORY_TYPE_MULTI_THREADED, __uuidof ( ID2D1Factory6 ), &options, ( void ** ) &tempD2DFactory ) ) )
		throw gcnew PlatformNotSupportedException ( "Cannot Create Direct2D 1 Factory object." );
	d2dFactory = tempD2DFactory.Detach ();

	CComPtr<ID3D11Device> d3dDevice;
	if ( FAILED ( D3D11CreateDevice ( nullptr, hardwareProcessing ? D3D_DRIVER_TYPE_HARDWARE : D3D_DRIVER_TYPE_SOFTWARE, 0, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
		nullptr, 0, D3D11_SDK_VERSION, &d3dDevice, nullptr, nullptr ) ) )
		throw gcnew PlatformNotSupportedException ( "Cannot Create Direct3D 11 Device object with BGRA format supported." );

	CComPtr<IDXGIDevice1> tempDXGIDevice;
	if ( FAILED ( d3dDevice->QueryInterface<IDXGIDevice1> ( &tempDXGIDevice ) ) )
		throw gcnew PlatformNotSupportedException ( "Cannot Query DXGI Device object from Direct3D 11 Device." );
	dxgiDevice = tempDXGIDevice.Detach ();

	CComPtr<ID2D1Device> tempD2DDevice;
	if ( FAILED ( D2D1CreateDevice ( dxgiDevice, D2D1::CreationProperties ( D2D1_THREADING_MODE_MULTI_THREADED,
		D2D1_DEBUG_LEVEL_INFORMATION,
		D2D1_DEVICE_CONTEXT_OPTIONS_ENABLE_MULTITHREADED_OPTIMIZATIONS ), &tempD2DDevice ) ) )
		throw gcnew PlatformNotSupportedException ( "Cannot Create Direct2D 1 Device object." );
	CComQIPtr<ID2D1Device5> tempQID2DDevice = tempD2DDevice;
	d2d1Device = tempQID2DDevice.Detach ();

	CComPtr<ID2D1DeviceContext5> tempD2DDeviceContext;
	if ( FAILED ( d2d1Device->CreateDeviceContext ( D2D1_DEVICE_CONTEXT_OPTIONS_ENABLE_MULTITHREADED_OPTIMIZATIONS, &tempD2DDeviceContext ) ) )
		throw gcnew PlatformNotSupportedException ( "Cannot Create Direct2D 1 Device Context object." );
	d2d1DeviceContext = tempD2DDeviceContext.Detach ();
}

Daramee::Videnc::Processor::~Processor ()
{
	SAFE_RELEASE ( d2d1CopyOnlyBitmap );

	SAFE_RELEASE ( d2d1DeviceContext );
	SAFE_RELEASE ( d2d1Device );
	SAFE_RELEASE ( dxgiDevice );
	SAFE_RELEASE ( d2dFactory );

	SAFE_RELEASE ( wicFactory );

	for each ( auto vp in videoProcessors )
		delete vp.Value;
	for each ( auto ap in audioProcessors )
		delete ap.Value;
}

inline bool Daramee::Videnc::Processor::IsTargetTime ( System::TimeSpan timeSpan, ProcessorTarget target )
{
	if ( nullptr == Items ) return false;
	for each ( ProcessorItem^ i in Items )
	{
		if ( i->Target == target && ( i->Timeline [ 0 ]->Time <= timeSpan && i->Timeline [ i->Timeline->Count - 1 ]->Time >= timeSpan ) )
		{
			if ( ( target == ProcessorTarget::Video && videoProcessors->ContainsKey ( i->Processor ) ) ||
				( target == ProcessorTarget::Audio && audioProcessors->ContainsKey ( i->Processor ) ) )
				return true;
		}
	}
	return false;
}

inline void Daramee::Videnc::Processor::Processing ( MPEG4Streamer ^ sender, ProcessorTarget target, System::TimeSpan timeSpan, BYTE * data, unsigned dataSize )
{
	if ( nullptr == Items ) return;

	auto info = sender->InputMediaInformations;

	CComPtr<ID2D1Bitmap1> backBuffer;

	if ( ProcessorTarget::Video == target )
	{
		d2d1DeviceContext->CreateBitmap ( D2D1::SizeU ( info.VideoWidth, info.VideoHeight ), data, info.VideoWidth * 4,
			D2D1::BitmapProperties1 ( D2D1_BITMAP_OPTIONS_TARGET, D2D1::PixelFormat ( DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED ) ),
			&backBuffer );
		d2d1DeviceContext->SetTarget ( backBuffer );
	}
	else
	{

	}

	for each ( ProcessorItem^ i in Items )
	{
		if ( target == ProcessorTarget::Video && i->Target == target && videoProcessors->ContainsKey ( i->Processor ) )
		{
			IVideoProcessor^ processor = videoProcessors [ i->Processor ];
			if ( processor != nullptr )
			{
				processor->Processing ( sender, i, timeSpan, backBuffer, d2d1DeviceContext );
			}
		}
		else if ( target == ProcessorTarget::Audio && i->Target == target && audioProcessors->ContainsKey ( i->Processor ) )
		{
			IAudioProcessor^ processor = audioProcessors [ i->Processor ];
			if ( processor != nullptr )
			{
				processor->Processing ( sender, i, timeSpan, data, dataSize );
			}
		}
	}

	if ( ProcessorTarget::Video == target )
	{
		if ( d2d1CopyOnlyBitmap == nullptr )
		{
			CComPtr<ID2D1Bitmap1> tempCopyOnlyBitmap;
			if ( FAILED ( d2d1DeviceContext->CreateBitmap ( D2D1::SizeU ( info.VideoWidth, info.VideoHeight ), nullptr, info.VideoWidth * 4,
				D2D1::BitmapProperties1 ( D2D1_BITMAP_OPTIONS_CANNOT_DRAW | D2D1_BITMAP_OPTIONS_CPU_READ,
					D2D1::PixelFormat ( DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED ) ),
				&tempCopyOnlyBitmap ) ) )
				throw gcnew PlatformNotSupportedException ( "Cannot Create Direct2D 1 CPU Accessable Bitmap object." );
			d2d1CopyOnlyBitmap = tempCopyOnlyBitmap.Detach ();
		}

		D2D1_MAPPED_RECT mappedRect;
		d2d1CopyOnlyBitmap->CopyFromBitmap ( nullptr, backBuffer, nullptr );

		Direct2DUtility::DeleteSharedBitmap ( d2d1DeviceContext );
		d2d1DeviceContext->SetTarget ( nullptr );

		d2d1CopyOnlyBitmap->Map ( D2D1_MAP_OPTIONS_READ, &mappedRect );
		RtlCopyMemory ( data, mappedRect.bits, dataSize );
		d2d1CopyOnlyBitmap->Unmap ();
	}
}

inline void Daramee::Videnc::Processor::AddVideoProcessor ( IVideoProcessor ^ processor )
{
	videoProcessors->Add ( processor->ProcessorType, processor );
}

inline void Daramee::Videnc::Processor::AddAudioProcessor ( IAudioProcessor ^ processor )
{
	audioProcessors->Add ( processor->ProcessorType, processor );
}
