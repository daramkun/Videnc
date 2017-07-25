#pragma once

#include <Windows.h>

IStream* CreateStreamFromFilename ( LPCTSTR filename, bool newFile = false );
IStream* CreateStreamFromStandardOutputHandle ();