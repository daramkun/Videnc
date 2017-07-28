#include "stdafx.h"

#include "SharpenFilterProcessor.h"

Daramee::Videnc::Processors::Video::SharpenFilterProcessor::SharpenFilterProcessor ()
{

}

Daramee::Videnc::Processors::Video::SharpenFilterProcessor::~SharpenFilterProcessor ()
{

}

void Daramee::Videnc::Processors::Video::SharpenFilterProcessor::GetDefaultOptions ( System::Collections::Generic::Dictionary<System::String ^, System::Object ^> ^ target, bool revalue )
{
	SetValueTo ( target, "range", System::Drawing::RectangleF ( -1, -1, -1, -1 ), revalue );
	SetValueTo ( target, "sharpeness", 1.0f, revalue );
	SetValueTo ( target, "threshold", 0.5f, revalue );
}

void Daramee::Videnc::Processors::Video::SharpenFilterProcessor::Processing ( Daramee::Videnc::MPEG4Streamer ^sender, ProcessorItem ^ item,
	System::TimeSpan timeSpan, ID2D1Bitmap1 * backBuffer, ID2D1DeviceContext * renderTarget )
{
	CComPtr<ID2D1Effect> sharpenEffect;
	if ( FAILED ( renderTarget->CreateEffect ( CLSID_D2D1Sharpen, &sharpenEffect ) ) )
		return;

	CComPtr<ID2D1Bitmap1> tempBitmap = ( ID2D1Bitmap1* ) Direct2DUtility::GetSharedBitmap ( sender, IntPtr ( renderTarget ), IntPtr ( backBuffer ) ).ToPointer ();
	if ( tempBitmap == nullptr ) return;

	ProcessorTimeline ^ start, ^ end;
	Utilities::GetOptimalTimelineOptions ( timeSpan, item, start, end );

	sharpenEffect->SetInput ( 0, tempBitmap );
	sharpenEffect->SetValue ( D2D1_SHARPEN_PROP_SHARPNESS,
		Utilities::WeightedFloat ( start->Time, end->Time, timeSpan, ( float ) start->Option [ "sharpeness" ], ( float ) end->Option [ "sharpeness" ] ) );
	sharpenEffect->SetValue ( D2D1_SHARPEN_PROP_THRESHOLD,
		Utilities::WeightedFloat ( start->Time, end->Time, timeSpan, ( float ) start->Option [ "threshold" ], ( float ) end->Option [ "threshold" ] ) );

	auto startRange = safe_cast< System::Drawing::RectangleF > ( start->Option [ "range" ] );
	auto endRange = safe_cast< System::Drawing::RectangleF > ( end->Option [ "range" ] );
	CheckRectangle ( sender, startRange );
	CheckRectangle ( sender, endRange );

	D2D1_RECT_F rect = ConvertRect ( Utilities::WeightedRectangle ( start->Time, end->Time, timeSpan, startRange, endRange ) );
	D2D1_POINT_2F offset = D2D1::Point2F ( rect.left, rect.top );

	renderTarget->BeginDraw ();
	renderTarget->DrawImage ( sharpenEffect, offset, rect );
	renderTarget->EndDraw ();
}
