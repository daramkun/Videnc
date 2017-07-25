#include "Videnc.hpp"

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mfplay.h>
#include <mftransform.h>
#include <codecapi.h>
#include <atlbase.h>

#pragma comment ( lib, "mf.lib" )
#pragma comment ( lib, "mfplat.lib" )
#pragma comment ( lib, "mfuuid.lib" )
#pragma comment ( lib, "mfreadwrite.lib" )

#include <thread>

#define SAFE_RELEASE(x)										if ( x ) x->Release (); x = nullptr;

////////////////////////////////////////////////////////////////////////////////////////////////////

IMFByteStream * inputStream, * outputStream;

IMFSourceReader * inputReader;
UINT inputWidth, inputHeight;
UINT inputSamplerate;
DWORD videoStreamIndex, audioStreamIndex;

IMFMediaSink * outputMediaSink;
IMFSinkWriter * outputWriter;

////////////////////////////////////////////////////////////////////////////////////////////////////

UINT __convert_quality ( const char * quality )
{
	if ( 0 == strcmp ( "--superhigh", quality ) )
		return 0;
	if ( 0 == strcmp ( "--high", quality ) )
		return 1;
	if ( 0 == strcmp ( "--medium", quality ) )
		return 2;
	if ( 0 == strcmp ( "--low", quality ) )
		return 3;
	if ( 0 == strcmp ( "--superlow", quality ) )
		return 4;
	return -1;
}

bool __create_video_output_media_type ( IMFMediaType * base, UINT quality, UINT * width, UINT * height,
	IMFMediaType ** output, IMFMediaType ** input, IMFMediaType ** decoder )
{
	MFGetAttributeSize ( base, MF_MT_FRAME_SIZE, width, height );
	UINT32 numerator, denominator;
	MFGetAttributeRatio ( base, MF_MT_FRAME_RATE, &numerator, &denominator );

	UINT factor = 0;
	switch ( quality )
	{
	case 0: factor = 7; break;
	case 1: factor = 6; break;
	case 2: factor = 5; break;
	case 3: factor = 4; break;
	case 4: factor = 3; break;
	default: return false;
	}

	if ( FAILED ( MFCreateMediaType ( output ) ) )
		return false;
	( *output )->SetGUID ( MF_MT_MAJOR_TYPE, MFMediaType_Video );
	( *output )->SetGUID ( MF_MT_SUBTYPE, MFVideoFormat_H264 );
	( *output )->SetUINT32 ( MF_MT_AVG_BITRATE, ( *width * *height ) * factor );
	( *output )->SetUINT32 ( MF_MT_MPEG2_PROFILE, eAVEncH264VProfile_High );
	( *output )->SetUINT32 ( MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive );
	MFSetAttributeSize ( ( *output ), MF_MT_FRAME_SIZE, *width, *height );
	MFSetAttributeRatio ( ( *output ), MF_MT_FRAME_RATE, numerator, denominator );
	MFSetAttributeRatio ( ( *output ), MF_MT_PIXEL_ASPECT_RATIO, 1, 1 );

	if ( FAILED ( MFCreateMediaType ( input ) ) )
		return false;
	( *input )->SetGUID ( MF_MT_MAJOR_TYPE, MFMediaType_Video );
	( *input )->SetGUID ( MF_MT_SUBTYPE, MFVideoFormat_RGB32 );
	( *input )->SetUINT32 ( MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive );
	MFSetAttributeSize ( ( *input ), MF_MT_FRAME_SIZE, *width, *height );
	MFSetAttributeRatio ( ( *input ), MF_MT_FRAME_RATE, numerator, denominator );
	MFSetAttributeRatio ( ( *input ), MF_MT_PIXEL_ASPECT_RATIO, 1, 1 );

	if ( FAILED ( MFCreateMediaType ( decoder ) ) )
		return false;
	( *decoder )->SetGUID ( MF_MT_MAJOR_TYPE, MFMediaType_Video );
	( *decoder )->SetGUID ( MF_MT_SUBTYPE, MFVideoFormat_RGB32 );

	return true;
}

