#include "Stdafx.h"

#include "Videnc.h"

ID2D1Bitmap1 * Daramee::Videnc::Direct2DUtility::GetSharedBitmap ( MPEG4Streamer ^ streamer, ID2D1DeviceContext * deviceContext, ID2D1Bitmap1 * copiee )
{
	if ( cachedBitmaps == nullptr )
		cachedBitmaps = gcnew ConcurrentDictionary<IntPtr, IntPtr> ();

	IntPtr key = IntPtr ( deviceContext );
	ID2D1Bitmap1 * retBitmap;
	if ( cachedBitmaps->ContainsKey ( key ) )
	{
		retBitmap = ( ID2D1Bitmap1* ) cachedBitmaps [ key ].ToPointer ();
	}
	else
	{
		CComPtr<ID2D1Bitmap1> bitmap;
		if ( FAILED ( deviceContext->CreateBitmap ( D2D1::SizeU ( streamer->InputMediaInformations.VideoWidth, streamer->InputMediaInformations.VideoHeight ),
			nullptr, streamer->InputMediaInformations.VideoWidth * 4,
			D2D1::BitmapProperties1 ( D2D1_BITMAP_OPTIONS_NONE, D2D1::PixelFormat ( DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED ) ), &bitmap ) ) )
			return nullptr;

		cachedBitmaps->TryAdd ( key, IntPtr ( retBitmap = bitmap.Detach () ) );
	}

	if ( copiee != nullptr )
	{
		retBitmap->CopyFromBitmap ( nullptr, copiee, nullptr );
	}

	return retBitmap;
}

IntPtr Daramee::Videnc::Direct2DUtility::GetSharedBitmap ( MPEG4Streamer ^ streamer, IntPtr deviceContext, IntPtr copiee )
{
	return IntPtr ( GetSharedBitmap ( streamer, ( ID2D1DeviceContext* ) deviceContext.ToPointer (), ( ID2D1Bitmap1* ) copiee.ToPointer () ) );
}

void Daramee::Videnc::Direct2DUtility::DeleteSharedBitmap ( ID2D1DeviceContext * deviceContext )
{
	if ( cachedBitmaps == nullptr )
		return;

	IntPtr key = IntPtr ( deviceContext );
	if ( cachedBitmaps->ContainsKey ( key ) )
	{
		IntPtr value;
		while ( !cachedBitmaps->TryRemove ( key, value ) )
			;
		( ( ID2D1Bitmap1* ) value.ToPointer () )->Release ();
	}
}

void Daramee::Videnc::Direct2DUtility::Cleanup ()
{
	if ( cachedBitmaps != nullptr )
	{
		for each ( auto kv in cachedBitmaps )
		{
			( ( ID2D1Bitmap1* ) kv.Value.ToPointer () )->Release ();
		}
		cachedBitmaps->Clear ();
	}
}
