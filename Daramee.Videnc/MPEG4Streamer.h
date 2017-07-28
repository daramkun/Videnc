// Daramee.Videnc.h

#pragma once

using namespace System;
using namespace System::IO;

namespace Daramee
{
	namespace Videnc
	{
		public enum class VideoFramerate
		{
			_24 = 24,
			_30 = 30,
			_60 = 60,
		};

		public enum class VideoBitrate : unsigned
		{
			_60Mbps = 60000000,
			_24Mbps = 24000000,
			_12Mbps = 12000000,
			_4Mbps = 400000,
			_2Mbps = 200000,
		};

		public enum class AudioBitrate : unsigned
		{
			_192Kbps = 24000,
			_160Kbps = 20000,
			_128Kbps = 16000,
			_96Kbps = 12000,
		};

		public enum class AudioSamplerate : unsigned
		{
			_44100 = 44100,
			_48000 = 48000,
		};

		public value struct MPEG4StreamSettings
		{
		public:
			property bool ResizeVideo;
			property unsigned VideoWidth;
			property unsigned VideoHeight;
			property bool ChangeFramerate;
			property VideoFramerate VideoFramerate;

			property unsigned VideoBitrate;
			property AudioBitrate AudioBitrate;

			property bool IsFragmentedMPEG4Stream;
			property bool HardwareAccelerationEncoding;
		};

		public value struct InputMediaInformation
		{
		public:
			property bool HasVideo;
			property unsigned VideoWidth;
			property unsigned VideoHeight;
			property unsigned VideoFramerateNumerator;
			property unsigned VideoFramerateDenominator;

			property bool HasAudio;
			property unsigned AudioChannels;
			property unsigned AudioBitsPerSample;
			property AudioSamplerate AudioSamplerate;

			property TimeSpan Duration;
		};

		ref class Processor;

		public ref class MPEG4Streamer
		{
		public:
			MPEG4Streamer ( Stream^ outputStream, MPEG4StreamSettings settings, Stream^ inputStream );
			virtual ~MPEG4Streamer ();

		public:
			property Processor^ Processor;

		public:
			void StartStreaming ();
			void StopStreaming ();

		public:
			property bool IsStreamingStarted { bool get () { return streamingStarted; } }
			property double Proceed { double get () { return proceed; } }

		public:
			property MPEG4StreamSettings StreamSettings { MPEG4StreamSettings get () { return streamSettings; } }
			property InputMediaInformation InputMediaInformations { InputMediaInformation get () { return inputMediaInfo; } }

		private:
			IMFByteStream* CreateByteStream ( ManagedIStream * stream );
			IMFAttributes* CreateAttribute ( bool forSourceReader );
			
			bool GetNativeMediaTypeFromSourceReader ( IMFMediaType ** videoMediaType, IMFMediaType ** audioMediaType );
			void GetSourceReaderStreamIndicies ();
			void AnalyseInputMediaInformation ( IMFMediaType * videoMediaType, IMFMediaType * audioMediaType );

			bool CreateVideoOutputMediaTypes ( IMFMediaType ** outputMediaType,
				IMFMediaType ** inputMediaType, IMFMediaType ** decoderMediaType );
			bool CreateAudioOutputMediaTypes ( IMFMediaType ** outputMediaType,
				IMFMediaType ** inputMediaType, IMFMediaType ** decoderMediaType );

			void StreamingThreadLogic ( System::Object ^ param );

		private:
			ManagedIStream * outputStream;
			ManagedIStream * inputStream;

			MPEG4StreamSettings streamSettings;

			IMFByteStream * outputByteStream;
			IMFByteStream * inputByteStream;

			IMFSourceReader * inputSourceReader;
			DWORD readerVideoStreamIndex, readerAudioStreamIndex;
			InputMediaInformation inputMediaInfo;

			IMFMediaSink * outputMediaSink;
			IMFSinkWriter * outputSinkWriter;

			double proceed;

			bool streamingStarted;
			System::Threading::Thread ^ streamingThread;
		};
	}
}