bool __create_audio_output_media_type ( IMFMediaType * base, UINT quality,  UINT * samplerate,
	IMFMediaType ** output, IMFMediaType ** input, IMFMediaType ** decoder )
{
	UINT channels;
	base->GetUINT32 ( MF_MT_AUDIO_SAMPLES_PER_SECOND, samplerate );
	base->GetUINT32 ( MF_MT_AUDIO_NUM_CHANNELS, &channels );

	UINT bps = 0;
	switch ( quality )
	{
	case 0: bps = 24000; break;
	case 1: bps = 24000; break;
	case 2: bps = 24000; break;
	case 3: bps = 20000; break;
	case 4: bps = 16000; break;
	default: return false;
	}

	if ( FAILED ( MFCreateMediaType ( output ) ) )
		return false;
	( *output )->SetGUID ( MF_MT_MAJOR_TYPE, MFMediaType_Audio );
	( *output )->SetGUID ( MF_MT_SUBTYPE, MFAudioFormat_AAC );
	( *output )->SetUINT32 ( MF_MT_AUDIO_BITS_PER_SAMPLE, 16 );
	( *output )->SetUINT32 ( MF_MT_AUDIO_SAMPLES_PER_SECOND, *samplerate );
	( *output )->SetUINT32 ( MF_MT_AUDIO_NUM_CHANNELS, channels );
	( *output )->SetUINT32 ( MF_MT_AUDIO_AVG_BYTES_PER_SECOND, bps );
	( *output )->SetUINT32 ( MF_MT_AAC_PAYLOAD_TYPE, 0 );

	if ( FAILED ( MFCreateMediaType ( input ) ) )
		return false;
	( *input )->SetGUID ( MF_MT_MAJOR_TYPE, MFMediaType_Audio );
	( *input )->SetGUID ( MF_MT_SUBTYPE, MFAudioFormat_PCM );
	( *input )->SetUINT32 ( MF_MT_AUDIO_BITS_PER_SAMPLE, 16 );
	( *input )->SetUINT32 ( MF_MT_AUDIO_SAMPLES_PER_SECOND, *samplerate );
	( *input )->SetUINT32 ( MF_MT_AUDIO_NUM_CHANNELS, channels );

	if ( FAILED ( MFCreateMediaType ( decoder ) ) )
		return false;
	( *decoder )->SetGUID ( MF_MT_MAJOR_TYPE, MFMediaType_Audio );
	( *decoder )->SetGUID ( MF_MT_SUBTYPE, MFAudioFormat_PCM );

	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

int begin_stream ( IStream * input, IStream * output, bool fragmentedMPEG4, const char * quality, unsigned * width, unsigned * height )
{
	if ( FAILED ( MFStartup ( MF_VERSION ) ) )
		return 0xffffffee;

	if ( FAILED ( MFCreateMFByteStreamOnStream ( input, &inputStream ) ) )
		return 0xffffffed;
	if ( FAILED ( MFCreateMFByteStreamOnStream ( output, &outputStream ) ) )
		return 0xffffffec;

	CComPtr<IMFAttributes> attr;
	if ( FAILED ( MFCreateAttributes ( &attr, 1 ) ) )
		return 0xffffffeb;
	attr->SetUINT32 ( MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, 1 );
	attr->SetUINT32 ( MF_SOURCE_READER_ENABLE_ADVANCED_VIDEO_PROCESSING, 1 );

	if ( FAILED ( MFCreateSourceReaderFromByteStream ( inputStream, attr, &inputReader ) ) )
		return 0xffffffea;

	attr = nullptr;

	CComPtr<IMFMediaType> videoMediaType, audioMediaType;
	if ( FAILED ( inputReader->GetNativeMediaType ( MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, &videoMediaType ) ) )
		return 0xffffffe9;
	if ( FAILED ( inputReader->GetNativeMediaType ( MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, &audioMediaType ) ) )
		return 0xffffffe8;

	CComPtr<IMFMediaType> videoOutputMediaType, videoInputMediaType, videoDecoderMediaType;
	if ( !__create_video_output_media_type ( videoMediaType, __convert_quality ( quality ),
		&inputWidth, &inputHeight,
		&videoOutputMediaType, &videoInputMediaType, &videoDecoderMediaType ) )
		return 0xffffffe7;
	*width = inputWidth;
	*height = inputHeight;
	CComPtr<IMFMediaType> audioOutputMediaType, audioInputMediaType, audioDecoderMediaType;
	if ( !__create_audio_output_media_type ( audioMediaType, __convert_quality ( quality ),
		&inputSamplerate,
		&audioOutputMediaType, &audioInputMediaType, &audioDecoderMediaType ) )
		return 0xffffffe6;

	if ( FAILED ( inputReader->SetCurrentMediaType ( MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, videoDecoderMediaType ) ) )
		return 0xffffffe5;
	if ( FAILED ( inputReader->SetCurrentMediaType ( MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, audioDecoderMediaType ) ) )
		return 0xffffffe4;

	if ( fragmentedMPEG4 )
	{
		if ( FAILED ( MFCreateFMPEG4MediaSink ( outputStream, videoOutputMediaType, audioOutputMediaType, &outputMediaSink ) ) )
			return 0xffffffe3;
	}
	else
	{
		if ( FAILED ( MFCreateMPEG4MediaSink ( outputStream, videoOutputMediaType, audioOutputMediaType, &outputMediaSink ) ) )
			return 0xffffffe3;
	}

	if ( FAILED ( MFCreateAttributes ( &attr, 1 ) ) )
		return 0xffffffe2;
	attr->SetUINT32 ( MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, 1 );
	attr->SetUINT32 ( MF_SINK_WRITER_DISABLE_THROTTLING, 1 );
	attr->SetUINT32 ( MF_MPEG4SINK_MOOV_BEFORE_MDAT, 1 );

	if ( FAILED ( MFCreateSinkWriterFromMediaSink ( outputMediaSink, attr, &outputWriter ) ) )
		return 0xffffffe1;

	attr = nullptr;

	if ( FAILED ( outputWriter->SetInputMediaType ( 0, videoInputMediaType, nullptr ) ) )
		return 0xffffffe0;
	if ( FAILED ( outputWriter->SetInputMediaType ( 1, audioInputMediaType, nullptr ) ) )
		return 0xffffffdf;

	DWORD temp1; LONGLONG temp2; IMFSample * temp3;
	if ( FAILED ( inputReader->ReadSample ( MF_SOURCE_READER_FIRST_VIDEO_STREAM, MF_SOURCE_READER_CONTROLF_DRAIN, &videoStreamIndex, &temp1, &temp2, &temp3 ) ) )
		return 0xffffffde;
	if ( FAILED ( inputReader->ReadSample ( MF_SOURCE_READER_FIRST_AUDIO_STREAM, MF_SOURCE_READER_CONTROLF_DRAIN, &audioStreamIndex, &temp1, &temp2, &temp3 ) ) )
		return 0xffffffdd;

	return 0;
}

void end_stream ()
{
	SAFE_RELEASE ( outputWriter );
	SAFE_RELEASE ( outputMediaSink );
	
	SAFE_RELEASE ( inputReader );

	SAFE_RELEASE ( outputStream );
	SAFE_RELEASE ( inputStream );

	MFShutdown ();
}

void streaming ()
{
	outputWriter->BeginWriting ();

	DWORD actualStreamIndex, streamFlags;
	LONGLONG readedTimeStamp = 0;
	IMFSample * readedSample;

	bool isVideoComplete = false, isAudioComplete = false;

	while ( true )
	{
		inputReader->ReadSample ( MF_SOURCE_READER_ANY_STREAM, 0, &actualStreamIndex, &streamFlags, &readedTimeStamp, &readedSample );

		if ( MF_SOURCE_READERF_ENDOFSTREAM == streamFlags && nullptr == readedSample )
		{
			if ( actualStreamIndex == videoStreamIndex ) isVideoComplete = true;
			else if ( actualStreamIndex == audioStreamIndex ) isAudioComplete = true;

			if ( isVideoComplete && isAudioComplete ) break;
			else continue;
		}
		else
		{
			if ( readedSample == nullptr )
				continue;

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

			if ( actualStreamIndex == videoStreamIndex )
			{
				writeBuffer->SetCurrentLength ( currentLength );
				writeBuffer->Lock ( &pbWriteBuffer, nullptr, nullptr );

				if ( is_detected_video_processing_time ( readedTimeStamp ) )
					process_video_filter ( readedTimeStamp, pbReadBuffer, inputWidth, inputHeight );

				for ( unsigned y = 0; y < inputHeight; ++y )
				{
					RtlCopyMemory (
						pbWriteBuffer + ( y * ( inputWidth * 4 ) ),
						pbReadBuffer + ( ( inputHeight - y - 1 ) * ( inputWidth * 4 ) ),
						inputWidth * 4 );
				}

				writeBuffer->Unlock ();

				HRESULT hr;
				if ( FAILED ( hr = outputWriter->WriteSample ( 0, writeSample ) ) )
					printf ( "Cannot Write Sample of Video\n" );
			}
			else if ( actualStreamIndex == audioStreamIndex )
			{
				writeBuffer->SetCurrentLength ( currentLength );
				writeBuffer->Lock ( &pbWriteBuffer, nullptr, nullptr );

				RtlCopyMemory ( pbWriteBuffer, pbReadBuffer, currentLength );

				writeBuffer->Unlock ();

				HRESULT hr;
				if ( FAILED ( hr = outputWriter->WriteSample ( 1, writeSample ) ) )
					printf ( "Cannot Write Sample of Audio\n" );
			}

			writeBuffer->Release ();
			writeSample->Release ();

			readBuffer->Unlock ();

			readBuffer->Release ();
			readedSample->Release ();
		}
	}

	outputWriter->Finalize ();
	outputMediaSink->Shutdown ();
}
