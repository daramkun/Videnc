// 기본 DLL 파일입니다.

#include "stdafx.h"

#include "Videnc.h"

Daramee::Videnc::MPEG4Streamer::MPEG4Streamer ( Stream^ outputStream, MPEG4StreamSettings settings, Stream^ inputStream )
	: outputStream ( new ManagedIStream ( outputStream ) ), inputStream ( new ManagedIStream ( inputStream ) ),
	  streamSettings ( settings ), streamingStarted ( false )
{
	if ( FAILED ( MFStartup ( MF_VERSION, MFSTARTUP_FULL ) ) )
		throw gcnew NotSupportedException ( "Media Foundation is not Ready to this Platform." );

	outputByteStream = CreateByteStream ( this->outputStream );
	if ( nullptr == outputByteStream ) throw gcnew IOException ( "Cannot create IMFByteStream from System::IO::Stream(outputStream)." );
	inputByteStream = CreateByteStream ( this->inputStream );
	if ( nullptr == inputByteStream ) throw gcnew IOException ( "Cannot create IMFByteStream from System::IO::Stream(inputStream)." );
	
	CComPtr<IMFAttributes> attr = CreateAttribute ( true );
	CComPtr<IMFSourceReader> tempSourceReader;
	if ( FAILED ( MFCreateSourceReaderFromByteStream ( inputByteStream, attr, &tempSourceReader ) ) )
		throw gcnew System::IO::IOException ( "Cannot create IMFSourceReader." );
	inputSourceReader = tempSourceReader.Detach ();
	attr.Release ();

	GetSourceReaderStreamIndicies ();

	CComPtr<IMFMediaType> nativeVideoMediaType, nativeAudioMediaType;
	GetNativeMediaTypeFromSourceReader ( &nativeVideoMediaType, &nativeAudioMediaType );
	AnalyseInputMediaInformation ( nativeVideoMediaType, nativeAudioMediaType );

	CComPtr<IMFMediaType> videoOutputMediaType, videoInputMediaType, videoDecoderMediaType;
	CComPtr<IMFMediaType> audioOutputMediaType, audioInputMediaType, audioDecoderMediaType;

	if ( !CreateVideoOutputMediaTypes ( &videoOutputMediaType, &videoInputMediaType, &videoDecoderMediaType ) )
		throw gcnew System::Exception ( "Cannot Create Media Types for Video." );
	if ( !CreateAudioOutputMediaTypes ( &audioOutputMediaType, &audioInputMediaType, &audioDecoderMediaType ) )
		throw gcnew System::Exception ( "Cannot Create Media Types for Audio." );

	if ( inputMediaInfo.HasVideo )
		if ( FAILED ( inputSourceReader->SetCurrentMediaType ( readerVideoStreamIndex, nullptr, videoDecoderMediaType ) ) )
			throw gcnew System::IO::IOException ( "Cannot Set Media Type for Video." );
	if ( inputMediaInfo.HasAudio )
		if ( FAILED ( inputSourceReader->SetCurrentMediaType ( readerAudioStreamIndex, nullptr, audioDecoderMediaType ) ) )
			throw gcnew System::IO::IOException ( "Cannot Set Media Type for Audio." );

	CComPtr<IMFMediaSink> tempMediaSink;
	if ( streamSettings.IsFragmentedMPEG4Stream )
	{
		if ( FAILED ( MFCreateFMPEG4MediaSink ( outputByteStream, videoOutputMediaType, audioOutputMediaType, &tempMediaSink ) ) )
			throw gcnew System::IO::IOException ( "Cannot Create Video Media Sink." );
	}
	else
	{
		if ( FAILED ( MFCreateMPEG4MediaSink ( outputByteStream, videoOutputMediaType, audioOutputMediaType, &tempMediaSink ) ) )
			throw gcnew System::IO::IOException ( "Cannot Create Audio Media Sink." );
	}
	outputMediaSink = tempMediaSink.Detach ();

	attr = CreateAttribute ( false );
	CComPtr<IMFSinkWriter> tempSinkWriter;
	if ( FAILED ( MFCreateSinkWriterFromMediaSink ( outputMediaSink, attr, &tempSinkWriter ) ) )
		throw gcnew System::IO::IOException ( "Cannot Create Sink Writer." );
	outputSinkWriter = tempSinkWriter.Detach ();
	attr.Release ();

	if ( inputMediaInfo.HasVideo )
		if ( FAILED ( outputSinkWriter->SetInputMediaType ( 0, videoInputMediaType, nullptr ) ) )
			throw gcnew System::Exception ( "Cannot Set Video Media Type to IMFSinkWriter." );
	if ( inputMediaInfo.HasAudio )
		if ( FAILED ( outputSinkWriter->SetInputMediaType ( 1, audioInputMediaType, nullptr ) ) )
			throw gcnew System::Exception ( "Cannot Set Audio Media Type to IMFSinkWriter." );
}

