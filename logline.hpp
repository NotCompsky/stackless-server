#pragma once

#include <cstddef> // for size_t
#include <time.h> // for time_t
#include <netinet/in.h> // for in_addr_t


constexpr const char* logfile_fp = "/media/vangelic/DATA/tmp/static_webserver.log";

constexpr size_t logline_datetime_sz = sizeof(time_t);
constexpr size_t logline_ipaddrindx = logline_datetime_sz;
constexpr size_t logline_customstrviewindx = logline_ipaddrindx + sizeof(in_addr_t);
constexpr size_t logline_respindxindx = logline_customstrviewindx + sizeof(unsigned);
constexpr size_t logline_keepaliveindx = logline_respindxindx + 1;
constexpr size_t logline_reqheadersindx = logline_keepaliveindx + 1;
constexpr size_t logline_reqheaders_len = 1024;
constexpr size_t logline_sz = logline_reqheadersindx + logline_reqheaders_len;
static char logline[
	logline_datetime_sz
	+ sizeof(in_addr_t) // ip_address
	+ sizeof(unsigned) // custom_strview.size()
	+ 1 // response_indx
	+ 1 // keepalive
	+ logline_reqheaders_len
];
