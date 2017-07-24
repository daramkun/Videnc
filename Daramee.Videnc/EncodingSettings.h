#pragma once

#include <Windows.h>

#define VideoFilterType_Sharpen								"sharpen"
#define VideoFilterType_Blur								"blur"
#define VideoFilterType_Grayscale							"grayscale"
#define VideoFilterType_Invert								"invert"
#define VideoFilterType_Sepia								"sepia"
#define VideoFilterType_BlendImage							"blendimage"
typedef std::string											VideoFilterType;

union VideoFilterOption
{
	struct
	{
		RECT range;
		float sharpeness;
		float threshold;
	} sharpenFilterOption;
	struct
	{
		RECT range;
		float deviation;
	} blurFilterOption;
	struct
	{
		RECT range;
	} grayscaleFilterOption;
	struct
	{
		RECT range;
	} invertFilterOption;
	struct
	{
		RECT range;
		float intensity;
	} sepiaFilterOption;
	struct
	{
		POINT position;
		char imageFilename [ MAX_PATH ];
	} blendImageFilterOption;
};

struct VideoFilter
{
	VideoFilterType filterType;
	ULONGLONG startTime, endTime;
	VideoFilterOption startOption, endOption;
};

int read_encoding_settings ( const std::string & text, std::vector<VideoFilter> & videoFilters );