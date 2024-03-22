#pragma once

#include <cstddef> // for size_t


constexpr const char* logfile_fp = "/media/vangelic/DATA/tmp/static_webserver.log";

constexpr size_t logline_datetime_sz = sizeof(int) + sizeof(int); // hour + minutes
constexpr size_t logline_respindxindx = logline_datetime_sz + sizeof(unsigned);
constexpr size_t logline_keepaliveindx = logline_respindxindx + 1;
constexpr size_t logline_reqheadersindx = logline_keepaliveindx + 1;
constexpr size_t logline_reqheaders_len = 1024;
constexpr size_t logline_sz = logline_reqheadersindx + logline_reqheaders_len;
static char logline[
	logline_datetime_sz
	+ sizeof(unsigned) // custom_strview.size()
	+ 1 // response_indx
	+ 1 // keepalive
	+ logline_reqheaders_len
];
