#include "stdafx.h"

#include "AddImageProcessor.h"

Daramee::Videnc::Processors::Video::AddImageProcessor::AddImageProcessor ()
{
	cachedBitmaps = gcnew Dictionary<String^, IntPtr> ();

	CComPtr<IWICImagingFactory> tempWICFactory;
	if ( FAILED ( CoCreateInstance ( CLSID_WICImagingFactory, 0, CLSCTX_INPROC_SERVER, IID_PPV_ARGS ( &tempWICFactory ) ) ) )
		throw gcnew PlatformNotSupportedException ( "Cannot Create WIC Factory object." );
	wicFactory = tempWICFactory.Detach ();
}

Daramee::Videnc::Processors::Video::AddImageProcessor::~AddImageProcessor ()
{
	SAFE_RELEASE ( wicFactory );

	for each ( auto i in cachedBitmaps )
	{
		IWICBitmap * bitmap = ( IWICBitmap* ) i.Value.ToPointer ();
		bitmap->Release ();
	}
	cachedBitmaps->Clear ();
}

void Daramee::Videnc::Processors::Video::AddImageProcessor::GetDefaultOptions ( System::Collections::Generic::Dictionary<System::String ^, System::Object ^> ^ target, bool revalue )
{
	SetValueTo ( target, "position", System::Drawing::PointF ( 0, 0 ), revalue );
	SetValueTo ( target, "filename", "", revalue );
}

void Daramee::Videnc::Processors::Video::AddImageProcessor::Processing ( Daramee::Videnc::MPEG4Streamer ^sender, ProcessorItem ^ item,
	System::TimeSpan timeSpan, ID2D1Bitmap1 * backBuffer, ID2D1DeviceContext * renderTarget )
{
	ProcessorTimeline ^ start, ^ end;
	Utilities::GetOptimalTimelineOptions ( timeSpan, item, start, end );

	IWICBitmap * bitmap = GetBitmap ( dynamic_cast< System::String^ > ( start->Option [ "filename" ] ) );
	CComPtr<ID2D1Bitmap1> d2dBitmap;
	renderTarget->CreateBitmapFromWicBitmap ( bitmap, &d2dBitmap );

	auto sp = safe_cast< System::Drawing::PointF > ( start->Option [ "position" ] );
	auto ep = safe_cast< System::Drawing::PointF > ( end->Option [ "position" ] );
	D2D1_POINT_2F offset = ConvertPoint ( Utilities::WeightedPoint ( start->Time, end->Time, timeSpan, sp, ep ) );

	renderTarget->BeginDraw ();
	renderTarget->DrawImage ( d2dBitmap, offset );
	renderTarget->EndDraw ();
}

IWICBitmap * Daramee::Videnc::Processors::Video::AddImageProcessor::GetBitmap ( System::String ^ filename )
{
	if ( !cachedBitmaps->ContainsKey ( filename ) )
	{
		CComPtr<IWICBitmapDecoder> decoder;
		wicFactory->CreateDecoderFromFilename ( msclr::interop::marshal_as<std::wstring> ( filename ).c_str (),
			0, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder );

		CComPtr<IWICBitmapFrameDecode> frameDecode;
		if ( FAILED ( decoder->GetFrame ( 0, &frameDecode ) ) )
			return nullptr;

		CComPtr<IWICFormatConverter> converter;
		if ( FAILED ( wicFactory->CreateFormatConverter ( &converter ) ) )
			return nullptr;

		if ( FAILED ( converter->Initialize ( frameDecode, GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, 0, 0.0, WICBitmapPaletteTypeCustom ) ) )
			return nullptr;

		CComPtr<IWICBitmap> bitmap;
		wicFactory->CreateBitmapFromSource ( converter, WICBitmapCacheOnDemand, &bitmap );

		cachedBitmaps->Add ( filename, IntPtr ( bitmap.Detach () ) );
	}
	return ( IWICBitmap* ) cachedBitmaps [ filename ].ToPointer ();
}
