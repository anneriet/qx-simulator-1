/** \file
 * Provides macros for logging and the global loglevel variable.
 * Based on the OpenQL logger
 */

#pragma once
#ifndef QX_LOGGER_H
#define QX_LOGGER_H

#include <iostream>

#define QX_EOUT(content) \
    do {                                                                                                    \
        if (::qx::logger::log_level >= ::qx::logger::LogLevel::LOG_ERROR) {                   \
            ::std::cerr << "[QXELERATOR] " __FILE__ ":" << __LINE__ << " Error: " << content << ::std::endl;    \
        }                                                                                                   \
    } while (false)

#define QX_DOUT(content) \
    do {                                                                                                    \
        if (::qx::logger::log_level >= ::qx::logger::LogLevel::LOG_DEBUG) {                   \
            ::std::cout <<"[QXELERATOR]" << __FILE__ << ":" << __LINE__ << " " << content << ::std::endl; \
        }                                                                                                   \
    } while (false)

// Definitions of println and print to avoid having to change those everywhere
// Although it is probably better if they are replaced by proper log statements (that make explicit at what level of log they should be printed)
#define println(content) \
    do {                                                                                                    \
        if (::qx::logger::log_level >= ::qx::logger::LogLevel::LOG_DEBUG) {                   \
            ::std::cout << content << ::std::endl; \
        }                                                                                                   \
    } while (false)

#define print(content) \
    do {                                                                                                    \
        if (::qx::logger::log_level >= ::qx::logger::LogLevel::LOG_DEBUG) {                   \
            ::std::cout << content; \
        }                                                                                                   \
    } while (false)


namespace qx {
namespace logger {

enum LogLevel {
    LOG_NOTHING,
    LOG_CRITICAL,
    LOG_ERROR,
    LOG_WARNING,
    LOG_INFO,
    LOG_DEBUG
};

// Log level default is debug, so everything, including simulation results and measurement averages etc. is printed to cout
LogLevel log_level = LOG_DEBUG;

/**
 * Converts the string representation of a log level to a LogLevel enum variant.
 * Throws ql::exception if the string could not be converted.
 */
LogLevel log_level_from_string(const std::string &level) {
    if (level == "LOG_NOTHING") {
        return LogLevel::LOG_NOTHING;
    } else if (level == "LOG_CRITICAL") {
        return LogLevel::LOG_CRITICAL;
    } else if (level == "LOG_ERROR") {
        return LogLevel::LOG_ERROR;
    } else if (level == "LOG_WARNING") {
        return LogLevel::LOG_WARNING;
    } else if (level == "LOG_INFO") {
        return LogLevel::LOG_INFO;
    } else if(level == "LOG_DEBUG") {
        return LogLevel::LOG_DEBUG;
    } else {
        QX_EOUT("unknown log level" << level);
    }
}

/**
 * Sets the current log level using its string representation.
 */
void set_log_level(const std::string &level) {
    log_level = log_level_from_string(level);
}

} // namespace logger
} // namespace qx
#endif