Daramee::Videnc::MPEG4Streamer::~MPEG4Streamer ()
{
	if ( this->Processor != nullptr )
		delete this->Processor;
	this->Processor = nullptr;

	if ( streamingThread != nullptr )
		StopStreaming ();

	SAFE_RELEASE ( outputSinkWriter );
	SAFE_RELEASE ( outputMediaSink );

	SAFE_RELEASE ( inputSourceReader );

	SAFE_RELEASE ( inputByteStream );
	SAFE_RELEASE ( outputByteStream );

	MFShutdown ();
}

void Daramee::Videnc::MPEG4Streamer::StartStreaming ()
{
	if ( streamingStarted ) return;

	streamingStarted = true;

	streamingThread = gcnew System::Threading::Thread ( gcnew System::Threading::ParameterizedThreadStart ( this, &Daramee::Videnc::MPEG4Streamer::StreamingThreadLogic ) );
	streamingThread->Start ( this );
}

void Daramee::Videnc::MPEG4Streamer::StopStreaming ()
{
	if ( streamingThread == nullptr ) return;
	streamingStarted = false;
	streamingThread->Join ();
	streamingThread = nullptr;
}

IMFByteStream* Daramee::Videnc::MPEG4Streamer::CreateByteStream ( ManagedIStream * stream )
{
	CComPtr<IStream> tempIStream = stream;
	IMFByteStream * tempByteStream;
	if ( FAILED ( MFCreateMFByteStreamOnStream ( tempIStream, &tempByteStream ) ) )
		throw gcnew System::IO::IOException ( "Cannot create Media Foundation Byte Stream from System::IO::Stream." );
	return tempByteStream;
}

IMFAttributes * Daramee::Videnc::MPEG4Streamer::CreateAttribute ( bool forSourceReader )
{
	CComPtr<IMFAttributes> attr;
	if ( FAILED ( MFCreateAttributes ( &attr, 1 ) ) ) return nullptr;
	if ( streamSettings.HardwareAccelerationEncoding )
		attr->SetUINT32 ( MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, 1 );

	if ( forSourceReader )
	{
		if ( streamSettings.HardwareAccelerationEncoding )
			attr->SetUINT32 ( MF_SOURCE_READER_ENABLE_ADVANCED_VIDEO_PROCESSING, 1 );
		else
			attr->SetUINT32 ( MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, 1 );
	}
	else
	{
		attr->SetUINT32 ( MF_SINK_WRITER_DISABLE_THROTTLING, 1 );
		attr->SetUINT32 ( MF_MPEG4SINK_MOOV_BEFORE_MDAT, 1 );
	}

	return attr.Detach ();
}

bool Daramee::Videnc::MPEG4Streamer::GetNativeMediaTypeFromSourceReader ( IMFMediaType ** videoMediaType, IMFMediaType ** audioMediaType )
{
	if ( nullptr != videoMediaType )
		if ( FAILED ( inputSourceReader->GetNativeMediaType ( readerVideoStreamIndex, 0, videoMediaType ) ) )
			return false;
	if ( nullptr != audioMediaType )
		if ( FAILED ( inputSourceReader->GetNativeMediaType ( readerAudioStreamIndex, 0, audioMediaType ) ) )
			return false;
	return true;
}

void Daramee::Videnc::MPEG4Streamer::GetSourceReaderStreamIndicies ()
{
	DWORD target, temp1; LONGLONG temp2; IMFSample * temp3;
	
	inputSourceReader->ReadSample ( MF_SOURCE_READER_FIRST_VIDEO_STREAM, MF_SOURCE_READER_CONTROLF_DRAIN, &target, &temp1, &temp2, &temp3 );
	readerVideoStreamIndex = target;
	
	inputSourceReader->ReadSample ( MF_SOURCE_READER_FIRST_AUDIO_STREAM, MF_SOURCE_READER_CONTROLF_DRAIN, &target, &temp1, &temp2, &temp3 );
	readerAudioStreamIndex = target;
}

