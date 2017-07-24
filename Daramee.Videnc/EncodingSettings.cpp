#include <string>
#include <vector>

#include "EncodingSettings.h"

#include "json.hpp"
using json = nlohmann::json;

#include <regex>
using regex = std::regex;
using smatch = std::smatch;

ULONGLONG convert_time ( const std::string & time )
{
	auto pattern = regex ( "([0-9]+):([0-9][0-9]):([0-9][0-9]).([0-9][0-9][0-9])" );
	smatch match;

	int hour, minute, second, millisecond;

	if ( std::regex_match ( time, match, pattern ) )
	{
		hour = atoi ( ( *( ++match.begin () ) ).str ().c_str () );
		minute = atoi ( ( *( ++++match.begin () ) ).str ().c_str () );
		second = atoi ( ( *( ++++++match.begin () ) ).str ().c_str () );
		millisecond = atoi ( ( *( ++++++++match.begin () ) ).str ().c_str () );

		ULONGLONG ret = 0;
		ret += millisecond;
		ret += ( second * 1000 );
		ret += ( minute * 60 * 1000 );
		ret += ( hour * 60 * 60 * 1000 );

		return ret * 10000ULL;
	}

	return 0;
}

RECT convert_range ( const std::string & range )
{
	auto pattern = regex ( "([0-9]+), ?([0-9]+), ?([0-9]+), ?([0-9]+)" );
	smatch match;

	RECT ret;
	if ( std::regex_match ( range, match, pattern ) )
	{
		ret.left = atoi ( ( *( ++match.begin () ) ).str ().c_str () );
		ret.top = atoi ( ( *( ++++match.begin () ) ).str ().c_str () );
		ret.right = atoi ( ( *( ++++++match.begin () ) ).str ().c_str () );
		ret.bottom = atoi ( ( *( ++++++++match.begin () ) ).str ().c_str () );
	}

	return ret;
}

POINT convert_point ( const std::string & point )
{
	auto pattern = regex ( "(-?[0-9]+), ?(-?[0-9]+)" );
	smatch match;

	POINT ret;
	if ( std::regex_match ( point, match, pattern ) )
	{
		ret.x = atoi ( ( *( ++match.begin () ) ).str ().c_str () );
		ret.y = atoi ( ( *( ++++match.begin () ) ).str ().c_str () );
	}

	return ret;
}

POINTFLOAT convert_point_float ( const std::string & point )
{
	auto pattern = regex ( "(-?[0-9]+.[0-9]*), ?(-?[0-9]+.[0-9]*)" );
	smatch match;

	POINTFLOAT ret;
	if ( std::regex_match ( point, match, pattern ) )
	{
		ret.x = ( float ) atof ( ( *( ++match.begin () ) ).str ().c_str () );
		ret.y = ( float ) atof ( ( *( ++++match.begin () ) ).str ().c_str () );
	}

	return ret;
}

VideoFilterOption convert_filter_option ( VideoFilterType type, json & json )
{
	VideoFilterOption ret = { 0, };

	if ( VideoFilterType_Sharpen == type )
	{
		ret.sharpenFilterOption.range = convert_range ( json.value ( "range", "-1,-1,-1,-1" ) );
		ret.sharpenFilterOption.sharpeness = json.value<float> ( "sharpness", 0.0f );
		ret.sharpenFilterOption.threshold = json.value<float> ( "threshold", 0.0f );
	}
	else if ( VideoFilterType_Blur == type )
	{
		ret.blurFilterOption.range = convert_range ( json.value ( "range", "-1,-1,-1,-1" ) );
		ret.blurFilterOption.deviation = json.value<float> ( "deviation", 3.0f );
	}
	else if ( VideoFilterType_Grayscale == type )
	{
		ret.grayscaleFilterOption.range = convert_range ( json.value ( "range", "-1,-1,-1,-1" ) );
	}
	else if ( VideoFilterType_Invert == type )
	{
		ret.invertFilterOption.range = convert_range ( json.value ( "range", "-1,-1,-1,-1" ) );
	}
	else if ( VideoFilterType_Sepia == type )
	{
		ret.sepiaFilterOption.range = convert_range ( json.value ( "range", "-1,-1,-1,-1" ) );
		ret.sepiaFilterOption.intensity = json.value<float> ( "intensity", 0.5f );
	}
	else if ( VideoFilterType_BlendImage == type )
	{
		ret.blendImageFilterOption.position = convert_point ( json.value ( "position", "0,0" ) );
		strcpy ( ret.blendImageFilterOption.imageFilename, json.value ( "filename", "" ).c_str () );
	}

	return ret;
}

int read_encoding_settings ( const std::string & text, std::vector<VideoFilter>& videoFilters )
{
	auto parsed = json::parse ( text.c_str () );
	
	if ( parsed.find ( "filters" ) == parsed.end () )
		return 0xfffffaff;
	auto filters = parsed [ "filters" ];

	if ( filters.find ( "video" ) == filters.end () )
		return 0xfffffafe;
	auto vid_filters = filters [ "video" ];

	size_t videoFilterCount = vid_filters.size ();
	for ( size_t i = 0; i < videoFilterCount; ++i )
	{
		auto currentFilter = vid_filters [ i ];

		VideoFilter filter;
		filter.filterType = currentFilter.value ( "type", "" );
		filter.startTime = convert_time ( currentFilter.value ( "start_time", "00:00:00.000" ) );
		filter.endTime = convert_time ( currentFilter.value ( "end_time", "00:00:00.000" ) );
		filter.startOption = convert_filter_option ( filter.filterType, currentFilter [ "start_option" ] );
		filter.endOption = convert_filter_option ( filter.filterType, currentFilter [ "end_option" ] );

		videoFilters.push_back ( filter );
	}
}
