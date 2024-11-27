/***************************************************************************************************************
 * filename     lazylog.c/cpp
 * describe     write log to console and local disk file
 * author       Created by dawson on 2019/04/18
 * Copyright    @2007 - 2029 Sunvally. All Rights Reserved.
 ***************************************************************************************************************/

#ifdef __ANDROID__
#include <android/log.h>
#endif
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>
#include <time.h>      // for strcat()
#include <assert.h>   // assert();
//#include <io.h>
#include <sys/timeb.h>
#ifdef _MSC_VER
#include <windows.h>
#include <direct.h>
//#include  <winsock.h>
//winsock2.h
#include <io.h>
#define snprintf    sprintf_s
#ifndef MUTEX_LOCK_DEFITION
#define MUTEX_LOCK_DEFITION
#define sv_mutex                    CRITICAL_SECTION
#define init_mutex(px)              InitializeCriticalSection(px)
#define deinit_mutex(px)            DeleteCriticalSection(px)
#define lock_mutex(px)              EnterCriticalSection(px)
#define unlock_mutex(px)            LeaveCriticalSection(px)
#pragma warning(disable:4996)
#endif
#else
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>
#include <pthread.h>
#include <errno.h>
#include <unistd.h>
#include <dirent.h>
#include <execinfo.h>
#ifndef MUTEX_LOCK_DEFITION
#define MUTEX_LOCK_DEFITION
#define sv_mutex                    pthread_mutex_t
#define init_mutex(px)               pthread_mutexattr_t attr; \
        pthread_mutexattr_init(&attr); \
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE); \
        pthread_mutex_init(px, &attr);
//#define init_mutex(px)              pthread_mutex_init(px, NULL)
#define deinit_mutex(px)            pthread_mutex_destroy(px)
#define lock_mutex(px)              pthread_mutex_lock(px)
#define unlock_mutex(px)            pthread_mutex_unlock(px)
#endif
#endif
#include "lazylog.h"


//log level name
static const char *verb_name   = "verb";
static const char *info_name   = "info";
static const char *trace_name  = "trace";
static const char *warn_name   = "warn";
static const char *error_name  = "error";


#define MAX_LOG_SIZE 10240


/**
 * log context describe
 **/
struct log_ctx
{
    // mutex
    sv_mutex   *pmutex;

    // file descriptor
    FILE       *pfd;

    int64_t    llast_log_time;
    // log write size
    int64_t     llwrite_size;

    // log level
    int         nlog_level;

    // log output falg
    int         nlog_output_flag;

    // log write path
    char       *plog_path;
    char       *plog_dir;

    // log buffer
    char       *plog_buf;
    int         nlog_buf_size;

    // error info
    char       *perr_buf;
    int         nerr_buf_size;

    // version info
    int         nmayjor_version;
    int         nminor_version;
    int         nmicro_version;
    int         ntiny_version;

    int			ngmtime;
    int64_t	    nlog_file_rotation_size;
    int			nlog_rotation_index;
    bool        benable_log_function;
    int         nlog_dir_chmod_mode;
    int         nlog_file_chmod_mode;
};

struct log_ctx *g_plog_ctx = NULL;
struct log_ctx *open_log_contex(const int nlog_leve, int nlog_output_flag, const char *plog_path)
{
    if(NULL == plog_path && (LOG_OUTPUT_FLAG_FILE &nlog_output_flag))
    {
        printf("%s plog_path:%p, nlog_output_flag:%d, failed!\n", __FUNCTION__, plog_path, nlog_output_flag);
        return NULL;
    }

    // create log context
    struct log_ctx *plogctx = (struct log_ctx *)malloc(sizeof(struct log_ctx));
    assert(plogctx);
    memset(plogctx, 0, sizeof(struct log_ctx));
    plogctx->nlog_level        = nlog_leve;

    // create and init mutex
    plogctx->pmutex = (sv_mutex *)malloc(sizeof(sv_mutex));
    init_mutex(plogctx->pmutex);

    // create and copy log path
    plogctx->plog_path         = (char *)malloc(strlen(plog_path) + 1);
    strcpy(plogctx->plog_path, plog_path);
    plogctx->nlog_output_flag  = nlog_output_flag;

    // create log buffer
    plogctx->plog_buf          = (char *)malloc(MAX_LOG_SIZE);
    plogctx->nlog_buf_size     = MAX_LOG_SIZE;
    plogctx->ngmtime	       = 0;
    plogctx->nlog_file_rotation_size = 1024 * 1024 * 100;
    plogctx->nlog_rotation_index = 0;
    plogctx->benable_log_function = false;
    plogctx->llast_log_time = utc_timestamp();
    plogctx->nlog_dir_chmod_mode = DEFAULT_DIR_MODE;
    plogctx->nlog_file_chmod_mode = DEFAULT_FILE_MODE;
    //printf("%s end, plogctx:%p\n", __FUNCTION__, plogctx);
    return plogctx;
}