void Daramee::Videnc::MPEG4Streamer::AnalyseInputMediaInformation ( IMFMediaType * videoMediaType, IMFMediaType * audioMediaType )
{
	inputMediaInfo = InputMediaInformation ();

	if ( nullptr != videoMediaType )
	{
		inputMediaInfo.HasVideo = true;

		UINT width, height;
		MFGetAttributeSize ( videoMediaType, MF_MT_FRAME_SIZE, &width, &height );
		UINT32 numerator, denominator;
		MFGetAttributeRatio ( videoMediaType, MF_MT_FRAME_RATE, &numerator, &denominator );

		inputMediaInfo.VideoWidth = width;
		inputMediaInfo.VideoHeight = height;
		inputMediaInfo.VideoFramerateNumerator = numerator;
		inputMediaInfo.VideoFramerateDenominator = denominator;
	}
	else inputMediaInfo.HasVideo = false;

	if ( nullptr != audioMediaType )
	{
		inputMediaInfo.HasAudio = true;

		UINT channels, samplerate, bitsPerSample;
		audioMediaType->GetUINT32 ( MF_MT_AUDIO_NUM_CHANNELS, &channels );
		audioMediaType->GetUINT32 ( MF_MT_AUDIO_SAMPLES_PER_SECOND, &samplerate );
		audioMediaType->GetUINT32 ( MF_MT_AUDIO_BITS_PER_SAMPLE, &bitsPerSample );

		inputMediaInfo.AudioChannels = channels;
		inputMediaInfo.AudioSamplerate = ( AudioSamplerate ) samplerate;
		inputMediaInfo.AudioBitsPerSample = bitsPerSample;
	}
	else inputMediaInfo.HasAudio = false;

	PROPVARIANT prop;
	if ( FAILED ( inputSourceReader->GetPresentationAttribute ( MF_SOURCE_READER_MEDIASOURCE, MF_PD_DURATION, &prop ) ) )
		throw gcnew System::IO::IOException ( "Cannot get Duration of Input Media." );
	inputMediaInfo.Duration = TimeSpan::FromTicks ( prop.uhVal.QuadPart );
}

bool Daramee::Videnc::MPEG4Streamer::CreateVideoOutputMediaTypes ( IMFMediaType ** outputMediaType, IMFMediaType ** inputMediaType, IMFMediaType ** decoderMediaType )
{
	if ( !inputMediaInfo.HasVideo )
	{
		*outputMediaType = nullptr;
		*inputMediaType = nullptr;
		*decoderMediaType = nullptr;

		return true;
	}

	CComPtr<IMFMediaType> ret;

	if ( FAILED ( MFCreateMediaType ( &ret ) ) ) return false;
	ret->SetGUID ( MF_MT_MAJOR_TYPE, MFMediaType_Video );
	ret->SetGUID ( MF_MT_SUBTYPE, MFVideoFormat_H264 );
	ret->SetUINT32 ( MF_MT_AVG_BITRATE, streamSettings.VideoBitrate );
	ret->SetUINT32 ( MF_MT_MPEG2_PROFILE, eAVEncH264VProfile_High );
	ret->SetUINT32 ( MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive );
	if ( streamSettings.ResizeVideo )
		MFSetAttributeSize ( ret, MF_MT_FRAME_SIZE, streamSettings.VideoWidth, streamSettings.VideoHeight );
	else
		MFSetAttributeSize ( ret, MF_MT_FRAME_SIZE, inputMediaInfo.VideoWidth, inputMediaInfo.VideoHeight );
	if ( streamSettings.ChangeFramerate )
		MFSetAttributeRatio ( ret, MF_MT_FRAME_RATE, ( UINT ) streamSettings.VideoFramerate, 1 );
	else
		MFSetAttributeRatio ( ret, MF_MT_FRAME_RATE, inputMediaInfo.VideoFramerateNumerator, inputMediaInfo.VideoFramerateDenominator );
	MFSetAttributeRatio ( ret, MF_MT_PIXEL_ASPECT_RATIO, 1, 1 );
	*outputMediaType = ret.Detach ();

	if ( FAILED ( MFCreateMediaType ( &ret ) ) ) return false;
	ret->SetGUID ( MF_MT_MAJOR_TYPE, MFMediaType_Video );
	ret->SetGUID ( MF_MT_SUBTYPE, MFVideoFormat_RGB32 );
	ret->SetUINT32 ( MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive );
	MFSetAttributeSize ( ret, MF_MT_FRAME_SIZE, inputMediaInfo.VideoWidth, inputMediaInfo.VideoHeight );
	MFSetAttributeRatio ( ret, MF_MT_FRAME_RATE, inputMediaInfo.VideoFramerateNumerator, inputMediaInfo.VideoFramerateDenominator );
	MFSetAttributeRatio ( ret, MF_MT_PIXEL_ASPECT_RATIO, 1, 1 );
	*inputMediaType = ret.Detach ();

	if ( FAILED ( MFCreateMediaType ( &ret ) ) ) return false;
	ret->SetGUID ( MF_MT_MAJOR_TYPE, MFMediaType_Video );
	ret->SetGUID ( MF_MT_SUBTYPE, MFVideoFormat_RGB32 );
	*decoderMediaType = ret.Detach ();

	return true;
}

