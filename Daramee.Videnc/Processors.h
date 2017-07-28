#pragma once

using namespace System;
using namespace System::Collections::Generic;
using namespace System::Linq;

namespace Daramee
{
	namespace Videnc
	{
		public ref class ProcessorTimeline
		{
		public:
			ProcessorTimeline ();

		public:
			property System::TimeSpan Time;
			property Dictionary<String^, Object^>^ Option;

		private:
			Dictionary<String^, Object^>^ option;
		};

		public enum class ProcessorTarget
		{
			Video,
			Audio
		};

		public ref class ProcessorItem
		{
		public:
			ProcessorItem ();

		public:
			property ProcessorTarget Target;
			property System::String^ Processor;
			property List<ProcessorTimeline^> ^ Timeline;

		private:
			List<ProcessorTimeline^> ^ timeline;
		};

		public interface class IProcessor
		{
			property System::String^ ProcessorType { System::String^ get (); }
			void GetDefaultOptions ( System::Collections::Generic::Dictionary<System::String^, System::Object^> ^ target, bool revalue );
		};

		public interface class IVideoProcessor : public IProcessor
		{
			void Processing ( MPEG4Streamer^ sender, ProcessorItem ^ item, System::TimeSpan timeSpan, ID2D1Bitmap1 * backBuffer, ID2D1DeviceContext * renderTarget );
		};

		public interface class IAudioProcessor : public IProcessor
		{
			void Processing ( MPEG4Streamer^ sender, ProcessorItem ^ item, System::TimeSpan timeSpan, BYTE * data, unsigned dataSize );
		};

		public ref class Processor
		{
		public:
			Processor ();
			~Processor ();

		public:
			property IEnumerable<ProcessorItem^>^ Items;

		public:
			bool IsTargetTime ( System::TimeSpan timeSpan, ProcessorTarget target );

			void Processing ( MPEG4Streamer ^ sender, ProcessorTarget target, System::TimeSpan timeSpan, BYTE * data, unsigned dataSize );

		public:
			void AddVideoProcessor ( IVideoProcessor ^ processor );
			void AddAudioProcessor ( IAudioProcessor ^ processor );

		private:
			Dictionary<String^, IVideoProcessor^>^ videoProcessors;
			Dictionary<String^, IAudioProcessor^>^ audioProcessors;

			IWICImagingFactory * wicFactory;

			ID2D1Factory6 * d2dFactory;
			IDXGIDevice1 * dxgiDevice;
			ID2D1Device5 * d2d1Device;
			ID2D1DeviceContext5 * d2d1DeviceContext;

			ID2D1Bitmap1 * d2d1CopyOnlyBitmap;
		};
	}
}