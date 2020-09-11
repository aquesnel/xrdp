/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2014
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <syslog.h>
#include <stdarg.h>
#include <stdio.h>
#include <time.h>
#include "list.h"
#include "file.h"
#include "os_calls.h"
#include "thread_calls.h"

/* Add a define here so that the log.h will hold more information
 * when compiled from this C file.
 * When compiled normally the log.h file only contain the public parts
 * of the operators in this file. */
#define LOGINTERNALSTUFF
#include "log.h"

/* Here we store the current state and configuration of the log */
static struct log_config *g_staticLogConfig = NULL;

/* This file first start with all private functions.
   In the end of the file the public functions is defined */

/**
 *
 * @brief Opens log file
 * @param fname log file name
 * @return see open(2) return values
 *
 */
int
internal_log_file_open(const char *fname)
{
    int ret = -1;

    if (fname != NULL)
    {
        ret = open(fname, O_WRONLY | O_CREAT | O_APPEND | O_SYNC,
                   S_IRUSR | S_IWUSR);
    }

#ifdef FD_CLOEXEC
    if (ret != -1)
    {
        fcntl(ret, F_SETFD, FD_CLOEXEC);
    }
#endif

    return ret;
}

/**
 *
 * @brief Converts xrdp log level to syslog logging level
 * @param xrdp logging level
 * @return syslog equivalent logging level
 *
 */
int
internal_log_xrdp2syslog(const enum logLevels lvl)
{
    switch (lvl)
    {
        case LOG_LEVEL_ALWAYS:
            return LOG_CRIT;
        case LOG_LEVEL_ERROR:
            return LOG_ERR;
        case LOG_LEVEL_WARNING:
            return LOG_WARNING;
        case LOG_LEVEL_INFO:
            return LOG_INFO;
        case LOG_LEVEL_DEBUG:
        case LOG_LEVEL_TRACE:
            return LOG_DEBUG;
        default:
            g_writeln("Undefined log level - programming error");
            return LOG_DEBUG;
    }
}

/**
 * @brief Converts xrdp log levels to textual logging levels
 * @param lvl logging level
 * @param str pointer to a string, must be allocated before
 * @return The log string in str pointer.
 *
 */
void
internal_log_lvl2str(const enum logLevels lvl, char *str)
{
    switch (lvl)
    {
        case LOG_LEVEL_ALWAYS:
            snprintf(str, 9, "%s", "[CORE ] ");
            break;
        case LOG_LEVEL_ERROR:
            snprintf(str, 9, "%s", "[ERROR] ");
            break;
        case LOG_LEVEL_WARNING:
            snprintf(str, 9, "%s", "[WARN ] ");
            break;
        case LOG_LEVEL_INFO:
            snprintf(str, 9, "%s", "[INFO ] ");
            break;
        case LOG_LEVEL_DEBUG:
            snprintf(str, 9, "%s", "[DEBUG] ");
            break;
        case LOG_LEVEL_TRACE:
            snprintf(str, 9, "%s", "[TRACE] ");
            break;
        default:
            snprintf(str, 9, "%s", "PRG ERR!");
            g_writeln("Programming error - undefined log level!!!");
    }
}

/******************************************************************************/
enum logReturns
internal_log_start(struct log_config *l_cfg)
{
    enum logReturns ret = LOG_GENERAL_ERROR;

    if (0 == l_cfg)
    {
        ret = LOG_ERROR_MALLOC;
        return ret;
    }

    /* if logfile is NULL, we return error */
    if (0 == l_cfg->log_file)
    {
        g_writeln("log_file not properly assigned");
        return ret;
    }

    /* if progname is NULL, we return error */
    if (0 == l_cfg->program_name)
    {
        g_writeln("program_name not properly assigned");
        return ret;
    }

    /* open file */
    l_cfg->fd = internal_log_file_open(l_cfg->log_file);

    if (-1 == l_cfg->fd)
    {
        return LOG_ERROR_FILE_OPEN;
    }

    /* if syslog is enabled, open it */
    if (l_cfg->enable_syslog)
    {
        openlog(l_cfg->program_name, LOG_CONS | LOG_PID, LOG_DAEMON);
    }

#ifdef LOG_ENABLE_THREAD
    pthread_mutexattr_init(&(l_cfg->log_lock_attr));
    pthread_mutex_init(&(l_cfg->log_lock), &(l_cfg->log_lock_attr));
#endif

    return LOG_STARTUP_OK;
}