bool Daramee::Videnc::MPEG4Streamer::CreateAudioOutputMediaTypes ( IMFMediaType ** outputMediaType, IMFMediaType ** inputMediaType, IMFMediaType ** decoderMediaType )
{
	if ( !inputMediaInfo.HasAudio )
	{
		*outputMediaType = nullptr;
		*inputMediaType = nullptr;
		*decoderMediaType = nullptr;

		return true;
	}

	CComPtr<IMFMediaType> ret;

	if ( FAILED ( MFCreateMediaType ( &ret ) ) ) return false;
	ret->SetGUID ( MF_MT_MAJOR_TYPE, MFMediaType_Audio );
	ret->SetGUID ( MF_MT_SUBTYPE, MFAudioFormat_AAC );
	ret->SetUINT32 ( MF_MT_AUDIO_BITS_PER_SAMPLE, 16 );
	ret->SetUINT32 ( MF_MT_AUDIO_SAMPLES_PER_SECOND, ( UINT ) inputMediaInfo.AudioSamplerate );
	ret->SetUINT32 ( MF_MT_AUDIO_NUM_CHANNELS, inputMediaInfo.AudioChannels );
	ret->SetUINT32 ( MF_MT_AUDIO_AVG_BYTES_PER_SECOND, ( UINT ) streamSettings.AudioBitrate );
	ret->SetUINT32 ( MF_MT_AAC_PAYLOAD_TYPE, 0 );
	*outputMediaType = ret.Detach ();

	if ( FAILED ( MFCreateMediaType ( &ret ) ) ) return false;
	ret->SetGUID ( MF_MT_MAJOR_TYPE, MFMediaType_Audio );
	ret->SetGUID ( MF_MT_SUBTYPE, MFAudioFormat_PCM );
	ret->SetUINT32 ( MF_MT_AUDIO_BITS_PER_SAMPLE, inputMediaInfo.AudioBitsPerSample );
	ret->SetUINT32 ( MF_MT_AUDIO_SAMPLES_PER_SECOND, ( UINT ) inputMediaInfo.AudioSamplerate );
	ret->SetUINT32 ( MF_MT_AUDIO_NUM_CHANNELS, inputMediaInfo.AudioChannels );
	*inputMediaType = ret.Detach ();

	if ( FAILED ( MFCreateMediaType ( &ret ) ) ) return false;
	ret->SetGUID ( MF_MT_MAJOR_TYPE, MFMediaType_Audio );
	ret->SetGUID ( MF_MT_SUBTYPE, MFAudioFormat_PCM );
	*decoderMediaType = ret.Detach ();

	return true;
}