void close_log_contex(struct log_ctx **pplogctx)
{
    if (NULL == pplogctx || NULL == *pplogctx)
    {
        return;
    }

    struct log_ctx *plogctx = *pplogctx;
    // lock the mutex before close file
    lock_mutex(plogctx->pmutex);
    if (plogctx->pfd)
    {
        const char *end = "\n************************** program safety exit ****************************\n";
        fwrite(end, 1, strlen(end), plogctx->pfd);
        fclose(plogctx->pfd);
#ifndef _MSC_VER
            chmod(plogctx->plog_path, plogctx->nlog_file_chmod_mode);
#endif
        plogctx->pfd = NULL;
    }
    if(plogctx->plog_path)
    {
        free(plogctx->plog_path);
        plogctx->plog_path = NULL;
    }
    if(plogctx->plog_dir)
    {
        free(plogctx->plog_dir);
        plogctx->plog_dir = NULL;
    }
    if(plogctx->plog_buf)
    {
        free(plogctx->plog_buf);
        plogctx->plog_buf = NULL;
    }
    if(plogctx->perr_buf)
    {
        free(plogctx->perr_buf);
        plogctx->perr_buf = NULL;
    }
    unlock_mutex(plogctx->pmutex);

    // destroy mutex last
    if(plogctx->pmutex)
    {
        deinit_mutex(plogctx->pmutex);
        free(plogctx->pmutex);
        plogctx->pmutex = NULL;
    }
    free(plogctx);
    plogctx = NULL;
    *pplogctx = NULL;
}


int set_current_version(struct log_ctx *plogctx, const int nmayjor, const int nminor, const int nmicro, const int ntiny)
{
    if(NULL == plogctx)
    {
        return -1;
    }

    lock_mutex(plogctx->pmutex);
    plogctx->nmayjor_version   = nmayjor;
    plogctx->nminor_version    = nminor;
    plogctx->nmicro_version    = nmicro;
    plogctx->ntiny_version     = ntiny;
    unlock_mutex(plogctx->pmutex);

    return 0;
}

/**********************************************************************************************************
 * 创建目录（如果目录不存在）
 * @param pdir 目录路径
 * @return 无
 **********************************************************************************************************/
