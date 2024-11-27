#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <inttypes.h>
#include "lazyutil/lazylog.h"
//#include <cstdarg>
#include "rtc_base/logging.h"

#define MSC_CHECK_ERROR(err, str)	if(!err.empty())\
{\
	RTC_LOG(LS_ERROR) << str << err; \
	return -1; \
}

#define MSC_CHECK_RESPONSE(resp, info) { \
	std::string err; \
	std::shared_ptr<signaling::ResponseHeader> resphdr = fromJsonString<signaling::ResponseHeader>(resp, err); \
if(!err.empty()) \
{ \
	RTC_LOG(LS_ERROR) << info << err; \
	return -1; \
} \
if(!resphdr || !resphdr->ok) \
{ \
	RTC_LOG(LS_ERROR) << info << ", error response!"; \
	return -1; \
} \
}


#define MSC_CHECK_RESPONSE_ERROR(response, err, info)		MSC_CHECK_ERROR(err, info) \
MSC_CHECK_RESPONSE(response, info)

#define MSC_FUNC_TRACE(fmt, ...) { char info[10240]; snprintf(info, 10240, fmt, ##__VA_ARGS__); RTC_LOG(LS_INFO) << __FUNCTION__ << ":" << info;}


#define PARSER_JSON_STRING(TYPENAME, str)  {std::string err; auto request = fromJsonString<TYPENAME>(str, err); if(!err.empty()) {RTC_LOG(LS_ERROR) << "parser json:" << str << "failed!";}}
//#define lbfunc(fmt, ...)				log_trace(g_plog_ctx, LOG_LEVEL_VERB, NULL, __FILE__, __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__)
/*va_list ap; \
va_start(ap, fmt); \
vsprintf(info, fmt, ap); \
va_end(ap); \
*/