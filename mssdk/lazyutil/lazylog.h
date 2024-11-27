/***************************************************************************************************************
 * filename     lazylog.h
 * describe     write log to console and local disk file
 * author       Created by dawson on 2019/04/18
 * Copyright    ?2007 - 2029 Sunvally. All Rights Reserved.
 ***************************************************************************************************************/
#ifndef _LAZY_LOG_H_
#define _LAZY_LOG_H_
#include <stdint.h>
#include <stdarg.h>
#include <inttypes.h>
// log level define
#define LOG_LEVEL_VERB            0x1
#define LOG_LEVEL_INFO            0x2
#define LOG_LEVEL_TRACE           0x3
#define LOG_LEVEL_WARN            0x4
#define LOG_LEVEL_ERROR           0x5
#define LOG_LEVEL_DISABLE         0x6
#define g_plog_ctx  g_hirtc_record_log
// log output mode define
// console output log info
#define LOG_OUTPUT_FLAG_CONSOLE     0x1

// file output log info
#define LOG_OUTPUT_FLAG_FILE        0x2

// file and console output
#define LOG_OUTPUT_FILE_AND_CONSOLE 0x3

#define DEFAULT_DIR_MODE 0777
#define DEFAULT_FILE_MODE 0644

/**********************************************************************************************************
 * 初始化日志信息
 * @parma plogdir		日志路径，可包括部分日志名
 * @parma nloglevel	日志等级
 * @parma output_mode	日志输出模式
 * @param nmayjor   程序主版本号，程序主体架构有很大的调整时（如重构）变更
 * @param nminor    程序次版本号，程序功能和结构迭代时变更
 * @param nmicro    程序微版本号, 程序修改一些小bug和小幅度的功能调整时变更
 * @param ntiny     程序极小版本号，程序日志增删、同一个类型bug的不同修改时变更
 * @return 日志上下文结构体
 **********************************************************************************************************/
struct log_ctx *init_log(const char *plogdir, const int nloglevel, const int output_mode, const int nmayjor, const int nminor, const int nmicro, const int ntiny);


/**********************************************************************************************************
 * 初始化日志信息
 * @parma plogdir		日志路径，可包括部分日志名
 * @parma nloglevel	日志等级
 * @parma output_mode	日志输出模式
 **********************************************************************************************************/
struct log_ctx *init_log2(const char *plogdir, const int nloglevel, const int output_mode);

void set_log_rotation_size(struct log_ctx *plogctx, int64_t rotation_size);
/**********************************************************************************************************
 * 销毁日志上下文
 * @parma pplogctx		日志上下文结构体双指针
 **********************************************************************************************************/
void close_log_contex(struct log_ctx **pplogctx);

/**********************************************************************************************************
 * 设置当前日志版本号
 * @parma:plogctx 日志上下文结构体
 * @param nmayjor  程序主版本号，程序主体架构有很大的调整时（如重构）变更
 * @param nminor   程序次版本号，程序功能和结构迭代时变更
 * @param nmicro   程序微版本号, 程序修改一些小bug和小幅度的功能调整时变更
 * @param ntiny    程序极小版本号，程序日志增删、同一个类型bug的不同修改时变更
 * @return 0为成功，否则为失败
 **********************************************************************************************************/
int set_current_version(struct log_ctx *plogctx, const int nmayjor, const int nminor, const int nmicro, const int ntiny);


/**********************************************************************************************************
 * log to the output
 * @parma plogctx 日志上下文结构体
 * @parms nlevel 日志输出等级
 * @param tag 日志输出标签
 * @param pfile 日志输出代码文件名称
 * @param line 日志输出代码位置（行数）
 * @param pfun 日志输出代码函数名称
 * @param fmt , ... 日志输出格式化字符串及其可变参数
 * @return 无
**********************************************************************************************************/
void log_trace(struct log_ctx *plogctx, const int nlevel, const char *tag, const char *pfile, int line, const char *pfun, const char *fmt, ...);