void mkdir_if_not_exist(const char *pdir, int mode)
{
    printf("mkdir_if_not_exist(pdir:%s, mode:%d)", pdir, mode);
#ifdef _MSC_VER
    if (_access(pdir, 0) != 0)
    {
        _mkdir(pdir);
    }
#else
    if (access(pdir, F_OK) != 0)
    {
        mkdir(pdir, mode);//S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    }
#endif
}

size_t gen_datetime(char *pdate, size_t len)
{
    struct tm *ptm;
    time_t rawtime;
    time(&rawtime);
    ptm = localtime(&rawtime);
    char datebuf[256] = { 0 };
    snprintf(datebuf, 256, "%04d%02d%02d-%02d%02d%02d", 1900 + ptm->tm_year, 1 + ptm->tm_mon, ptm->tm_mday, ptm->tm_hour, ptm->tm_min, ptm->tm_sec);
    size_t buf_len = strlen(datebuf) + 1;
    if (strlen(datebuf) < len)
    {
        memcpy(pdate, datebuf, buf_len);
        return buf_len;
    }

    return 0;
}
void convert_from_pattern(char *path, size_t len, const char *pattern)
{

    char *ptmp = strstr(path, pattern);
    if (ptmp)
    {
        char buff[1024] = { 0 };
        size_t offset = ptmp - path;
        memcpy(buff, path, offset);
        offset += gen_datetime(&buff[offset], 1024 - offset);
        //offset = strlen(buff);
        ptmp += strlen(pattern);
        //char *pdst = &buff[offset - 1];
        memcpy(&buff[offset - 1], ptmp, strlen(ptmp));
        if (strlen(buff) < len)
        {
            memcpy(path, buff, strlen(buff) + 1);
        }
        else
        {
        }
    }
}
/**********************************************************************************************************
 * init write file log in a simple way
 * @param plogdir 日志输出路径
 * @param nloglevel 日志等级
 * @param nlogoutputflag 日志输出标识, 输出到文件，控制台
 * @param nmayjor   程序主版本号，程序主体架构有很大的调整时（如重构）变更
 * @param nminor    程序次版本号，程序功能和结构迭代时变更
 * @param nmicro    程序微版本号, 程序修改一些小bug和小幅度的功能调整时变更
 * @param ntiny     程序极小版本号，程序日志增删、同一个类型bug的不同修改时变更
 * @return 日志上下文结构体
 **********************************************************************************************************/
struct log_ctx *init_log(const char *plogdir, const int nloglevel, const int output_mode, const int nmayjor, const int nminor, const int nmicro, const int ntiny)
//static log_ctx* init_file_log(const char* plogdir, const int nloglevel, const int nlogoutputflag, const int nmayjor, const int nminor, const int nmicro, const int ntiny)
{
#ifdef __ANDROID__
    __android_log_print(ANDROID_LOG_INFO, "lazytrace", "init_file_log(%s, %d, %d)\n", plogdir, nloglevel, nlogoutputflag);
#endif
    char logpath[256] = { 0 };
    char *plog_dir = NULL;
    if (plogdir && (output_mode &LOG_OUTPUT_FLAG_FILE))
    {
        //printf("if plogdir:%p\n", plogdir);
        strcpy(logpath, plogdir);
        char *ptmp = strrchr(logpath, '/');
        unsigned long len = ptmp - logpath;
        if (ptmp)
        {
            ptmp[0] = 0;
        }
        //mkdir_if_not_exist(logpath);
        plog_dir = (char *)malloc(strlen(plogdir) + 1);
        strcpy(plog_dir, plogdir);
        strcpy(logpath, plogdir);

        convert_from_pattern(logpath + len, 256 - len, "[datetime]");
    }

    struct log_ctx *plogctx = open_log_contex(nloglevel, output_mode, logpath);
    //printf("plogctx:%p = open_log_contex\n", plogctx);
    plogctx->plog_dir = plog_dir;
    printf("plogctx->plog_dir:%s", plogctx->plog_dir);
    //strcpy(plogctx->plog_dir, plogdir);
    if (nmayjor >= 0)
    {
        set_current_version(plogctx, nmayjor, nminor, nmicro, ntiny);
    }
    //printf("init_file_log end, plogctx:%p\n", plogctx);
    return plogctx;
}

struct log_ctx *init_log2(const char *plogdir, const int nloglevel, const int output_mode)
{
    return init_log(plogdir, nloglevel, output_mode, -1, -1, -1, -1);
}

void set_log_rotation_size(struct log_ctx *plogctx, int64_t rotation_size)
{
    if (plogctx)
    {
        plogctx->nlog_file_rotation_size = rotation_size;
    }
}
/**********************************************************************************************************
 * create log line header with code file name, line, function name, thread id, process id, level, context_id
 * @parma plogctx 日志上下文结构体
 * @param plogbuf 用于接收头部信息的buffer
 * @param ptag  输出日志标签信息
 * @param pfilepath 输出日志的代码文件名
 * @param line  输出日志的代码位置（行数）
 * @param pfun  输出日志的函数名称
 * @param nlevel 输出日志的等级
 * @param pheader_size 头部信息的buffer大小，头部创建成功后返回头部信息大小
 * @return 0为成功，否则为失败
 **********************************************************************************************************/
int generate_header(struct log_ctx *plogctx, char *plogbuf, const char *ptag,  const char *pfilepath, const int line, const char *pfun, const int nlevel, int *pheader_size)
{
    int log_header_size = -1;
    int log_buf_size;
    char loghdr[256] = {0};
    const char *pfilename = NULL;
    const char *plevelname = NULL;


    if(NULL == plogbuf || NULL == pheader_size)
    {
        return -1;
    }

    log_buf_size = *pheader_size;
    // clock time
    struct tm *ptm;
    time_t rawtime;
    time(&rawtime);

    //search the last '/' at pfile_path, return pos + 1
    if (pfilepath)
    {
        pfilename = strrchr(pfilepath, '/');
        if (NULL == pfilename)
        {
            pfilename = strrchr(pfilepath, '\\');
        }
        if(pfilename)
        {
            pfilename += 1;
        }
        else
        {
            pfilename = pfilepath;
        }
    }

    //printf("pfilename:%s, pfile_path:%s\n", pfilename, pfile_path);
    if(ptag)
    {
        sprintf(loghdr, "%s %s:%d", ptag, pfilename, line);
    }
    else
    {
        sprintf(loghdr, "%s:%d", pfilename, line);
    }

    if (plogctx->benable_log_function)
    {
        sprintf(loghdr + strlen(loghdr), " %s", pfun);
    }
    // use local time, if you want to use utc time, please use tm = gmtime(&tv.tv_sec)
    if(plogctx->ngmtime)
    {
        ptm = gmtime(&rawtime);
        //printf("tm:%p = gmtime(&tv.tv_sec)\n", tm);
        if(!ptm)
        {
            printf("tm:%p = gmtime(&tv.tv_sec) failed\n", ptm);
            return -1;
        }
        //printf("after tm = gmtime(&tv.tv_sec)\n");
    }
    else if ((ptm = localtime(&rawtime)) == NULL)
    {
        printf("ptm:%p = localtime(&tv.tv_sec) failed\n", ptm);
        return -1;
    }
    //printf("before switch nlevel:%d\n", nlevel);
    // from level to log name
    switch(nlevel)
    {
    case LOG_LEVEL_VERB:
        plevelname = verb_name;
        break;
    case LOG_LEVEL_INFO:
        plevelname = info_name;
        break;
    case LOG_LEVEL_TRACE:
        plevelname = trace_name;
        break;
    case LOG_LEVEL_WARN:
        plevelname = warn_name;
        break;
    case LOG_LEVEL_ERROR:
        plevelname = error_name;
        break;
    case LOG_LEVEL_DISABLE:
    default:
        return 0;
    }
    unsigned long int threadid = 0;
#ifdef _MSC_VER

    threadid = (unsigned long int)GetCurrentThreadId();//GetCurrentThread();
#else
    threadid = pthread_self();
#endif
    // write log header
    if (nlevel >= LOG_LEVEL_ERROR)
    {
        log_header_size = snprintf(plogbuf, log_buf_size,
                                   "[%d-%02d-%02d %02d:%02d:%02d.%03d][%s][%s][%lu][%d][%s] ",
                                   1900 + ptm->tm_year, 1 + ptm->tm_mon, ptm->tm_mday, ptm->tm_hour, ptm->tm_min, ptm->tm_sec, (int)(ms_timestamp() % 1000),
                                   plevelname, loghdr, threadid, errno, strerror(errno));
    }
    else
    {
        log_header_size = snprintf(plogbuf, log_buf_size,
                                   "[%d-%02d-%02d %02d:%02d:%02d.%03d][%s][%s][%lu]",
                                   1900 + ptm->tm_year, 1 + ptm->tm_mon, ptm->tm_mday, ptm->tm_hour, ptm->tm_min, ptm->tm_sec, (int)(ms_timestamp() % 1000),
                                   plevelname, loghdr, threadid);
    }

    if (log_header_size == -1)
    {
        return -1;
    }

    // write the header size.
    *pheader_size = (log_buf_size - 1) > log_header_size ? log_header_size : (log_buf_size - 1);
    //printf("generate_header end\n");
    return log_header_size;
}

/**********************************************************************************************************
 * write log to the output
 * @parma plogctx 日志上下文结构体
 * @param plogbuf 输出日志信息buffer
 * @param nloglen 输出日志信息长度
 * @return 0为成功，否则为失败
 **********************************************************************************************************/
int write_log(struct log_ctx *plogctx, const int nlevel, char *plogbuf, const size_t nloglen)
{
    size_t loglen = nloglen;
    if(NULL == plogctx || NULL == plogbuf || nloglen <= 0 || 0 == plogctx->nlog_output_flag)
    {
        return -1;
    }

    loglen = (size_t)(plogctx->nlog_buf_size - 1) > nloglen ? nloglen : (size_t)(plogctx->nlog_buf_size - 1);

    // add some to the end of char.
    if('\n' != plogbuf[loglen - 1])
    {
        plogbuf[loglen++] = '\n';
        plogbuf[loglen] = '\0';
    }
    plogctx->llwrite_size += loglen;
    size_t buf_len = strlen(plogbuf);
    assert(buf_len == loglen);
    // if not to file, to console and return.
    if (LOG_OUTPUT_FLAG_CONSOLE & plogctx->nlog_output_flag)
    {
        // if is error msg, then print color msg.
        // \033[31m : red text code in shell
        // \033[32m : green text code in shell
        // \033[33m : yellow text code in shell
        // \033[0m : normal text code
        if (nlevel <= LOG_LEVEL_INFO)
        {
#ifdef __ANDROID__
            __android_log_print(ANDROID_LOG_INFO, "lazytrace", "%s\n", plogbuf);
#else
            std::cout << plogbuf;
            //printf(plogbuf);
            //printf("%.*s", loglen, plogbuf);
#endif
        }
        else if (nlevel <= LOG_LEVEL_TRACE)
        {
#ifdef __ANDROID__
            __android_log_print(ANDROID_LOG_INFO, "lazytrace", "%s\n", plogbuf);
#else
            std::cout << plogbuf;
            //printf(plogbuf);
            //printf("%.*s", loglen, plogbuf);
#endif
        }
        else if (nlevel == LOG_LEVEL_WARN)
        {
#ifdef __ANDROID__
            __android_log_print(ANDROID_LOG_WARN, "lazytrace", "%s\n", plogbuf);
#else
            //printf(plogbuf);
            printf("\033[33m%.*s\033[0m", (int)loglen, plogbuf);
#endif
        }
        else
        {
#ifdef __ANDROID__
            __android_log_print(ANDROID_LOG_ERROR, "lazytrace", "%s\n", plogbuf);
#else
            printf("\033[31m%.*s\033[0m", (int)loglen, plogbuf);
#endif
        }
        //fflush(stdout);
    }

    if(plogctx->pfd && (LOG_OUTPUT_FLAG_FILE & plogctx->nlog_output_flag))
    {
        size_t writelen = fwrite(plogbuf, 1, loglen, plogctx->pfd);
        if((writelen > 0 && nlevel >= LOG_LEVEL_ERROR) || utc_timestamp() - plogctx->llast_log_time > 3000)
        {
            fflush(plogctx->pfd);
            plogctx->llast_log_time = utc_timestamp();
        }

        if (ftell(plogctx->pfd) >= plogctx->nlog_file_rotation_size)
        {
            char name[256];
            memset(name, 0, 256);
            fclose(plogctx->pfd);
#ifndef _MSC_VER
            chmod(plogctx->plog_path, plogctx->nlog_file_chmod_mode);
#endif
            size_t len = strlen(plogctx->plog_path);
            char *ptmp = strrchr(plogctx->plog_path, '.');
            if (ptmp)
            {
                len = ptmp - plogctx->plog_path;
            }
            memcpy(name, plogctx->plog_path, len);
            sprintf(name + strlen(name), "_%d.log", plogctx->nlog_rotation_index++);
            rename(plogctx->plog_path, name);
            plogctx->pfd = fopen(plogctx->plog_path, "ab");
        }
    }

    return 0;
}

/**********************************************************************************************************
 * get system version
 * @parma psysver buf to receive system version
 * @parms len buffer len
 * @return system version length if success, else fail
 **********************************************************************************************************/
int get_system_version(char *psysver, int len)
{
#ifdef _MSC_VER
    OSVERSIONINFO sVersionInfo;
    memset((BYTE *)&sVersionInfo, 0, sizeof(sVersionInfo));
    sVersionInfo.dwOSVersionInfoSize = sizeof(sVersionInfo);

    //获得计算机硬件信息
    BOOL ret = GetVersionEx(&sVersionInfo);
    sprintf_s(psysver, len, "%d.%d.%d", sVersionInfo.dwMajorVersion, sVersionInfo.dwMinorVersion, sVersionInfo.dwBuildNumber);
    return (int)strlen(psysver);
#else
    FILE *pfile = fopen("/proc/sys/kernel/version", "r");
    int readlen = 0;
    if(pfile)
    {
        readlen = (int)fread(psysver, 1, 80, pfile);
        fclose(pfile);
        pfile = NULL;
    }
    return readlen;
#endif

}

/**********************************************************************************************************
 * log to the output
 * @parma plogctx 日志上下文结构体
 * @parms nlevel 日志输出等级
 * @param tag 日志输出标签
 * @param pfile 日志输出代码文件名称
 * @param line 日志输出代码位置（行数）
 * @param pfun 日志输出代码函数名称
 * @param fmt , ... 日志输出格式化字符串及其可变参数
 * @return 0为成功，否则为失败
**********************************************************************************************************/
void log_trace(struct log_ctx *plogctx, const int nlevel, const char *tag, const char *pfile, int line, const char *pfun, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    // we reserved 1 bytes for the new line.
    log_format(plogctx, nlevel, tag, pfile, line, pfun, fmt, ap);
    va_end(ap);
}

void log_format(struct log_ctx *plogctx, const int nlevel, const char *tag, const char *pfile, int line, const char *pfun, const char *fmt, va_list val)
{
    int nlog_buf_size = 0;

    if (NULL == plogctx || plogctx->nlog_level > nlevel)
    {
        return ;
    }

    lock_mutex(plogctx->pmutex);
    nlog_buf_size = plogctx->nlog_buf_size;
    if (!generate_header(plogctx, plogctx->plog_buf, tag, pfile, line, pfun, nlevel, &nlog_buf_size))
    {
        unlock_mutex(plogctx->pmutex);
        return ;
    }

    // format log info string
    nlog_buf_size += vsprintf(plogctx->plog_buf + nlog_buf_size, fmt, val);

    if (NULL == plogctx->pfd && plogctx->plog_path && (LOG_OUTPUT_FLAG_FILE & plogctx->nlog_output_flag))
    {
        /*if(plogctx->plog_dir)
        {
            mkdir_if_not_exist(plogctx->plog_dir, plogctx->nlog_dir_chmod_mode);
        }*/
        plogctx->pfd = fopen(plogctx->plog_path, "ab");
        if(NULL == plogctx->pfd)
        {
            printf("create file:%s failed, errno:%d, reason:%s\n", plogctx->plog_path, errno, strerror(errno));
        }
        char loghdr[256] = { 0 };
        sprintf(loghdr, "version:%d.%d.%d.%d, timezone:%ld, build time:%s-%s, system version:", plogctx->nmayjor_version, plogctx->nminor_version, plogctx->nmicro_version, plogctx->ntiny_version, get_time_zone(), __DATE__, __TIME__);
        get_system_version(loghdr + strlen(loghdr), 256 - (int)strlen(loghdr));

        write_log(plogctx, nlevel, loghdr, (int)strlen(loghdr));
    }
    write_log(plogctx, nlevel, plogctx->plog_buf, nlog_buf_size);
    unlock_mutex(plogctx->pmutex);
}

void log_buf(struct log_ctx *plogctx, const int nlevel, const char *tag, const char *pfile, int line, const char *pfun, const char *pbuf, int size)
{
    int nlog_buf_offset = 0;

    if (NULL == plogctx || plogctx->nlog_level > nlevel)
    {
        return ;
    }

    lock_mutex(plogctx->pmutex);
    nlog_buf_offset = plogctx->nlog_buf_size;
    if (!generate_header(plogctx, plogctx->plog_buf, tag, pfile, line, pfun, nlevel, &nlog_buf_offset))
    {
        unlock_mutex(plogctx->pmutex);
        return ;
    }

    // format log info string
    if(plogctx->nlog_buf_size - nlog_buf_offset < size)
    {
        unlock_mutex(plogctx->pmutex);
        printf("(plogctx->nlog_buf_size:%d - nlog_buf_offset:%d):%d < size:%d\n", plogctx->nlog_buf_size, nlog_buf_offset, plogctx->nlog_buf_size - nlog_buf_offset, size);
        return ;
    }
    memcpy(plogctx->plog_buf + nlog_buf_offset, pbuf, size);
    nlog_buf_offset += size;
    //nlog_buf_size += vsprintf(plogctx->plog_buf + nlog_buf_size, fmt, val);

    if (NULL == plogctx->pfd && plogctx->plog_path && (LOG_OUTPUT_FLAG_FILE & plogctx->nlog_output_flag))
    {
        /*if(plogctx->plog_dir)
        {
            mkdir_if_not_exist(plogctx->plog_dir, plogctx->nlog_dir_chmod_mode);
        }*/

        //printf("open log file\n");
        plogctx->pfd = fopen(plogctx->plog_path, "ab");
        if(NULL == plogctx->pfd)
        {
            printf("create file:%s failed, errno:%d, reason:%s\n", plogctx->plog_path, errno, strerror(errno));
        }
        char loghdr[256] = { 0 };
        sprintf(loghdr, "version:%d.%d.%d.%d, timezone:%ld, build time:%s-%s, system version:", plogctx->nmayjor_version, plogctx->nminor_version, plogctx->nmicro_version, plogctx->ntiny_version, get_time_zone(), __DATE__, __TIME__);
        get_system_version(loghdr + strlen(loghdr), 256 - (int)strlen(loghdr));

        write_log(plogctx, nlevel, loghdr, (int)strlen(loghdr));
    }
    write_log(plogctx, nlevel, plogctx->plog_buf, nlog_buf_offset);
    unlock_mutex(plogctx->pmutex);
}

/**********************************************************************************************************
 * log memory to the output
 * @parma plogctx 日志上下文结构体
 * @parms nlevel generate_header 日志输出等级
 * @param pmemory 输出内存指针
 * @param len 输出内存大小
 * @return 0为成功，否则为失败
 **********************************************************************************************************/
int log_memory(struct log_ctx *plogctx, const int nlevel, const char *pmemory, int len)
{
    if(NULL == plogctx || plogctx->nlog_level > nlevel)
    {
        return 0;
    }
    lock_mutex(plogctx->pmutex);
    const unsigned char *psrc = (unsigned char *)pmemory;
    memset(plogctx->plog_buf, 0, plogctx->nlog_buf_size);
    sprintf(plogctx->plog_buf, "size:%d, memory:", len);
    for(int i = 0; i < len && (int)strlen(plogctx->plog_buf) < plogctx->nlog_buf_size; i++)
    {
        int val = *(psrc + i);
        sprintf(plogctx->plog_buf + strlen(plogctx->plog_buf), "%02x", val);
    }
    write_log(plogctx, nlevel, plogctx->plog_buf, (int)strlen(plogctx->plog_buf));
    unlock_mutex(plogctx->pmutex);
    return 0;
}

int set_log_file_chmod_mode(struct log_ctx *plogctx, int dir_mode, int file_mode)
{
    if(plogctx)
    {
        plogctx->nlog_dir_chmod_mode = dir_mode;
        plogctx->nlog_file_chmod_mode = file_mode;
        return 0;
    }

    return -1;
}

void force_flush(struct log_ctx *plogctx)
{
    if(NULL == plogctx)
    {
        return ;
    }

    fclose(plogctx->pfd);
    plogctx->pfd = NULL;
    plogctx->pfd = fopen(plogctx->plog_path, "a+");
}

/**********************************************************************************************************
 * get current system time in microsecond
 * @return system current time in microseconds
**********************************************************************************************************/
unsigned long get_sys_time()
{
    unsigned long curtime = 0;
#ifdef _MSC_VER
    return GetTickCount();
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    curtime = tv.tv_sec * 1000 + tv.tv_usec / 1000;
#endif
    return curtime;
}

long get_time_zone()
{
    long time_zone = 0;
#ifdef _MSC_VER
    time_t time_utc = 0;
    struct tm *p_tm_time;

    p_tm_time = localtime( &time_utc );   //转成当地时间
    time_zone = ( p_tm_time->tm_hour > 12 ) ?   ( p_tm_time->tm_hour -=  24 )  :  p_tm_time->tm_hour;
#else
    struct timeval tv;
    struct timezone tz;
    gettimeofday(&tv, &tz);
    time_zone = tz.tz_minuteswest / 60;
#endif

    return time_zone;
}

/**********************************************************************************************************
 * 获取日志路径
 * @return 成功返回日志路径指针常量，失败返回NULL
**********************************************************************************************************/
const char *get_log_path()
{
    if(!g_plog_ctx)
    {
        return NULL;
    }

    return g_plog_ctx->plog_dir;
}

/**********************************************************************************************************
 * ffmpeg 日志输出回调函数钩子
 * @parma ptr 所有者指针
 * @parma level 日志等级
 * @parma fmt 日志格式化字符串
 * @parma vl 格式化字符串参数
 * @return 无
**********************************************************************************************************/
void ffmpeg_log_callback(void *ptr, int level, const char *fmt, va_list vl)
{
    //int ff_level = level;
    if (level <= 16)//AV_LOG_ERROR
    {
        level = LOG_LEVEL_ERROR;
    }
    else if(level == 24)//AV_LOG_WARNING
    {
        level = LOG_LEVEL_WARN;
    }
    else if (32 == level /*|| 40 == level || 48 == level*/)
    {
        level = LOG_LEVEL_TRACE;
        //return;
    }
    else
    {
        return;
    }

    default_log_callback(ptr, level, fmt, vl);
}

void default_log_callback(const void *ptr, int level, const char *fmt, va_list vl)
{
    log_format(g_plog_ctx, level, NULL, NULL, -1, NULL, fmt, vl);
}

const char *get_last_error(int *error)
{
    if (g_plog_ctx)
    {
        if (NULL == g_plog_ctx->perr_buf)
        {
            g_plog_ctx->nerr_buf_size = 1024;
            g_plog_ctx->perr_buf = (char *)malloc(g_plog_ctx->nerr_buf_size);
        }
#ifdef _MSC_VER
        DWORD lasterror = GetLastError();
        if (error)
        {
            *error = lasterror;
        }

        if (lasterror)
        {
            LPVOID lpMsgBuf = NULL;
            DWORD bufLen = FormatMessageA(
                               FORMAT_MESSAGE_ALLOCATE_BUFFER |
                               FORMAT_MESSAGE_FROM_SYSTEM |
                               FORMAT_MESSAGE_IGNORE_INSERTS,
                               NULL,
                               lasterror,
                               MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                               (LPSTR)lpMsgBuf,
                               0, NULL );
            if (bufLen)
            {
                strcpy_s(g_plog_ctx->perr_buf, g_plog_ctx->nerr_buf_size, (const char *)lpMsgBuf);

                LocalFree(lpMsgBuf);

                return g_plog_ctx->perr_buf;
            }
            strcpy_s(g_plog_ctx->perr_buf, g_plog_ctx->nerr_buf_size, "unknown error");
        }
        else
        {
            strcpy_s(g_plog_ctx->perr_buf, g_plog_ctx->nerr_buf_size, "success");
        }
        return g_plog_ctx->perr_buf;

#else
        sprintf(g_plog_ctx->perr_buf, "errno:%d, errnostr:%s\n", errno, strerror(errno));
        if (error)
        {
            *error = errno;
        }
        return g_plog_ctx->perr_buf;
#endif
    }
    return NULL;
}

void lazysleep(uint32_t ms)
{
#ifdef _MSC_VER
    Sleep(ms);
#else
    usleep(ms * 1000);
#endif
}

bool lazysleepto(int64_t mstime)
{
    int64_t cur_ms_time = ms_timestamp();
    if (cur_ms_time > mstime)
    {
        return false;
    }

    int64_t milliseconds = (int64_t)(mstime - cur_ms_time);
    if (milliseconds > 1)
    {
        if (milliseconds > 10000)
        {
            lbtrace("tried to sleep for %u seconds, that can't be right! Triggering breakpoint.\n", milliseconds);
            assert(0);
        }
        lazysleep(milliseconds);
    }

    for (;;)
    {
        int64_t cur_ms = ms_timestamp();
        if (cur_ms >= mstime)
        {
            return true;
        }
        else
        {
            int64_t dur = mstime - cur_ms;
            lazysleep(dur);
        }
    }
}

uint64_t ns_timestamp()
{
    uint64_t curtime = 0;
#ifdef _MSC_VER
    curtime = GetTickCount64() * 1000000;
#else
    struct timespec time;
    clock_gettime(CLOCK_REALTIME, &time);
    curtime = time.tv_sec * 1000000000 + time.tv_nsec;
#endif

    return curtime;
}

uint64_t ms_timestamp()
{
    struct timeb tb;
    ftime(&tb);
    return tb.time * 1000 + tb.millitm;
}

int64_t utc_timestamp()
{
    time_t now = time(NULL);
    tm *utc_time = gmtime(&now);
    int64_t utc_time_stamp = mktime(utc_time);

    struct timeb tb;
    ftime(&tb);
    utc_time_stamp = utc_time_stamp * 1000 + tb.millitm;
    return utc_time_stamp;
}
#ifndef _MSC_VER
void show_call_stack()
{
    const int size = 100;
    void *buf[size];
    char **strings;
    char stack_buffer[1024];
    int nptrs = backtrace(buf, size);
    lbinfo("%d function stacks call", nptrs);
    strings = backtrace_symbols(buf, nptrs);
    if(NULL == strings)
    {
        lbwarn("strings:%p = backtrace_symbols(buf, nptrs:%d) faied", strings, nptrs);
        return;
    }

    for(int i = 0; i < nptrs; i++)
    {
        int len = sprintf(stack_buffer, "%s\n", strings[i]);
        write_log(g_plog_ctx, LOG_LEVEL_TRACE, stack_buffer, len);
    }
    free(strings);
}
#endif
LazyTimeOut::LazyTimeOut(uint64_t timeout_ms, const char *func)
{
    begin_time_ = ms_timestamp();
    timeout_ = timeout_ms;
    ptimeout_info = NULL;
    if (func)
    {
        size_t len = strlen(func) + 1;
        ptimeout_info = (char *)malloc(len);
        memcpy(ptimeout_info, func, len);
    }

}

LazyTimeOut::LazyTimeOut(uint64_t timeout_ms, const char *func, const char *fmt, ...)
{
    begin_time_ = ms_timestamp();
    timeout_ = timeout_ms;
    ptimeout_info = NULL;
    char buf[2048];
    int offset = 0;
    // format log info string
    offset = sprintf(buf, "%s ", func);
    va_list ap;
    va_start(ap, fmt);
    // we reserved 1 bytes for the new line.
    offset += vsnprintf(buf + offset, 2048 - offset, fmt, ap);
    va_end(ap);
    ptimeout_info = (char *)malloc(offset + 1);
    memcpy(ptimeout_info, buf, offset + 1);

}
LazyTimeOut::~LazyTimeOut()
{
    if (ms_timestamp() - begin_time_ >= timeout_)
    {
        lbwarn("%s timeout:%llu\n", ptimeout_info, ms_timestamp() - begin_time_);
    }

    if (ptimeout_info)
    {
        free(ptimeout_info);
    }
}
