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

#ifndef LOG_H
#define LOG_H

#include <pthread.h>

#include "arch.h"
#include "list.h"

/* logging buffer size */
#define LOG_BUFFER_SIZE      1024
#define LOGGER_NAME_SIZE     25

/* logging levels */
enum logLevels
{
    LOG_LEVEL_ALWAYS = 0,
    LOG_LEVEL_ERROR,     /* for describing non-recoverable error states in a request or method */
    LOG_LEVEL_WARNING,   /* for describing recoverable error states in a request or method */
    LOG_LEVEL_INFO,      /* for low verbosity and high level descriptions of normal operations */
    LOG_LEVEL_DEBUG,     /* for medium verbosity and low level descriptions of normal operations */
    LOG_LEVEL_TRACE      /* for high verbosity and low level descriptions of normal operations (eg. method or wire tracing) */
};

/* startup return values */
enum logReturns
{
    LOG_STARTUP_OK = 0,
    LOG_ERROR_MALLOC,
    LOG_ERROR_NULL_FILE,
    LOG_ERROR_FILE_OPEN,
    LOG_ERROR_NO_CFG,
    LOG_ERROR_FILE_NOT_OPEN,
    LOG_GENERAL_ERROR
};

#define SESMAN_CFG_LOGGING            "Logging"
#define SESMAN_CFG_LOGGING_LOGGER     "Logging_PerLogger"
#define SESMAN_CFG_LOG_FILE           "LogFile"
#define SESMAN_CFG_LOG_LEVEL          "LogLevel"
#define SESMAN_CFG_LOG_ENABLE_CONSOLE "EnableConsole"
#define SESMAN_CFG_LOG_CONSOLE_LEVEL  "ConsoleLevel"
#define SESMAN_CFG_LOG_ENABLE_SYSLOG  "EnableSyslog"
#define SESMAN_CFG_LOG_SYSLOG_LEVEL   "SyslogLevel"
#define SESMAN_CFG_LOG_ENABLE_PID     "EnableProcessId"

/* enable threading */
/*#define LOG_ENABLE_THREAD*/

#ifdef XRDP_DEBUG

/**
 * @brief Logging macro for messages that are for an XRDP developper to understand and 
 * debug XRDP code.
 * 
 * Note: all log levels are relavant to help a developper understand XRDP at 
 *      different levels of granularity.
 * 
 * Note: the logging function calls are removed when XRDP_DEBUG is NOT defined.
 * 
 * Note: when the build is configured with --disable-xrdpdebug, then 
 *      "#define XRDP_DEBUG" can temporarily be added to any c file to
 *      add targeted developper logging in that compilation unit.
 * 
 * @param lvl, the log level
 * @param msg, the log text as a printf format c-string
 * @param ... the arguments for the printf format c-string
 */
#define LOG_DEVEL(log_level, args...) \
        log_message_with_location(__func__, __FILE__, __LINE__, log_level, args);

/**
 * @brief Logging macro for messages that are for a systeam administrator to
 * configure and run XRDP on their machine.
 * 
 * Note: the logging function calls contain additional code location info when 
 *      XRDP_DEBUG is defined.
 * 
 * @param lvl, the log level
 * @param msg, the log text as a printf format c-string
 * @param ... the arguments for the printf format c-string
 */
#define LOG(log_level, args...) \
        log_message_with_location(__func__, __FILE__, __LINE__, log_level, args);
#else
#define LOG_DEVEL(log_level, args...)
#define LOG(log_level, args...) log_message(log_level, args);
#endif

struct log_logger_level
{
    enum logLevels log_level;
    char logger_name[LOGGER_NAME_SIZE + 1];
};

struct log_config
{
    const char *program_name;
    char *log_file;
    int fd;
    enum logLevels log_level;
    int enable_console;
    enum logLevels console_level;
    int enable_syslog;
    enum logLevels syslog_level;
    struct list *per_logger_level;
    int enable_pid;
    pthread_mutex_t log_lock;
    pthread_mutexattr_t log_lock_attr;
};

/* internal functions, only used in log.c if this ifdef is defined.*/
#ifdef LOGINTERNALSTUFF

/**
 *
 * @brief Starts the logging subsystem
 * @param l_cfg logging system configuration
 * @return
 *
 */
enum logReturns
internal_log_start(struct log_config *l_cfg);

/**
 *
 * @brief Shuts down the logging subsystem
 * @param l_cfg pointer to the logging subsystem to stop
 *
 */
enum logReturns
internal_log_end(struct log_config *l_cfg);

/**
 * Converts a log level to a string
 * @param lvl, the loglevel
 * @param str pointer where the string will be stored.
 */
void
internal_log_lvl2str(const enum logLevels lvl, char *str);

/**
 *
 * @brief Converts a string to a log level
 * @param s The string to convert
 * @return The corresponding level or LOG_LEVEL_DEBUG if error
 *
 */
enum logLevels
internal_log_text2level(const char *buf);

/**
 * A function that init our struct that holds all state and
 * also init its content.
 * @return  LOG_STARTUP_OK or LOG_ERROR_MALLOC
 */
enum logReturns
internalInitAndAllocStruct(void);

/**
 * Read configuration from a file and store the values in lists.
 * @param file
 * @param lc
 * @param param_n
 * @param param_v
 * @param applicationName, the application name used in the log events.
 * @return
 */
enum logReturns
internal_config_read_logging(int file, struct log_config *lc,
                             struct list *param_n,
                             struct list *param_v,
                             const char *applicationName);
                             
/**
 * the log function that all files use to log an event.
 * @param lvl, the loglevel
 * @param msg, the logtext.
 * @param ...
 * @return
 */
enum logReturns
internal_log_message(const enum logLevels lvl, const char *msg, va_list args);

/*End of internal functions*/
#endif
/**
 * This function initialize the log facilities according to the configuration
 * file, that is described by the in parameter.
 * @param iniFile
 * @param applicationName, the name that is used in the log for the running application
 * @return LOG_STARTUP_OK on success
 */
enum logReturns
log_start(const char *iniFile, const char *applicationName);

/**
 * An alternative log_start where the caller gives the params directly.
 * @param iniParams
 * @return
 */
enum logReturns
log_start_from_param(const struct log_config *iniParams);
/**
 * Function that terminates all logging
 * @return
 */
enum logReturns
log_end(void);

/**
 * the log function that all files use to log an event.
 * 
 * Please prefer to use the LOG and LOG_DEVEL macros instead of this function directly.
 * 
 * @param lvl, the loglevel
 * @param msg, the logtext.
 * @param ...
 * @return
 */
enum logReturns
log_message(const enum logLevels lvl, const char *msg, ...) printflike(2, 3);

/**
 * the log function that all files use to log an event, 
 * with the function name and file line.
 *
 * Please prefer to use the LOG and LOG_DEVEL macros instead of this function directly.
 * 
 * @param function_name, the function name (typicaly the __func__ macro)
 * @param file_name, the file name (typicaly the __FILE__ macro)
 * @param line_number, the line number in the file (typicaly the __LINE__ macro)
 * @param lvl, the loglevel
 * @param msg, the logtext.
 * @param ...
 * @return
 */
enum logReturns
log_message_with_location(const char *function_name, 
                          const char *file_name, 
                          const int line_number, 
                          const enum logLevels lvl, 
                          const char *msg, 
                          ...) printflike(5, 6);

/**
 * This function returns the configured file name for the logfile
 * @param replybuf the buffer where the reply is stored
 * @param bufsize how big is the reply buffer.
 * @return
 */
char *getLogFile(char *replybuf, int bufsize);
#endif