/******************************************************************************/
enum logReturns
internal_log_end(struct log_config *l_cfg)
{
    enum logReturns ret = LOG_GENERAL_ERROR;

    /* if log is closed, quit silently */
    if (0 == l_cfg)
    {
        return ret;
    }

    if (-1 != l_cfg->fd)
    {
        /* closing logfile... */
        g_file_close(l_cfg->fd);
    }

    /* if syslog is enabled, close it */
    if (l_cfg->enable_syslog)
    {
        closelog();
    }

    /* freeing allocated memory */
    if (0 != l_cfg->log_file)
    {
        g_free(l_cfg->log_file);
        l_cfg->log_file = 0;
    }

    ret = LOG_STARTUP_OK;
    return ret;
}

/**
 * Converts a string representing th log level to a value
 * @param buf
 * @return
 */
enum logLevels
internal_log_text2level(const char *buf)
{
    if (0 == g_strcasecmp(buf, "0") ||
            0 == g_strcasecmp(buf, "core"))
    {
        return LOG_LEVEL_ALWAYS;
    }
    else if (0 == g_strcasecmp(buf, "1") ||
             0 == g_strcasecmp(buf, "error"))
    {
        return LOG_LEVEL_ERROR;
    }
    else if (0 == g_strcasecmp(buf, "2") ||
             0 == g_strcasecmp(buf, "warn") ||
             0 == g_strcasecmp(buf, "warning"))
    {
        return LOG_LEVEL_WARNING;
    }
    else if (0 == g_strcasecmp(buf, "3") ||
             0 == g_strcasecmp(buf, "info"))
    {
        return LOG_LEVEL_INFO;
    }
    else if (0 == g_strcasecmp(buf, "4") ||
             0 == g_strcasecmp(buf, "debug"))
    {
        return LOG_LEVEL_DEBUG;
    }
    else if (0 == g_strcasecmp(buf, "5") ||
             0 == g_strcasecmp(buf, "trace"))
    {
        return LOG_LEVEL_TRACE;
    }

    g_writeln("Your configured log level is corrupt - we use debug log level");
    return LOG_LEVEL_DEBUG;
}

enum logReturns
internalReadConfiguration(const char *inFilename, const char *applicationName)
{
    int fd;
    enum logReturns ret = LOG_GENERAL_ERROR;
    struct list *param_n;
    struct list *param_v;

    if (inFilename == NULL)
    {
        g_writeln("The inifile is null to readConfiguration!");
        return ret;
    }

    fd = g_file_open(inFilename);

    if (-1 == fd)
    {
        ret = LOG_ERROR_NO_CFG;
        g_writeln("We could not open the configuration file to read log parameters");
        return ret;
    }

    /* we initialize the memory for the configuration and set all content
       to zero. */
    ret = internalInitAndAllocStruct();

    if (ret != LOG_STARTUP_OK)
    {
        g_file_close(fd);
        return ret;
    }

    param_n = list_create();
    param_n->auto_free = 1;
    param_v = list_create();
    param_v->auto_free = 1;

    /* read logging config */
    ret = internal_config_read_logging(fd, g_staticLogConfig, param_n,
                                       param_v, applicationName);

    /* cleanup */
    list_delete(param_v);
    list_delete(param_n);
    g_file_close(fd);
    return ret;
}