/**********************************************************************************************************
 * log to the output
 * @parma plogctx 日志上下文结构体
 * @parms nlevel 日志输出等级
 * @param tag 日志输出标签
 * @param pfile 日志输出代码文件名称
 * @param line 日志输出代码位置（行数）
 * @param pfun 日志输出代码函数名称
 * @param pbuf , ... 日志输出内容
 * @param size , ... 日志输出长度
 * @return 无
**********************************************************************************************************/
void log_buf(struct log_ctx *plogctx, const int nlevel, const char *tag, const char *pfile, int line, const char *pfun, const char *pbuf, int size);

/**********************************************************************************************************
 * log to the output
 * @parma plogctx 日志上下文结构体
 * @parms nlevel 日志输出等级
 * @param tag 日志输出标签
 * @param pfile 日志输出代码文件名称
 * @param line 日志输出代码位置（行数）
 * @param pfun 日志输出代码函数名称
 * @param fmt , ... 日志输出格式化字符串及其可变参数
 * @va_list val, 可变参数列表
 * @return 无
**********************************************************************************************************/
void log_format(struct log_ctx *plogctx, const int nlevel, const char *tag, const char *pfile, int line, const char *pfun, const char *fmt, va_list val);

/**********************************************************************************************************
 * log memory to the output
 * @parma plogctx 日志上下文结构体
 * @parms nlevel generate_header 日志输出等级
 * @param pmemory 输出内存指针
 * @param len 输出内存大小
 * @return 0为成功，否则为失败
 **********************************************************************************************************/
int log_memory(struct log_ctx *plogctx, const int nlevel, const char *pmemory, int len);

/**********************************************************************************************************
 * set log file chmod mode
 * @parma plogctx 日志上下文结构体
 * @parms mode log file chmod mode
 **********************************************************************************************************/
int set_log_file_chmod_mode(struct log_ctx *plogctx, int dir_mode, int file_mode);

/**********************************************************************************************************
 * force flush log to log file
 * @parma plogctx 日志上下文结构体
************************************************************************************************************/

void force_flush(struct log_ctx *plogctx);

/**********************************************************************************************************
 * 获取当前时区
 * @return 成功当前时区
 **********************************************************************************************************/
long get_time_zone();

/**********************************************************************************************************
 * 获取当前系统版本号
 * @parma psysver 当前系统版本信息接收buffer
 * @parms len	  buffer psysver的长度
 * @return 成功返回0，否则为失败
 **********************************************************************************************************/
int get_system_version(char *psysver, int len);

/**********************************************************************************************************
 * get current log path dir
 * @return 成功返回日志路径指针常量，失败返回NULL
**********************************************************************************************************/
const char *get_log_path();

/**********************************************************************************************************
 * 线程休眠函数
 * @parma 毫秒
 * @return 无
**********************************************************************************************************/
void lazysleep(uint32_t ms);

/**********************************************************************************************************
 * 线程休眠到固定时间
 * @parma 毫秒， ms_timestamp对应的时间
 * @return 无
**********************************************************************************************************/
bool lazysleepto(int64_t mstime);

/**********************************************************************************************************
 * 获取当前时间（精确到纳秒）
 * @return 当前时间（纳秒数）
**********************************************************************************************************/
uint64_t ns_timestamp();

/**********************************************************************************************************
 * 获取当前时间（精确到毫秒）
 * @return 当前时间（毫秒数）
**********************************************************************************************************/
uint64_t ms_timestamp();

/**********************************************************************************************************
 * 获取当前世界协调/格林威治时间（精确到毫秒）
 * @return 当前世界协调/格林威治时间（毫秒数）
**********************************************************************************************************/
int64_t utc_timestamp();

/**********************************************************************************************************
 * ffmpeg 日志输出回调函数钩子
 * @parma ptr 所有者指针
 * @parma level 日志等级
 * @parma fmt 日志格式化字符串
 * @parma vl 格式化字符串参数
 * @return 无
**********************************************************************************************************/
void ffmpeg_log_callback(void *ptr, int level, const char *fmt, va_list vl);

/**********************************************************************************************************
 * 日志输出回调函数钩子
 * @parma ptr 所有者指针
 * @parma level 日志等级
 * @parma fmt 日志格式化字符串
 * @parma vl 格式化字符串参数
 * @return 无
**********************************************************************************************************/
void default_log_callback(const void *ptr, int level, const char *fmt, va_list vl);

