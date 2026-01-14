/**
 * QLTOTapeMan - Qt-based LTO Tape Manager
 * Logger Implementation
 *
 * Copyright (c) 2026 Jeffrey ZHU (zhxsh1225@gmail.com)
 * https://github.com/Gypsop/QLTOTapeMan
 */

#include "Logger.h"

#include <QTextStream>
#include <QDir>
#include <QFileInfo>
#include <iostream>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace qltfs {

// =============================================================================
// Logger Implementation
// =============================================================================

Logger::Logger()
    : m_logLevel(LogLevel::Info)
    , m_consoleOutput(true)
    , m_fileOutput(true)
    , m_nextHandlerId(1)
{
}

Logger::~Logger()
{
    shutdown();
}

Logger& Logger::instance()
{
    static Logger instance;
    return instance;
}

bool Logger::init(const QString& logFilePath, bool append)
{
    QMutexLocker locker(&m_mutex);

    // Close existing file if open
    if (m_logFile.isOpen()) {
        m_logFile.close();
    }

    // Create directory if needed
    QFileInfo fileInfo(logFilePath);
    QDir dir = fileInfo.absoluteDir();
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            std::cerr << "Failed to create log directory: " 
                      << dir.absolutePath().toStdString() << std::endl;
            return false;
        }
    }

    // Open log file
    m_logFile.setFileName(logFilePath);
    QIODevice::OpenMode mode = QIODevice::WriteOnly | QIODevice::Text;
    if (append) {
        mode |= QIODevice::Append;
    } else {
        mode |= QIODevice::Truncate;
    }

    if (!m_logFile.open(mode)) {
        std::cerr << "Failed to open log file: " 
                  << logFilePath.toStdString() << std::endl;
        return false;
    }

    // Write header
    QTextStream stream(&m_logFile);
    stream << "\n";
    stream << "========================================\n";
    stream << "QLTOTapeMan Log Started\n";
    stream << "Time: " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n";
    stream << "========================================\n";
    stream.flush();

    return true;
}

void Logger::shutdown()
{
    QMutexLocker locker(&m_mutex);

    if (m_logFile.isOpen()) {
        QTextStream stream(&m_logFile);
        stream << "\n";
        stream << "========================================\n";
        stream << "QLTOTapeMan Log Ended\n";
        stream << "Time: " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n";
        stream << "========================================\n";
        stream.flush();
        m_logFile.close();
    }

    m_handlers.clear();
}

void Logger::setLogLevel(LogLevel level)
{
    QMutexLocker locker(&m_mutex);
    m_logLevel = level;
}

void Logger::setConsoleOutput(bool enabled)
{
    QMutexLocker locker(&m_mutex);
    m_consoleOutput = enabled;
}

void Logger::setFileOutput(bool enabled)
{
    QMutexLocker locker(&m_mutex);
    m_fileOutput = enabled;
}

int Logger::addHandler(LogHandler handler)
{
    QMutexLocker locker(&m_mutex);
    int id = m_nextHandlerId++;
    m_handlers.insert(id, handler);
    return id;
}

void Logger::removeHandler(int handlerId)
{
    QMutexLocker locker(&m_mutex);
    m_handlers.remove(handlerId);
}

void Logger::log(LogLevel level, const QString& category, const QString& message,
                 const char* file, int line, const char* function)
{
    // Check log level (without lock for performance)
    if (level < m_logLevel) {
        return;
    }

    // Create log entry
    LogEntry entry;
    entry.timestamp = QDateTime::currentDateTime();
    entry.level = level;
    entry.category = category;
    entry.message = message;
    entry.file = file ? QString::fromUtf8(file) : QString();
    entry.line = line;
    entry.function = function ? QString::fromUtf8(function) : QString();

    QMutexLocker locker(&m_mutex);

    // Write to console
    if (m_consoleOutput) {
        writeToConsole(entry);
    }

    // Write to file
    if (m_fileOutput && m_logFile.isOpen()) {
        writeToFile(entry);
    }

    // Call custom handlers
    for (auto& handler : m_handlers) {
        try {
            handler(entry);
        } catch (...) {
            // Ignore handler exceptions
        }
    }
}

QString Logger::levelToString(LogLevel level)
{
    switch (level) {
    case LogLevel::Trace:   return QStringLiteral("TRACE");
    case LogLevel::Debug:   return QStringLiteral("DEBUG");
    case LogLevel::Info:    return QStringLiteral("INFO");
    case LogLevel::Warning: return QStringLiteral("WARN");
    case LogLevel::Error:   return QStringLiteral("ERROR");
    case LogLevel::Fatal:   return QStringLiteral("FATAL");
    }
    return QStringLiteral("UNKNOWN");
}