/******************************************************************************/
enum logReturns
internal_config_read_logging(int file, struct log_config *lc,
                             struct list *param_n,
                             struct list *param_v,
                             const char *applicationName)
{
    int i;
    char *buf;
    char *temp_buf;
    char str_level[20];
    int len;
    struct log_logger_level* logger;

    list_clear(param_v);
    list_clear(param_n);

    /* setting defaults */
    lc->program_name = applicationName;
    lc->log_file = 0;
    lc->fd = -1;
    lc->log_level = LOG_LEVEL_DEBUG;
    lc->enable_console = 1;
    lc->console_level = LOG_LEVEL_DEBUG;
    lc->enable_syslog = 0;
    lc->syslog_level = LOG_LEVEL_DEBUG;
    lc->enable_pid = 0;

    file_read_section(file, SESMAN_CFG_LOGGING, param_n, param_v);

    for (i = 0; i < param_n->count; i++)
    {
        buf = (char *)list_get_item(param_n, i);

        if (0 == g_strcasecmp(buf, SESMAN_CFG_LOG_FILE))
        {
            lc->log_file = g_strdup((char *)list_get_item(param_v, i));

            if (lc->log_file != NULL)
            {
                if (lc->log_file[0] != '/')
                {
                    temp_buf = (char *)g_malloc(512, 0);
                    g_snprintf(temp_buf, 511, "%s/%s", XRDP_LOG_PATH, lc->log_file);
                    g_free(lc->log_file);
                    lc->log_file = temp_buf;
                }
            }
        }

        if (0 == g_strcasecmp(buf, SESMAN_CFG_LOG_LEVEL))
        {
            lc->log_level = internal_log_text2level((char *)list_get_item(param_v, i));
        }

        if (0 == g_strcasecmp(buf, SESMAN_CFG_LOG_ENABLE_SYSLOG))
        {
            lc->enable_syslog = g_text2bool((char *)list_get_item(param_v, i));
        }

        if (0 == g_strcasecmp(buf, SESMAN_CFG_LOG_SYSLOG_LEVEL))
        {
            lc->syslog_level = internal_log_text2level((char *)list_get_item(param_v, i));
        }
        
        if (0 == g_strcasecmp(buf, SESMAN_CFG_LOG_ENABLE_CONSOLE))
        {
            lc->enable_console = g_text2bool((char *)list_get_item(param_v, i));
        }
        
        if (0 == g_strcasecmp(buf, SESMAN_CFG_LOG_CONSOLE_LEVEL))
        {
            lc->console_level = internal_log_text2level((char *)list_get_item(param_v, i));
        }
        
        if (0 == g_strcasecmp(buf, SESMAN_CFG_LOG_ENABLE_PID))
        {
            lc->enable_pid = g_text2bool((char *)list_get_item(param_v, i));
        }
    }

    if (0 == lc->log_file)
    {
        lc->log_file = g_strdup("./sesman.log");
    }

    /* try to create path if not exist */
    g_create_path(lc->log_file);

    g_printf("logging configuration:\r\n");
    internal_log_lvl2str(lc->log_level, str_level);
    g_printf("\tLogFile:       %s\r\n", lc->log_file);
    g_printf("\tLogLevel:      %s\r\n", str_level);
    
    internal_log_lvl2str(lc->console_level, str_level);
    g_printf("\tEnableConsole: %s\r\n", (lc->enable_console ? "true" : "false"));
    g_printf("\tConsoleLevel:  %s\r\n", str_level);
    
    internal_log_lvl2str(lc->syslog_level, str_level);
    g_printf("\tEnableSyslog:  %s\r\n", (lc->enable_syslog ? "true" : "false"));
    g_printf("\tSyslogLevel:   %s\r\n", str_level);
    
    g_printf("per logger configuration:\r\n");
    list_clear(param_v);
    list_clear(param_n);
    file_read_section(file, SESMAN_CFG_LOGGING_LOGGER, param_n, param_v);
    for (i = 0; i < param_n->count; i++)
    {
        logger = (struct log_logger_level*)g_malloc(sizeof(struct log_logger_level), 1);
        list_add_item(lc->per_logger_level, (tbus) logger);
        logger->log_level = internal_log_text2level((char *)list_get_item(param_v, i));
        internal_log_lvl2str(logger->log_level, str_level);
        
        g_strncpy(logger->logger_name, (char *)list_get_item(param_n, i), LOGGER_NAME_SIZE);
        logger->logger_name[LOGGER_NAME_SIZE] = '\0';
        len = g_strlen(logger->logger_name);
        if(len >= 2 
                && logger->logger_name[len-2] == '(' 
                && logger->logger_name[len-1] == ')' )
        {
            g_printf("\t%-*s: %s\r\n", LOGGER_NAME_SIZE, logger->logger_name, str_level);
            logger->logger_type = LOG_TYPE_FUNCTION;
            logger->logger_name[len-2] = '\0';
        }
        else
        {
            g_printf("\t%-*s: %s\r\n", LOGGER_NAME_SIZE, logger->logger_name, str_level);
            logger->logger_type = LOG_TYPE_FILE;
        }
    }
    if(param_n->count == 0)
    {
        g_printf("\tNone\r\n");
    }

    return LOG_STARTUP_OK;
}

enum logReturns
internalInitAndAllocStruct(void)
{
    enum logReturns ret = LOG_GENERAL_ERROR;
    g_staticLogConfig = g_new0(struct log_config, 1);

    if (g_staticLogConfig != NULL)
    {
        g_staticLogConfig->fd = -1;
        g_staticLogConfig->enable_syslog = 0;
        g_staticLogConfig->per_logger_level = list_create();
        if (g_staticLogConfig->per_logger_level != NULL)
        {
            g_staticLogConfig->per_logger_level->auto_free = 1;
            ret = LOG_STARTUP_OK;
        }
        else
        {
            g_writeln("could not allocate memory for log struct");
            g_free(g_staticLogConfig);
            ret = LOG_ERROR_MALLOC;
        }
    }
    else
    {
        g_writeln("could not allocate memory for log struct");
        ret = LOG_ERROR_MALLOC;
    }

    return ret;
}

