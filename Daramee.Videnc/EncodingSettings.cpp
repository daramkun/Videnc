#include "Videnc.hpp"

#include "json.hpp"
using json = nlohmann::json;

#include <regex>
using regex = std::regex;
using smatch = std::smatch;
#include <fstream>
#include <codecvt>
#include <sstream>

////////////////////////////////////////////////////////////////////////////////////////////////////

ULONGLONG __convert_time ( const std::string & time )
{
	auto pattern = regex ( "([0-9]+):([0-9][0-9]):([0-9][0-9])(.([0-9][0-9][0-9]))" );
	smatch match;

	ULONGLONG hour, minute, second, millisecond = 0;

	if ( std::regex_match ( time, match, pattern ) )
	{
		hour = atoi ( ( *( ++match.begin () ) ).str ().c_str () );
		minute = atoi ( ( *( ++++match.begin () ) ).str ().c_str () );
		second = atoi ( ( *( ++++++match.begin () ) ).str ().c_str () );
		if ( match.size () > 5 )
			millisecond = atoi ( ( *( ++++++++++match.begin () ) ).str ().c_str () );

		return millisecond + ( second * 10000000ULL ) + ( minute * 600000000ULL ) + ( hour * 36000000000ULL );
	}

	return 0;
}

RECT __convert_range ( const std::string & range )
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

POINT __convert_point ( const std::string & point )
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

POINTFLOAT __convert_point_float ( const std::string & point )
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

VideoFilterOption __convert_filter_option ( VideoFilterType type, json & json )
{
	VideoFilterOption ret = { 0, };

	if ( VideoFilterType_Sharpen == type )
	{
		ret.sharpen.range = __convert_range ( json.value ( "range", "-1,-1,-1,-1" ) );
		ret.sharpen.sharpeness = json.value<float> ( "sharpness", 0.0f );
		ret.sharpen.threshold = json.value<float> ( "threshold", 0.0f );
	}
	else if ( VideoFilterType_Blur == type )
	{
		ret.blur.range = __convert_range ( json.value ( "range", "-1,-1,-1,-1" ) );
		ret.blur.deviation = json.value<float> ( "deviation", 3.0f );
	}
	else if ( VideoFilterType_Grayscale == type )
	{
		ret.grayscale.range = __convert_range ( json.value ( "range", "-1,-1,-1,-1" ) );
	}
	else if ( VideoFilterType_Invert == type )
	{
		ret.invert.range = __convert_range ( json.value ( "range", "-1,-1,-1,-1" ) );
	}
	else if ( VideoFilterType_Sepia == type )
	{
		ret.sepia.range = __convert_range ( json.value ( "range", "-1,-1,-1,-1" ) );
		ret.sepia.intensity = json.value<float> ( "intensity", 0.5f );
	}
	else if ( VideoFilterType_AddImage == type )
	{
		ret.addImage.position = __convert_point ( json.value ( "position", "0,0" ) );
		strcpy ( ret.addImage.imageFilename, json.value ( "filename", "" ).c_str () );
	}

	return ret;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

std::string json_file_to_text ( const char * filename )
{
	std::ifstream ifs ( filename );
	char bomBuffer [ 4 ];
	ifs.read ( bomBuffer, 4 );
	if ( '\xEF' == bomBuffer [ 0 ] && '\xBB' == bomBuffer [ 1 ] && '\xBF' == bomBuffer [ 2 ] )
	{
		ifs.seekg ( 3, std::ios::beg );
		ifs.imbue ( std::locale ( std::locale::empty (), new std::codecvt_utf8<char> ) );
	}
	else if ( '\xFE' == bomBuffer [ 0 ] && '\xFF' == bomBuffer [ 1 ] )
	{
		ifs.seekg ( 2, std::ios::beg );
		ifs.imbue ( std::locale ( std::locale::empty (), new std::codecvt_utf16<char> ) );
	}
	else
		ifs.seekg ( 0, std::ios::beg );

	std::stringstream ss;
	ss << ifs.rdbuf ();
	
	return ss.str ();
}

int read_encoding_settings ( const std::string & text, std::vector<VideoFilter>& videoFilters, std::vector<AudioFilter> & audioFilters )
{
	auto parsed = json::parse ( text.c_str () );
	
	if ( parsed.find ( "filters" ) == parsed.end () ) return 0xfffffaff;
	auto filters = parsed [ "filters" ];

	if ( filters.find ( "video" ) == filters.end () ) return 0xfffffafe;
	if ( filters.find ( "audio" ) == filters.end () ) return 0xfffffafd;

	auto vid_filters = filters [ "video" ];
	for ( auto i = vid_filters.begin (); i != vid_filters.end (); ++i )
	{
		auto currentFilter = *i;

		VideoFilter filter;
		filter.filterType = currentFilter.value ( "type", "" );
		auto timelines = currentFilter [ "timeline" ];
		for ( auto j = timelines.begin (); j != timelines.end (); ++j )
		{
			auto currentTimeline = *j;
			VideoFilterTimeline timeline = { 0, };
			timeline.time = __convert_time ( currentTimeline.value ( "time", "00:00:00.000" ) );
			timeline.option = __convert_filter_option ( filter.filterType, currentTimeline [ "option" ] );
			filter.timeline.push_back ( timeline );
		}

		videoFilters.push_back ( filter );
	}

	auto aud_filters = filters [ "audio" ];
	for ( auto i = aud_filters.begin (); i != aud_filters.end (); ++i )
	{
		auto currentFilter = *i;

		AudioFilter filter;
		filter.filterType = currentFilter.value ( "type", "" );

		audioFilters.push_back ( filter );
	}

	return 0;
}
