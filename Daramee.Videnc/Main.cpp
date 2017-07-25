#include <iostream>
#include <cstring>

#include <atlconv.h>
#include <crtdbg.h>

#include "Videnc.hpp"

#define ARGUMENT_OUTPUT										1
#define ARGUMENT_TARGET										2
#define ARGUMENT_QUALITY									3
#define ARGUMENT_SETTINGS									4
#define ARGUMENT_MAXIMUM_VALUE								ARGUMENT_SETTINGS

void print_usage ()
{
	std::cout << "USAGE: Videnc.exe <OUTPUT> <TARGET_VIDEO> <ENCODING QUALITY> <ENCODING SETTINGS JSON FILENAME>" << std::endl;
}

int main ( int argc, char ** argv )
{
	_CrtSetDbgFlag ( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );

	USES_CONVERSION;

	CoInitialize ( nullptr );

	if ( argc != ARGUMENT_MAXIMUM_VALUE + 1 )
	{
		print_usage ();
		return 0xffffffff;
	}

	std::string text = json_file_to_text ( argv [ ARGUMENT_SETTINGS ] );

	std::vector<VideoFilter> videoFilters;
	std::vector<AudioFilter> audioFilters;
	read_encoding_settings ( text, videoFilters, audioFilters );

	IStream * inputStream;
	IStream * outputStream;

	LPCTSTR inputFilename = A2W ( argv [ ARGUMENT_TARGET ] );
	inputStream = CreateStreamFromFilename ( inputFilename );
	if ( nullptr == inputStream )
		return 0xfffffffe;

	bool fragmented;
	if ( strcmp ( "stdout", argv [ ARGUMENT_OUTPUT ] ) == 0 )
	{
		outputStream = CreateStreamFromStandardOutputHandle ();
		fragmented = true;
	}
	else
	{
		LPCTSTR outputName = A2W ( argv [ ARGUMENT_OUTPUT ] );
		outputStream = CreateStreamFromFilename ( outputName, true );
		fragmented = false;
	}
	if ( nullptr == outputStream )
		return 0xfffffffd;

	// TODO
	int result;
	unsigned width, height;
	if ( 0 != ( result = begin_stream ( inputStream, outputStream, fragmented, argv [ ARGUMENT_QUALITY ], &width, &height ) ) )
	{
		end_stream ();
		return result;
	}
	if ( 0 != ( result = begin_video_filter_processor ( videoFilters, width, height ) ) )
	{
		end_video_filter_processor ();
		end_stream ();
		return result;
	}

	streaming ();

	end_video_filter_processor ();
	end_stream ();

	inputStream->Release ();
	outputStream->Release ();

	CoUninitialize ();

	return 0;
}