#pragma once

namespace Daramee
{
	namespace Videnc
	{
		namespace Processors
		{
			namespace Video
			{
				public ref class GaussianBlurFilterProcessor : public Daramee::Videnc::IVideoProcessor
				{
				public:
					GaussianBlurFilterProcessor ();
					~GaussianBlurFilterProcessor ();

				public:
					// IProcessor을(를) 통해 상속됨
					virtual property System::String ^ ProcessorType { System::String^ get () { return "blur"; } }
					virtual void GetDefaultOptions ( System::Collections::Generic::Dictionary<System::String ^, System::Object ^> ^ target, bool revalue );

					// IVideoProcessor을(를) 통해 상속됨
					virtual property Daramee::Videnc::ProcessorTarget Target { Daramee::Videnc::ProcessorTarget get () { return Daramee::Videnc::ProcessorTarget::Video; } }
					virtual void Processing ( Daramee::Videnc::MPEG4Streamer ^sender, ProcessorItem ^ item, System::TimeSpan timeSpan,
						ID2D1Bitmap1 * backBuffer, ID2D1DeviceContext * renderTarget );
				};
			}
		}
	}
}