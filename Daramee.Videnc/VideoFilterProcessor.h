#pragma once

int begin_video_filter_processor ( std::vector<VideoFilter> & videoFilters, unsigned width, unsigned height );
void end_video_filter_processor ();

bool is_detected_video_processing_time ( ULONGLONG time );
void process_video_filter ( ULONGLONG time, BYTE * pbBuffer, UINT width, UINT height );