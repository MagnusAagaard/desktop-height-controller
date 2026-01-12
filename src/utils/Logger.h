/**
 * @file Logger.h
 * @brief Logging utility with configurable levels and serial output
 * 
 * Provides structured logging with multiple severity levels.
 * Serial output enabled by default, optional SD card logging.
 */

#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>
#include "../Config.h"

/**
 * @enum LogLevel
 * @brief Severity levels for log messages
 */
enum class LogLevel : uint8_t {
    DEBUG = 0,   ///< Detailed debugging information
    INFO = 1,    ///< General operational information
    WARN = 2,    ///< Warning conditions
    ERROR = 3,   ///< Error conditions
    NONE = 4     ///< Disable all logging
};

/**
 * @class Logger
 * @brief Singleton logging utility
 * 
 * Usage:
 *   Logger::init(LogLevel::INFO);
 *   Logger::info("HeightController", "Sensor initialized");
 *   Logger::error("WiFiManager", "Connection failed: %s", ssid);
 */
class Logger {
public:
    /**
     * @brief Initialize the logger
     * @param minLevel Minimum level to output (messages below this are ignored)
     * @param serialOutput Enable serial output (default: true)
     */
    static void init(LogLevel minLevel = LogLevel::INFO, bool serialOutput = true);
    
    /**
     * @brief Log a debug message
     * @param tag Module/component name
     * @param format Printf-style format string
     * @param ... Format arguments
     */
    static void debug(const char* tag, const char* format, ...);
    
    /**
     * @brief Log an info message
     * @param tag Module/component name
     * @param format Printf-style format string
     * @param ... Format arguments
     */
    static void info(const char* tag, const char* format, ...);
    
    /**
     * @brief Log a warning message
     * @param tag Module/component name
     * @param format Printf-style format string
     * @param ... Format arguments
     */
    static void warn(const char* tag, const char* format, ...);
    
    /**
     * @brief Log an error message
     * @param tag Module/component name
     * @param format Printf-style format string
     * @param ... Format arguments
     */
    static void error(const char* tag, const char* format, ...);
    
    /**
     * @brief Set minimum log level at runtime
     * @param level New minimum level
     */
    static void setLevel(LogLevel level);
    
    /**
     * @brief Get current minimum log level
     * @return LogLevel Current level
     */
    static LogLevel getLevel();
    
    /**
     * @brief Get the level name as string
     * @param level Log level
     * @return const char* Level name ("DEBUG", "INFO", etc.)
     */
    static const char* levelToString(LogLevel level);

private:
    static LogLevel minLevel_;
    static bool serialEnabled_;
    static bool initialized_;
    
    static const uint16_t MAX_LOG_LENGTH = 256;
    static char buffer_[MAX_LOG_LENGTH];
    
    /**
     * @brief Internal log function
     * @param level Message level
     * @param tag Module name
     * @param format Format string
     * @param args Variable arguments
     */
    static void log(LogLevel level, const char* tag, const char* format, va_list args);
    
    /**
     * @brief Get current timestamp in milliseconds
     * @return unsigned long Timestamp
     */
    static unsigned long getTimestamp();
};

#endif // LOGGER_H