enum logReturns
internal_free_log_struct(void)
{
    if (g_staticLogConfig != NULL)
    {
        if (g_staticLogConfig->per_logger_level != NULL)
        {
            list_delete(g_staticLogConfig->per_logger_level);
            g_staticLogConfig->per_logger_level = NULL;
        }
        g_free(g_staticLogConfig);
        g_staticLogConfig = NULL;
    }

    return LOG_STARTUP_OK;
}

/*
 * Here below the public functions
 */

enum logReturns
log_start_from_param(const struct log_config *iniParams)
{
    enum logReturns ret = LOG_GENERAL_ERROR;

    if (g_staticLogConfig != NULL)
    {
        log_message(LOG_LEVEL_ALWAYS, "Log already initialized");
        return ret;
    }

    if (iniParams == NULL)
    {
        g_writeln("inparam to log_start_from_param is NULL");
        return ret;
    }
    else
    {
        /*Copy the struct information*/
        ret = internalInitAndAllocStruct();

        if (ret != LOG_STARTUP_OK)
        {
            g_writeln("internalInitAndAllocStruct failed");
            return ret;
        }

        g_staticLogConfig->enable_syslog = iniParams->enable_syslog;
        g_staticLogConfig->fd = iniParams->fd;
        g_staticLogConfig->log_file = g_strdup(iniParams->log_file);
        g_staticLogConfig->log_level = iniParams->log_level;
        g_staticLogConfig->log_lock = iniParams->log_lock;
        g_staticLogConfig->log_lock_attr = iniParams->log_lock_attr;
        g_staticLogConfig->program_name = iniParams->program_name;
        g_staticLogConfig->syslog_level = iniParams->syslog_level;
        ret = internal_log_start(g_staticLogConfig);

        if (ret != LOG_STARTUP_OK)
        {
            g_writeln("Could not start log");

            internal_free_log_struct();
        }
    }

    return ret;
}

/**
 * This function initialize the log facilities according to the configuration
 * file, that is described by the in parameter.
 * @param iniFile
 * @param applicationName, the name that is used in the log for the running application
 * @return 0 on success
 */
enum logReturns
log_start(const char *iniFile, const char *applicationName)
{
    enum logReturns ret = LOG_GENERAL_ERROR;

    if (applicationName == NULL)
    {
        g_writeln("Programming error your application name cannot be null");
        return ret;
    }

    ret = internalReadConfiguration(iniFile, applicationName);

    if (ret == LOG_STARTUP_OK)
    {
        ret = internal_log_start(g_staticLogConfig);

        if (ret != LOG_STARTUP_OK)
        {
            g_writeln("Could not start log");
            internal_free_log_struct();
        }
    }
    else
    {
        g_writeln("Error reading configuration for log based on config: %s",
                  iniFile);
    }

    return ret;
}

/**
 * Function that terminates all logging
 * @return
 */
enum logReturns
log_end(void)
{
    enum logReturns ret = LOG_GENERAL_ERROR;
    ret = internal_log_end(g_staticLogConfig);
    internal_free_log_struct();

    return ret;
}

enum logReturns
log_message_with_location(const char *function_name, 
                          const char *file_name, 
                          const int line_number, 
                          const enum logLevels level, 
                          const char *msg, 
                          ...)
{
    va_list ap;
    enum logReturns rv;
    char buff[LOG_BUFFER_SIZE];
    struct log_logger_level *logger;
    int i;
    bool_t force_log = 0;
    
    if (g_staticLogConfig == NULL)
    {
        g_writeln("The log reference is NULL - log not initialized properly "
                  "when called from [%s(%s:%d)]", 
                  function_name, file_name, line_number);
        return LOG_ERROR_NO_CFG;
    }
    for (i = 0; i < g_staticLogConfig->per_logger_level->count; i++)
    {
        logger = (struct log_logger_level *)list_get_item(g_staticLogConfig->per_logger_level, i);

        if ((logger->logger_type == LOG_TYPE_FILE
                && 0 == g_strncmp(logger->logger_name, file_name, LOGGER_NAME_SIZE))
            || (logger->logger_type == LOG_TYPE_FUNCTION
                && 0 == g_strncmp(logger->logger_name, function_name, LOGGER_NAME_SIZE)))
        {
            if(logger->log_level < level)
            {
                return LOG_STARTUP_OK;
            }
            force_log = 1;
            break;
        }
    }
    
    g_snprintf(buff, LOG_BUFFER_SIZE, "[%s(%s:%d)] %s", 
               function_name, file_name, line_number, msg);

    va_start(ap, msg);
    rv = internal_log_message(level, force_log, buff, ap);
    va_end(ap);
    return rv;
}

