#pragma once

int begin_stream ( IStream * input, IStream * output, bool fragmentedMPEG4, const char * quality, unsigned * width, unsigned * height );
void end_stream ();

void streaming ();