void Daramee::Videnc::MPEG4Streamer::StreamingThreadLogic ( System::Object ^ param )
{
	MPEG4Streamer^ streamer = dynamic_cast< MPEG4Streamer^ > ( param );
	streamer->proceed = 0;
	if ( FAILED ( streamer->outputSinkWriter->BeginWriting () ) )
		throw gcnew IOException ( "Cannot Writing Media to IMFSinkWriter." );

	DWORD actualStreamIndex, streamFlags;
	LONGLONG readedTimeStamp = 0;
	IMFSample * readedSample;

	TimeSpan videoProceed, audioProceed;

	bool isVideoComplete = false, isAudioComplete = false;

	while ( streamer->IsStreamingStarted )
	{
		if ( FAILED ( streamer->inputSourceReader->ReadSample ( MF_SOURCE_READER_ANY_STREAM, 0, &actualStreamIndex, &streamFlags, &readedTimeStamp, &readedSample ) ) )
			throw gcnew IOException ( "Cannot Read Sample." );

		if ( MF_SOURCE_READERF_ENDOFSTREAM == streamFlags && nullptr == readedSample )
		{
			if ( actualStreamIndex == streamer->readerVideoStreamIndex ) isVideoComplete = true;
			else if ( actualStreamIndex == streamer->readerAudioStreamIndex ) isAudioComplete = true;

			if ( isVideoComplete && isAudioComplete ) break;
			else continue;
		}
		else
		{
			if ( readedSample == nullptr ) continue;
			if ( actualStreamIndex != readerVideoStreamIndex && actualStreamIndex != readerAudioStreamIndex ) continue;

			DWORD writeTargetStreamIndex;
			if ( actualStreamIndex == readerVideoStreamIndex ) writeTargetStreamIndex = 0;
			else writeTargetStreamIndex = 1;

			TimeSpan timeSpan = TimeSpan::FromTicks ( readedTimeStamp );

			LONGLONG sampleDuration, sampleTime;
			DWORD sampleFlags;
			readedSample->GetSampleDuration ( &sampleDuration );
			readedSample->GetSampleTime ( &sampleTime );
			readedSample->GetSampleFlags ( &sampleFlags );

			IMFMediaBuffer * readBuffer;
			readedSample->ConvertToContiguousBuffer ( &readBuffer );

			BYTE * pbReadBuffer, *pbWriteBuffer;
			DWORD maxLength, currentLength;
			readBuffer->Lock ( &pbReadBuffer, &maxLength, &currentLength );

			IMFSample * writeSample;
			MFCreateSample ( &writeSample );
			IMFMediaBuffer * writeBuffer;
			MFCreateMemoryBuffer ( maxLength, &writeBuffer );
			writeSample->AddBuffer ( writeBuffer );

			writeSample->SetSampleDuration ( sampleDuration );
			writeSample->SetSampleTime ( sampleTime );
			writeSample->SetSampleFlags ( 0 );

			writeBuffer->SetCurrentLength ( currentLength );
			writeBuffer->Lock ( &pbWriteBuffer, nullptr, nullptr );

			if ( writeTargetStreamIndex == 0 )
			{
				if ( nullptr != streamer->Processor && streamer->Processor->IsTargetTime ( timeSpan, ProcessorTarget::Video ) )
					streamer->Processor->Processing ( streamer, ProcessorTarget::Video, timeSpan, pbReadBuffer, currentLength );

				for ( unsigned y = 0; y < inputMediaInfo.VideoHeight; ++y )
				{
					RtlCopyMemory (
						pbWriteBuffer + ( y * ( inputMediaInfo.VideoWidth * 4 ) ),
						pbReadBuffer + ( ( inputMediaInfo.VideoHeight - y - 1 ) * ( inputMediaInfo.VideoWidth * 4 ) ),
						inputMediaInfo.VideoWidth * 4 );
				}

				videoProceed = timeSpan;
			}
			else
			{
				if ( nullptr != streamer->Processor && streamer->Processor->IsTargetTime ( timeSpan, ProcessorTarget::Audio ) )
					streamer->Processor->Processing ( streamer, ProcessorTarget::Audio, timeSpan, pbReadBuffer, currentLength );

				RtlCopyMemory ( pbWriteBuffer, pbReadBuffer, currentLength );

				audioProceed = timeSpan;
			}

			writeBuffer->Unlock ();

			if ( FAILED ( streamer->outputSinkWriter->WriteSample ( writeTargetStreamIndex, writeSample ) ) )
				printf ( "Cannot Write Video or Audio Sample.\n" );

			SAFE_RELEASE ( writeBuffer );
			SAFE_RELEASE ( writeSample );

			SAFE_RELEASE ( readBuffer );
			SAFE_RELEASE ( readedSample );

			double videoPercentage = ( videoProceed.Ticks / ( double ) inputMediaInfo.Duration.Ticks );
			double audioPercentage = ( audioProceed.Ticks / ( double ) inputMediaInfo.Duration.Ticks );
			proceed = ( videoPercentage + audioPercentage ) / 2;
		}
	}

	streamer->outputSinkWriter->Finalize ();
	streamer->outputMediaSink->Shutdown ();

	streamer->proceed = 1;
	streamer->streamingStarted = false;
	streamer->streamingThread = nullptr;
}
