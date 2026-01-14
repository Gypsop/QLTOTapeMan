/**
 * QLTOTapeMan - Qt-based LTO Tape Manager
 * Logger Header
 *
 * Copyright (c) 2026 Jeffrey ZHU (zhxsh1225@gmail.com)
 * https://github.com/Gypsop/QLTOTapeMan
 */

#ifndef QLTFS_LOGGER_H
#define QLTFS_LOGGER_H

#include "../libqltfs_global.h"

#include <QString>
#include <QFile>
#include <QMutex>
#include <QDateTime>
#include <functional>

namespace qltfs {

/**
 * @brief Log level enumeration
 */
enum class LogLevel {
    Trace,      ///< Most verbose, for debugging
    Debug,      ///< Debug information
    Info,       ///< Informational messages
    Warning,    ///< Warning messages
    Error,      ///< Error messages
    Fatal       ///< Fatal errors
};

/**
 * @brief Log entry structure
 */
struct LIBQLTFS_EXPORT LogEntry {
    QDateTime timestamp;
    LogLevel level;
    QString category;
    QString message;
    QString file;
    int line;
    QString function;
};

/**
 * @brief Callback type for log handlers
 */
using LogHandler = std::function<void(const LogEntry&)>;

/**
 * @brief Thread-safe logging utility
 *
 * Provides centralized logging with multiple output destinations.
 */
class LIBQLTFS_EXPORT Logger
{
public:
    /**
     * @brief Get singleton instance
     */
    static Logger& instance();

    /**
     * @brief Initialize logger with file output
     * @param logFilePath Path to log file
     * @param append Append to existing file
     * @return true if initialization succeeded
     */
    bool init(const QString& logFilePath, bool append = true);

    /**
     * @brief Shutdown logger and close file
     */
    void shutdown();

    /**
     * @brief Set minimum log level
     */
    void setLogLevel(LogLevel level);

    /**
     * @brief Get current log level
     */
    LogLevel logLevel() const { return m_logLevel; }

    /**
     * @brief Enable/disable console output
     */
    void setConsoleOutput(bool enabled);

    /**
     * @brief Enable/disable file output
     */
    void setFileOutput(bool enabled);

    /**
     * @brief Add custom log handler
     * @param handler Callback function
     * @return Handler ID for later removal
     */
    int addHandler(LogHandler handler);

    /**
     * @brief Remove custom log handler
     * @param handlerId Handler ID returned by addHandler
     */
    void removeHandler(int handlerId);

    /**
     * @brief Log a message
     */
    void log(LogLevel level, const QString& category, const QString& message,
             const char* file = nullptr, int line = 0, const char* function = nullptr);

    /**
     * @brief Convert log level to string
     */
    static QString levelToString(LogLevel level);

    /**
     * @brief Parse string to log level
     */
    static LogLevel stringToLevel(const QString& str);

    // Convenience methods
    void trace(const QString& category, const QString& message);
    void debug(const QString& category, const QString& message);
    void info(const QString& category, const QString& message);
    void warning(const QString& category, const QString& message);
    void error(const QString& category, const QString& message);
    void fatal(const QString& category, const QString& message);

private:
    Logger();
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void writeToFile(const LogEntry& entry);
    void writeToConsole(const LogEntry& entry);
    QString formatEntry(const LogEntry& entry) const;

    QMutex m_mutex;
    QFile m_logFile;
    LogLevel m_logLevel;
    bool m_consoleOutput;
    bool m_fileOutput;
    QMap<int, LogHandler> m_handlers;
    int m_nextHandlerId;
};

// =============================================================================
// Logging Macros
// =============================================================================

#define QLTFS_LOG(level, category, message) \
    qltfs::Logger::instance().log(level, category, message, __FILE__, __LINE__, Q_FUNC_INFO)

#define QLTFS_TRACE(category, message) \
    QLTFS_LOG(qltfs::LogLevel::Trace, category, message)

#define QLTFS_DEBUG(category, message) \
    QLTFS_LOG(qltfs::LogLevel::Debug, category, message)

#define QLTFS_INFO(category, message) \
    QLTFS_LOG(qltfs::LogLevel::Info, category, message)

#define QLTFS_WARNING(category, message) \
    QLTFS_LOG(qltfs::LogLevel::Warning, category, message)

#define QLTFS_ERROR(category, message) \
    QLTFS_LOG(qltfs::LogLevel::Error, category, message)

#define QLTFS_FATAL(category, message) \
    QLTFS_LOG(qltfs::LogLevel::Fatal, category, message)

} // namespace qltfs

#endif // QLTFS_LOGGER_H
