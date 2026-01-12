/**
 * @file Logger.cpp
 * @brief Implementation of logging utility
 */

#include "Logger.h"
#include <stdarg.h>

// Static member initialization
LogLevel Logger::minLevel_ = LogLevel::INFO;
bool Logger::serialEnabled_ = true;
bool Logger::initialized_ = false;
char Logger::buffer_[MAX_LOG_LENGTH];

void Logger::init(LogLevel minLevel, bool serialOutput) {
    minLevel_ = minLevel;
    serialEnabled_ = serialOutput;
    
    if (serialOutput && !Serial) {
        Serial.begin(SERIAL_BAUD_RATE);
        // Wait for serial to initialize (with timeout)
        unsigned long startTime = millis();
        while (!Serial && (millis() - startTime) < 1000) {
            delay(10);
        }
    }
    
    initialized_ = true;
    
    if (minLevel != LogLevel::NONE) {
        info("Logger", "Initialized with level: %s", levelToString(minLevel));
    }
}

void Logger::debug(const char* tag, const char* format, ...) {
    if (minLevel_ > LogLevel::DEBUG) return;
    
    va_list args;
    va_start(args, format);
    log(LogLevel::DEBUG, tag, format, args);
    va_end(args);
}

void Logger::info(const char* tag, const char* format, ...) {
    if (minLevel_ > LogLevel::INFO) return;
    
    va_list args;
    va_start(args, format);
    log(LogLevel::INFO, tag, format, args);
    va_end(args);
}

void Logger::warn(const char* tag, const char* format, ...) {
    if (minLevel_ > LogLevel::WARN) return;
    
    va_list args;
    va_start(args, format);
    log(LogLevel::WARN, tag, format, args);
    va_end(args);
}

void Logger::error(const char* tag, const char* format, ...) {
    if (minLevel_ > LogLevel::ERROR) return;
    
    va_list args;
    va_start(args, format);
    log(LogLevel::ERROR, tag, format, args);
    va_end(args);
}

void Logger::setLevel(LogLevel level) {
    minLevel_ = level;
}

LogLevel Logger::getLevel() {
    return minLevel_;
}

const char* Logger::levelToString(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO";
        case LogLevel::WARN:  return "WARN";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::NONE:  return "NONE";
        default:              return "UNKNOWN";
    }
}

void Logger::log(LogLevel level, const char* tag, const char* format, va_list args) {
    if (!serialEnabled_) return;
    
    // Format: [timestamp] [LEVEL] [tag] message
    unsigned long timestamp = getTimestamp();
    
    // Build the message
    vsnprintf(buffer_, MAX_LOG_LENGTH - 1, format, args);
    buffer_[MAX_LOG_LENGTH - 1] = '\0';  // Ensure null termination
    
    // Print to serial
    Serial.printf("[%8lu] [%-5s] [%-16s] %s\n", 
                  timestamp, 
                  levelToString(level), 
                  tag, 
                  buffer_);
}

unsigned long Logger::getTimestamp() {
    return millis();
}
