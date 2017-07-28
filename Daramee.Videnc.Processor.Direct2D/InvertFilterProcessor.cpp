#include "stdafx.h"

#include "InvertFilterProcessor.h"

Daramee::Videnc::Processors::Video::InvertFilterProcessor::InvertFilterProcessor ()
{

}

Daramee::Videnc::Processors::Video::InvertFilterProcessor::~InvertFilterProcessor ()
{

}

void Daramee::Videnc::Processors::Video::InvertFilterProcessor::GetDefaultOptions ( System::Collections::Generic::Dictionary<System::String ^, System::Object ^> ^ target, bool revalue )
{
	SetValueTo ( target, "range", System::Drawing::RectangleF ( -1, -1, -1, -1 ), revalue );
}

void Daramee::Videnc::Processors::Video::InvertFilterProcessor::Processing ( Daramee::Videnc::MPEG4Streamer ^sender, ProcessorItem ^ item,
	System::TimeSpan timeSpan, ID2D1Bitmap1 * backBuffer, ID2D1DeviceContext * renderTarget )
{
	CComPtr<ID2D1Effect> invertEffect;
	if ( FAILED ( renderTarget->CreateEffect ( CLSID_D2D1Invert, &invertEffect ) ) )
		return;

	CComPtr<ID2D1Bitmap1> tempBitmap = ( ID2D1Bitmap1* ) Direct2DUtility::GetSharedBitmap ( sender, IntPtr ( renderTarget ), IntPtr ( backBuffer ) ).ToPointer ();
	if ( tempBitmap == nullptr ) return;

	ProcessorTimeline ^ start, ^ end;
	Utilities::GetOptimalTimelineOptions ( timeSpan, item, start, end );

	invertEffect->SetInput ( 0, tempBitmap );

	auto startRange = safe_cast< System::Drawing::RectangleF > ( start->Option [ "range" ] );
	auto endRange = safe_cast< System::Drawing::RectangleF > ( end->Option [ "range" ] );
	CheckRectangle ( sender, startRange );
	CheckRectangle ( sender, endRange );

	D2D1_RECT_F rect = ConvertRect ( Utilities::WeightedRectangle ( start->Time, end->Time, timeSpan, startRange, endRange ) );
	D2D1_POINT_2F offset = D2D1::Point2F ( rect.left, rect.top );

	renderTarget->BeginDraw ();
	renderTarget->DrawImage ( invertEffect, offset, rect );
	renderTarget->EndDraw ();
}
