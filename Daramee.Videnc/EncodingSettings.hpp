#pragma once

#include <Windows.h>

#define VideoFilterType_Sharpen								"sharpen"
#define VideoFilterType_Blur								"blur"
#define VideoFilterType_Grayscale							"grayscale"
#define VideoFilterType_Invert								"invert"
#define VideoFilterType_Sepia								"sepia"
#define VideoFilterType_AddImage							"addimage"
typedef std::string											VideoFilterType;

#define AudioFilterType_Mute								"mute"
typedef std::string											AudioFilterType;

union VideoFilterOption
{
	struct
	{
		RECT range;
		float sharpeness;
		float threshold;
	} sharpen;
	struct
	{
		RECT range;
		float deviation;
	} blur;
	struct
	{
		RECT range;
	} grayscale;
	struct
	{
		RECT range;
	} invert;
	struct
	{
		RECT range;
		float intensity;
	} sepia;
	struct
	{
		POINT position;
		char imageFilename [ MAX_PATH ];
	} addImage;
};

struct VideoFilterTimeline
{
	ULONGLONG time;
	VideoFilterOption option;
};

struct VideoFilter
{
	VideoFilterType filterType;
	std::vector<VideoFilterTimeline> timeline;
};

struct AudioFilter
{
	AudioFilterType filterType;

};

int read_encoding_settings ( const std::string & text, std::vector<VideoFilter> & videoFilters, std::vector<AudioFilter> & audioFilters );