const char *get_last_error(int *error);

void show_call_stack();

extern log_ctx *g_plog_ctx;

// log operator micro define

#define lbinit_log(path, level, flag,  vmayjor, vminor, vmicro, vtiny)       g_plog_ctx = init_log(path, level, flag,  vmayjor, vminor, vmicro, vtiny)
#define lbdeinit_log()														 if(g_plog_ctx) {close_log_contex(&g_plog_ctx);}

// log output micro define
#define lbverb(fmt, ...)				log_trace(g_plog_ctx, LOG_LEVEL_VERB, NULL, __FILE__, __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__)
#define lbinfo(fmt, ...)				log_trace(g_plog_ctx, LOG_LEVEL_INFO, NULL, __FILE__, __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__)
#define lbtrace(fmt, ...)				log_trace(g_plog_ctx, LOG_LEVEL_TRACE, NULL, __FILE__, __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__)
#define lbwarn(fmt, ...)				log_trace(g_plog_ctx, LOG_LEVEL_WARN, NULL, __FILE__, __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__)
#define lberror(fmt, ...)				log_trace(g_plog_ctx, LOG_LEVEL_ERROR, NULL, __FILE__, __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__)
#define lbbuf(level, buf, size)         log_buf(g_plog_ctx, level, NULL, __FILE__, __LINE__, __FUNCTION__, buf, size);

#define lbcheck_result(ret, format, ...)	 if(0 != ret){ lberror("[%s:%d] check result failed, return ret:%d" format, __FUNCTION__, __LINE__, ret, ##__VA_ARGS__); return ret;}

#define lbcheck_break(ret, format, ...)	 if(0 != ret){ lberror("[%s:%d] check result failed, return ret:%d" format, __FUNCTION__, __LINE__, ret, ##__VA_ARGS__); break;}

#define lbcheck_ptr(ptr, ret, format, ...)	if(NULL == ptr) { lberror("[%s:%d] check ptr failed" format, __FUNCTION__, __LINE__, ##__VA_ARGS__); return ret;}

#define lbcheck_ptr_break(ptr, format, ...)	if(NULL == ptr) { lberror("[%s:%d] check ptr failed" format, __FUNCTION__, __LINE__, ##__VA_ARGS__); break;}

#define lbcheck_value(val, ret, format, ...)	if(!val) { lberror("[%s:%d] check value failed " format, __FUNCTION__, __LINE__, ##__VA_ARGS__); return ret;}

#define lbcheck_value_break(val, format, ...)	if(!val) { lberror("[%s:%d] check value failed " format, __FUNCTION__, __LINE__, ##__VA_ARGS__); break;}

#include <mutex>
#include <map>

#define lblogi(per, level, fmt, ...)                                                                            \
        {                                                                                                       \
            static std::mutex mt;                                                                               \
            static std::map<void*, int64_t>   mp;                                                               \
            std::lock_guard<std::mutex> lock(mt);                                                               \
            if(mp.find(this) == mp.end())                                                                       \
            {                                                                                                   \
                mp[this] = 0;                                                                                   \
            }                                                                                                   \
            if(per > 0 && 0 == mp[this]++%per)                                                                  \
            {                                                                                                   \
                log_trace(g_plog_ctx, level, NULL, __FILE__, __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__);        \
            }                                                                                                   \
        }

#define lbmemory(level, pmen, size)		log_memory(g_plog_ctx, level, pmen, size);

#define lbflush()                       force_flush(g_plog_ctx);

class LazyTimeOut
{
protected:
    uint64_t begin_time_;
    uint64_t timeout_;
    char *ptimeout_info;
public:
    LazyTimeOut(uint64_t timeout_ms, const char *func);
    LazyTimeOut(uint64_t timeout_ms, const char *func, const char *fmt, ...);

    ~LazyTimeOut();
};

#define SET_TIMEOUT(timeout_ms) LazyTimeOut timeout(timeout_ms, __FUNCTION__);
#define SET_FMT_TIMEOUT(timeout_ms, fmt, ...) LazyTimeOut fmt_timeout(timeout_ms, __FUNCTION__, fmt, ##__VA_ARGS__);

#define MSC_TRACE()

#endif