enum logReturns
log_message(const enum logLevels lvl, const char *msg, ...)
{
    va_list ap;
    enum logReturns rv;
    
    va_start(ap, msg);
    rv = internal_log_message(lvl, 0, msg, ap);
    va_end(ap);
    return rv;
}

enum logReturns
internal_log_message(const enum logLevels lvl, bool_t force_log, const char *msg, va_list ap)
{
    char buff[LOG_BUFFER_SIZE + 31]; /* 19 (datetime) 4 (space+cr+lf+\0) */
    int len = 0;
    enum logReturns rv = LOG_STARTUP_OK;
    int writereply = 0;
    time_t now_t;
    struct tm *now;

    if (g_staticLogConfig == NULL)
    {
        g_writeln("The log reference is NULL - log not initialized properly");
        return LOG_ERROR_NO_CFG;
    }

    if (0 > g_staticLogConfig->fd 
            && g_staticLogConfig->enable_syslog == 0
            && g_staticLogConfig->enable_console == 0)
    {
        return LOG_ERROR_FILE_NOT_OPEN;
    }

    if (!((g_staticLogConfig->fd >= 0 && (force_log || lvl <= g_staticLogConfig->log_level))
            || (g_staticLogConfig->enable_syslog && (force_log || lvl <= g_staticLogConfig->syslog_level))
            || (g_staticLogConfig->enable_console && (force_log || lvl <= g_staticLogConfig->console_level))))
    {
        return LOG_STARTUP_OK;
    }

    now_t = time(&now_t);
    now = localtime(&now_t);

    strftime(buff, 21, "[%Y%m%d-%H:%M:%S] ", now);

    internal_log_lvl2str(lvl, buff + 20);

    if (g_staticLogConfig->enable_pid)
    {
        g_snprintf(buff + 28, LOG_BUFFER_SIZE, "[pid:%d tid:%lld] ", 
                   g_getpid(), (long long) tc_get_threadid());
        len = g_strlen(buff + 28);
    }
    len += vsnprintf(buff + 28 + len, LOG_BUFFER_SIZE - len, msg, ap);

    /* checking for truncated messages */
    if (len > LOG_BUFFER_SIZE)
    {
        log_message(LOG_LEVEL_WARNING, "next message will be truncated");
        len = LOG_BUFFER_SIZE;
    }

    /* forcing the end of message string */
#ifdef _WIN32
    buff[len + 28] = '\r';
    buff[len + 29] = '\n';
    buff[len + 30] = '\0';
#else
#ifdef _MACOS
    buff[len + 28] = '\r';
    buff[len + 29] = '\0';
#else
    buff[len + 28] = '\n';
    buff[len + 29] = '\0';
#endif
#endif

    if (g_staticLogConfig->enable_syslog && (force_log || lvl <= g_staticLogConfig->syslog_level))
    {
        /* log to syslog*/
        /* %s fix compiler warning 'not a string literal' */
        syslog(internal_log_xrdp2syslog(lvl), "%s", buff + 20);
    }

    if (g_staticLogConfig->enable_console && (force_log || lvl <= g_staticLogConfig->console_level))
    {
        /* log to console */
        g_printf("%s", buff);
    }

    if (force_log || lvl <= g_staticLogConfig->log_level)
    {
        /* log to application logfile */
#ifdef LOG_ENABLE_THREAD
        pthread_mutex_lock(&(g_staticLogConfig->log_lock));
#endif

        if (g_staticLogConfig->fd >= 0)
        {
            writereply = g_file_write(g_staticLogConfig->fd, buff, g_strlen(buff));

            if (writereply <= 0)
            {
                rv = LOG_ERROR_NULL_FILE;
            }
        }

#ifdef LOG_ENABLE_THREAD
        pthread_mutex_unlock(&(g_staticLogConfig->log_lock));
#endif
    }

    return rv;
}

/**
 * Return the configured log file name
 * @return
 */
char *
getLogFile(char *replybuf, int bufsize)
{
    if (g_staticLogConfig)
    {
        if (g_staticLogConfig->log_file)
        {
            g_strncpy(replybuf, g_staticLogConfig->log_file, bufsize);
        }
        else
        {
            g_sprintf(replybuf, "The log_file name is NULL");
        }
    }
    else
    {
        g_snprintf(replybuf, bufsize, "The log is not properly started");
    }

    return replybuf;
}
