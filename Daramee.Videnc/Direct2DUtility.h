#pragma once

using namespace System;
using namespace System::Collections::Generic;
using namespace System::Collections::Concurrent;

namespace Daramee
{
	namespace Videnc
	{
		public ref class Direct2DUtility abstract sealed
		{
		public:
			static ID2D1Bitmap1* GetSharedBitmap ( MPEG4Streamer ^ streamer, ID2D1DeviceContext * deviceContext, ID2D1Bitmap1 * copiee );
			static IntPtr GetSharedBitmap ( MPEG4Streamer ^ streamer, IntPtr deviceContext, IntPtr copiee );
			static void DeleteSharedBitmap ( ID2D1DeviceContext * deviceContext );

		public:
			static void Cleanup ();

		private:
			static ConcurrentDictionary<IntPtr, IntPtr> ^ cachedBitmaps;
		};
	}
}