LogLevel Logger::stringToLevel(const QString& str)
{
    QString upper = str.toUpper().trimmed();
    if (upper == QLatin1String("TRACE")) return LogLevel::Trace;
    if (upper == QLatin1String("DEBUG")) return LogLevel::Debug;
    if (upper == QLatin1String("INFO"))  return LogLevel::Info;
    if (upper == QLatin1String("WARN") || upper == QLatin1String("WARNING")) return LogLevel::Warning;
    if (upper == QLatin1String("ERROR")) return LogLevel::Error;
    if (upper == QLatin1String("FATAL")) return LogLevel::Fatal;
    return LogLevel::Info; // Default
}

void Logger::trace(const QString& category, const QString& message)
{
    log(LogLevel::Trace, category, message);
}

void Logger::debug(const QString& category, const QString& message)
{
    log(LogLevel::Debug, category, message);
}

void Logger::info(const QString& category, const QString& message)
{
    log(LogLevel::Info, category, message);
}

void Logger::warning(const QString& category, const QString& message)
{
    log(LogLevel::Warning, category, message);
}

void Logger::error(const QString& category, const QString& message)
{
    log(LogLevel::Error, category, message);
}

void Logger::fatal(const QString& category, const QString& message)
{
    log(LogLevel::Fatal, category, message);
}

void Logger::writeToFile(const LogEntry& entry)
{
    if (!m_logFile.isOpen()) return;

    QTextStream stream(&m_logFile);
    stream << formatEntry(entry) << "\n";
    stream.flush();
}

void Logger::writeToConsole(const LogEntry& entry)
{
    QString formatted = formatEntry(entry);

#ifdef Q_OS_WIN
    // Windows: Handle console colors
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    WORD originalAttrs = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;

    // Get original attributes
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(hConsole, &csbi)) {
        originalAttrs = csbi.wAttributes;
    }

    // Set color based on level
    WORD color = originalAttrs;
    switch (entry.level) {
    case LogLevel::Trace:
        color = FOREGROUND_INTENSITY;
        break;
    case LogLevel::Debug:
        color = FOREGROUND_GREEN | FOREGROUND_BLUE;
        break;
    case LogLevel::Info:
        color = FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
        break;
    case LogLevel::Warning:
        color = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
        break;
    case LogLevel::Error:
        color = FOREGROUND_RED | FOREGROUND_INTENSITY;
        break;
    case LogLevel::Fatal:
        color = FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
        break;
    }

    SetConsoleTextAttribute(hConsole, color);
    std::cout << formatted.toLocal8Bit().constData() << std::endl;
    SetConsoleTextAttribute(hConsole, originalAttrs);
#else
    // Unix: Use ANSI escape codes
    const char* colorCode = "\033[0m";  // Reset
    switch (entry.level) {
    case LogLevel::Trace:
        colorCode = "\033[90m";  // Dark gray
        break;
    case LogLevel::Debug:
        colorCode = "\033[36m";  // Cyan
        break;
    case LogLevel::Info:
        colorCode = "\033[32m";  // Green
        break;
    case LogLevel::Warning:
        colorCode = "\033[33m";  // Yellow
        break;
    case LogLevel::Error:
        colorCode = "\033[31m";  // Red
        break;
    case LogLevel::Fatal:
        colorCode = "\033[35m";  // Magenta
        break;
    }

    std::cout << colorCode << formatted.toLocal8Bit().constData() << "\033[0m" << std::endl;
#endif
}

QString Logger::formatEntry(const LogEntry& entry) const
{
    QString result;
    result.reserve(256);

    // Timestamp
    result += entry.timestamp.toString(QStringLiteral("yyyy-MM-dd hh:mm:ss.zzz"));
    result += QLatin1String(" [");

    // Level (padded to 5 chars)
    QString levelStr = levelToString(entry.level);
    result += levelStr.leftJustified(5);
    result += QLatin1String("] ");

    // Category
    if (!entry.category.isEmpty()) {
        result += QLatin1Char('[');
        result += entry.category;
        result += QLatin1String("] ");
    }

    // Message
    result += entry.message;

    // Source location for debug/trace
    if ((entry.level <= LogLevel::Debug) && !entry.file.isEmpty()) {
        result += QLatin1String(" (");
        
        // Extract just filename
        int lastSlash = entry.file.lastIndexOf(QLatin1Char('/'));
        if (lastSlash < 0) {
            lastSlash = entry.file.lastIndexOf(QLatin1Char('\\'));
        }
        QString filename = (lastSlash >= 0) ? entry.file.mid(lastSlash + 1) : entry.file;
        
        result += filename;
        result += QLatin1Char(':');
        result += QString::number(entry.line);
        result += QLatin1Char(')');
    }

    return result;
}

} // namespace qltfs
