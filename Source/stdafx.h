// stdafx.h : 標準のシステム インクルード ファイルのインクルード ファイル、または
// 参照回数が多く、かつあまり変更されない、プロジェクト専用のインクルード ファイル
// を記述します。
//

#pragma once

#include <assert.h>

// for http server
#include <winsock2.h>

#pragma warning( disable : 4838 )

#if DEBUG
//#define FW_ASSERT	assert
#define FW_ASSERT(_expression)	\
    do {\
        if (!(_expression)) {\
            __debugbreak();	\
        }\
    } while(false)
#else
// Release
#define FW_ASSERT(_expression)	(void)(_expression)
#endif
