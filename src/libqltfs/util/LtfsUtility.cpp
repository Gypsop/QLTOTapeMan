/*
 * QLTOTapeMan - Qt-based LTO Tape Manager
 *
 * Copyright (c) 2026 Jeffrey ZHU (zhxsh1225@gmail.com)
 * https://github.com/Gypsop/QLTOTapeMan
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "LtfsUtility.h"

#include <QUuid>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QMutex>
#include <QCoreApplication>

#include <cmath>

namespace qltfs {
namespace util {

// ============================================================================
// LtfsUtility Implementation
// ============================================================================

QString LtfsUtility::formatSize(qint64 bytes, bool useBinary, int precision)
{
    if (bytes < 0) {
        return QString("-%1").arg(formatSize(-bytes, useBinary, precision));
    }

    const qint64 base = useBinary ? 1024 : 1000;
    const char *binaryUnits[] = {"B", "KiB", "MiB", "GiB", "TiB", "PiB", "EiB"};
    const char *siUnits[] = {"B", "KB", "MB", "GB", "TB", "PB", "EB"};
    const char **units = useBinary ? binaryUnits : siUnits;

    int unitIndex = 0;
    double size = static_cast<double>(bytes);

    while (size >= base && unitIndex < 6) {
        size /= base;
        unitIndex++;
    }

    return QString("%1 %2")
        .arg(size, 0, 'f', (unitIndex == 0) ? 0 : precision)
        .arg(units[unitIndex]);
}

qint64 LtfsUtility::parseSize(const QString &sizeStr)
{
    static QRegularExpression regex(
        R"(^\s*(-?[\d.]+)\s*([KMGTPE]?i?[Bb]?)?\s*$)",
        QRegularExpression::CaseInsensitiveOption);

    QRegularExpressionMatch match = regex.match(sizeStr);
    if (!match.hasMatch()) {
        return -1;
    }

    bool ok;
    double value = match.captured(1).toDouble(&ok);
    if (!ok) {
        return -1;
    }

    QString unit = match.captured(2).toLower();
    
    qint64 multiplier = 1;
    bool binary = unit.contains('i');

    if (unit.startsWith('k')) {
        multiplier = binary ? 1024LL : 1000LL;
    } else if (unit.startsWith('m')) {
        multiplier = binary ? (1024LL * 1024) : (1000LL * 1000);
    } else if (unit.startsWith('g')) {
        multiplier = binary ? (1024LL * 1024 * 1024) : (1000LL * 1000 * 1000);
    } else if (unit.startsWith('t')) {
        multiplier = binary ? (1024LL * 1024 * 1024 * 1024) : (1000LL * 1000 * 1000 * 1000);
    } else if (unit.startsWith('p')) {
        multiplier = binary ? (1024LL * 1024 * 1024 * 1024 * 1024) : (1000LL * 1000 * 1000 * 1000 * 1000);
    } else if (unit.startsWith('e')) {
        multiplier = binary ? (1024LL * 1024 * 1024 * 1024 * 1024 * 1024) : (1000LL * 1000 * 1000 * 1000 * 1000 * 1000);
    }

    return static_cast<qint64>(value * multiplier);
}

QString LtfsUtility::formatSpeed(double bytesPerSec, bool useBinary)
{
    return formatSize(static_cast<qint64>(bytesPerSec), useBinary) + "/s";
}

QString LtfsUtility::formatDuration(qint64 seconds, bool showMillis)
{
    Q_UNUSED(showMillis)

    if (seconds < 0) {
        return QString("-%1").arg(formatDuration(-seconds));
    }

    qint64 hours = seconds / 3600;
    qint64 minutes = (seconds % 3600) / 60;
    qint64 secs = seconds % 60;

    if (hours > 0) {
        return QString("%1:%2:%3")
            .arg(hours)
            .arg(minutes, 2, 10, QChar('0'))
            .arg(secs, 2, 10, QChar('0'));
    } else {
        return QString("%1:%2")
            .arg(minutes)
            .arg(secs, 2, 10, QChar('0'));
    }
}

QString LtfsUtility::formatDurationMs(qint64 milliseconds)
{
    qint64 seconds = milliseconds / 1000;
    qint64 ms = milliseconds % 1000;

    return QString("%1.%2")
        .arg(formatDuration(seconds))
        .arg(ms, 3, 10, QChar('0'));
}

QString LtfsUtility::formatLtfsDateTime(const QDateTime &dateTime)
{
    // LTFS uses ISO 8601 format: 2023-12-25T14:30:00.123456Z
    return dateTime.toUTC().toString(Qt::ISODateWithMs) + "Z";
}

QDateTime LtfsUtility::parseLtfsDateTime(const QString &dateTimeStr)
{
    QString str = dateTimeStr;
    
    // Remove trailing 'Z' if present
    if (str.endsWith('Z') || str.endsWith('z')) {
        str.chop(1);
    }

    QDateTime dt = QDateTime::fromString(str, Qt::ISODateWithMs);
    if (!dt.isValid()) {
        dt = QDateTime::fromString(str, Qt::ISODate);
    }

    if (dt.isValid()) {
        dt.setTimeSpec(Qt::UTC);
    }

    return dt;
}

bool LtfsUtility::isValidBarcode(const QString &barcode)
{
    // LTO barcode format: 6 characters + 2 character media type
    // Example: ABC123L8 (L = LTO, 8 = generation)
    static QRegularExpression regex("^[A-Z0-9]{6}L[1-9]$",
        QRegularExpression::CaseInsensitiveOption);

    return regex.match(barcode).hasMatch();
}

int LtfsUtility::getLtoGenerationFromBarcode(const QString &barcode)
{
    if (barcode.length() < 8) {
        return 0;
    }

    // Last character is the generation
    QChar genChar = barcode.at(barcode.length() - 1);
    if (genChar.isDigit()) {
        return genChar.digitValue();
    }

    return 0;
}

QString LtfsUtility::getLtoMediaTypeString(int generation)
{
    if (generation >= 1 && generation <= 9) {
        return QString("LTO-%1").arg(generation);
    }
    return QString("Unknown");
}

qint64 LtfsUtility::getLtoNominalCapacity(int generation, bool compressed)
{
    // Native capacities in bytes (approximate)
    static const qint64 nativeCapacities[] = {
        0,                              // 0 - invalid
        100LL * 1000 * 1000 * 1000,     // LTO-1: 100 GB
        200LL * 1000 * 1000 * 1000,     // LTO-2: 200 GB
        400LL * 1000 * 1000 * 1000,     // LTO-3: 400 GB
        800LL * 1000 * 1000 * 1000,     // LTO-4: 800 GB
        1500LL * 1000 * 1000 * 1000,    // LTO-5: 1.5 TB
        2500LL * 1000 * 1000 * 1000,    // LTO-6: 2.5 TB
        6000LL * 1000 * 1000 * 1000,    // LTO-7: 6 TB
        12000LL * 1000 * 1000 * 1000,   // LTO-8: 12 TB
        18000LL * 1000 * 1000 * 1000    // LTO-9: 18 TB
    };

    if (generation < 1 || generation > 9) {
        return 0;
    }

    qint64 capacity = nativeCapacities[generation];
    if (compressed) {
        // Typical 2.5:1 compression ratio
        capacity = static_cast<qint64>(capacity * 2.5);
    }

    return capacity;
}

QString LtfsUtility::sanitizeLtfsFilename(const QString &filename)
{
    QString result = filename;

    // Replace invalid characters
    static const char invalidChars[] = {'/', '\\', ':', '*', '?', '"', '<', '>', '|', '\0'};
    for (char c : invalidChars) {
        if (c != '\0') {
            result.replace(c, '_');
        }
    }

    // Remove leading/trailing spaces and dots
    result = result.trimmed();
    while (result.endsWith('.')) {
        result.chop(1);
    }

    // Limit length (LTFS typically supports up to 255 bytes)
    if (result.toUtf8().size() > 255) {
        // Truncate while preserving extension
        QString ext = getFileExtension(result);
        QString base = getFileBaseName(result);
        
        int maxBaseLength = 255 - ext.toUtf8().size() - 1;  // -1 for dot
        while (base.toUtf8().size() > maxBaseLength && !base.isEmpty()) {
            base.chop(1);
        }
        
        result = ext.isEmpty() ? base : (base + "." + ext);
    }

    return result.isEmpty() ? QString("_") : result;
}

bool LtfsUtility::isValidLtfsFilename(const QString &filename)
{
    if (filename.isEmpty()) {
        return false;
    }

    // Check for invalid characters
    static const char invalidChars[] = {'/', '\\', ':', '*', '?', '"', '<', '>', '|'};
    for (char c : invalidChars) {
        if (filename.contains(c)) {
            return false;
        }
    }

    // Check for control characters
    for (const QChar &ch : filename) {
        if (ch.unicode() < 32) {
            return false;
        }
    }

    // Check length
    if (filename.toUtf8().size() > 255) {
        return false;
    }

    return true;
}

QString LtfsUtility::normalizeLtfsPath(const QString &path)
{
    QString result = path;
    result.replace('\\', '/');

    // Remove double slashes
    while (result.contains("//")) {
        result.replace("//", "/");
    }

    // Remove trailing slash
    while (result.endsWith('/') && result.length() > 1) {
        result.chop(1);
    }

    return result;
}

QString LtfsUtility::getFileExtension(const QString &filename)
{
    int lastDot = filename.lastIndexOf('.');
    int lastSlash = qMax(filename.lastIndexOf('/'), filename.lastIndexOf('\\'));

    if (lastDot > lastSlash + 1 && lastDot < filename.length() - 1) {
        return filename.mid(lastDot + 1);
    }

    return QString();
}

QString LtfsUtility::getFileBaseName(const QString &filename)
{
    // Remove path
    int lastSlash = qMax(filename.lastIndexOf('/'), filename.lastIndexOf('\\'));
    QString name = (lastSlash >= 0) ? filename.mid(lastSlash + 1) : filename;

    // Remove extension
    int lastDot = name.lastIndexOf('.');
    if (lastDot > 0) {
        return name.left(lastDot);
    }

    return name;
}

QString LtfsUtility::generateUniqueName(const QString &baseName, 
                                         const QStringList &existingNames)
{
    if (!existingNames.contains(baseName, Qt::CaseInsensitive)) {
        return baseName;
    }

    QString ext = getFileExtension(baseName);
    QString base = getFileBaseName(baseName);

    int counter = 2;
    QString newName;

    do {
        if (ext.isEmpty()) {
            newName = QString("%1 (%2)").arg(base).arg(counter);
        } else {
            newName = QString("%1 (%2).%3").arg(base).arg(counter).arg(ext);
        }
        counter++;
    } while (existingNames.contains(newName, Qt::CaseInsensitive));

    return newName;
}

quint64 LtfsUtility::calculateBlockCount(qint64 fileSize, quint32 blockSize)
{
    if (fileSize <= 0 || blockSize == 0) {
        return 0;
    }

    return static_cast<quint64>((fileSize + blockSize - 1) / blockSize);
}

QString LtfsUtility::generateUuid()
{
    QUuid uuid = QUuid::createUuid();
    return uuid.toString(QUuid::WithoutBraces);
}

quint32 LtfsUtility::calculateCrc32(const QByteArray &data)
{
    // CRC-32 polynomial used in LTFS (same as used in Ethernet, ZIP, etc.)
    static quint32 crcTable[256];
    static bool tableInitialized = false;

    if (!tableInitialized) {
        for (int i = 0; i < 256; i++) {
            quint32 crc = static_cast<quint32>(i);
            for (int j = 0; j < 8; j++) {
                crc = (crc & 1) ? (crc >> 1) ^ 0xEDB88320 : crc >> 1;
            }
            crcTable[i] = crc;
        }
        tableInitialized = true;
    }

    quint32 crc = 0xFFFFFFFF;
    for (int i = 0; i < data.size(); i++) {
        quint8 index = (crc ^ static_cast<quint8>(data[i])) & 0xFF;
        crc = crcTable[index] ^ (crc >> 8);
    }

    return crc ^ 0xFFFFFFFF;
}

bool LtfsUtility::verifyLtfsIndexSignature(const QByteArray &xmlData)
{
    // Check for LTFS index XML signature
    if (xmlData.size() < 100) {
        return false;
    }

    // Look for LTFS namespace
    return xmlData.contains("http://www.ibm.com/ltfs/2013/09/")
        || xmlData.contains("ltfsindex");
}

QString LtfsUtility::getCreatorString()
{
    return QString("QLTOTapeMan %1")
        .arg(QCoreApplication::applicationVersion().isEmpty() 
             ? "1.0.0" : QCoreApplication::applicationVersion());
}

QString LtfsUtility::blockAddressToString(quint8 partition, quint64 block)
{
    return QString("P%1:B%2").arg(partition).arg(block);
}

bool LtfsUtility::parseBlockAddress(const QString &addressStr, 
                                     quint8 &partition, quint64 &block)
{
    static QRegularExpression regex("^P(\\d+):B(\\d+)$",
        QRegularExpression::CaseInsensitiveOption);

    QRegularExpressionMatch match = regex.match(addressStr);
    if (!match.hasMatch()) {
        return false;
    }

    bool ok1, ok2;
    int p = match.captured(1).toInt(&ok1);
    quint64 b = match.captured(2).toULongLong(&ok2);

    if (!ok1 || !ok2 || p < 0 || p > 255) {
        return false;
    }

    partition = static_cast<quint8>(p);
    block = b;
    return true;
}

// ============================================================================
// ErrorHandler Implementation
// ============================================================================

ErrorHandler &ErrorHandler::instance()
{
    static ErrorHandler instance;
    return instance;
}

ErrorHandler::ErrorHandler()
    : m_maxHistorySize(1000)
    , m_consoleOutput(true)
    , m_fileOutput(false)
    , m_minSeverity(Severity::Debug)
{
    m_lastError.severity = Severity::Info;
    m_lastError.category = Category::General;
    m_lastError.code = 0;
}

ErrorHandler::~ErrorHandler()
{
}

void ErrorHandler::logError(Severity severity, Category category, int code,
                            const QString &message, const QString &details,
                            const QString &source)
{
    if (severity < m_minSeverity) {
        return;
    }

    ErrorInfo error;
    error.severity = severity;
    error.category = category;
    error.code = code;
    error.message = message;
    error.details = details;
    error.source = source;
    error.timestamp = QDateTime::currentDateTime();

    m_lastError = error;

    // Add to history
    m_history.append(error);
    while (m_history.size() > m_maxHistorySize) {
        m_history.removeFirst();
    }

    // Output
    if (m_consoleOutput) {
        writeToConsole(error);
    }

    if (m_fileOutput && !m_logFilePath.isEmpty()) {
        writeToFile(error);
    }
}

void ErrorHandler::debug(const QString &message, const QString &details)
{
    logError(Severity::Debug, Category::General, 0, message, details);
}

void ErrorHandler::info(const QString &message, const QString &details)
{
    logError(Severity::Info, Category::General, 0, message, details);
}

void ErrorHandler::warning(const QString &message, const QString &details)
{
    logError(Severity::Warning, Category::General, 0, message, details);
}

void ErrorHandler::error(const QString &message, const QString &details)
{
    logError(Severity::Error, Category::General, 0, message, details);
}

void ErrorHandler::critical(const QString &message, const QString &details)
{
    logError(Severity::Critical, Category::General, 0, message, details);
}

QList<ErrorHandler::ErrorInfo> ErrorHandler::errorHistory(int maxCount) const
{
    if (maxCount <= 0 || maxCount >= m_history.size()) {
        return m_history;
    }

    return m_history.mid(m_history.size() - maxCount);
}

void ErrorHandler::clearHistory()
{
    m_history.clear();
}

void ErrorHandler::setLogFile(const QString &path)
{
    m_logFilePath = path;
    m_fileOutput = !path.isEmpty();
}

QString ErrorHandler::severityToString(Severity severity)
{
    switch (severity) {
    case Severity::Debug:    return "DEBUG";
    case Severity::Info:     return "INFO";
    case Severity::Warning:  return "WARN";
    case Severity::Error:    return "ERROR";
    case Severity::Critical: return "CRIT";
    default:                 return "?";
    }
}

QString ErrorHandler::categoryToString(Category category)
{
    switch (category) {
    case Category::General:       return "General";
    case Category::Device:        return "Device";
    case Category::IO:            return "IO";
    case Category::Index:         return "Index";
    case Category::Filesystem:    return "Filesystem";
    case Category::Network:       return "Network";
    case Category::Configuration: return "Config";
    default:                      return "?";
    }
}

void ErrorHandler::writeToFile(const ErrorInfo &error)
{
    static QMutex mutex;
    QMutexLocker locker(&mutex);

    QFile file(m_logFilePath);
    if (file.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream stream(&file);
        stream << error.timestamp.toString("yyyy-MM-dd HH:mm:ss.zzz")
               << " [" << severityToString(error.severity) << "]"
               << " [" << categoryToString(error.category) << "]"
               << " " << error.message;
        if (!error.details.isEmpty()) {
            stream << " (" << error.details << ")";
        }
        if (!error.source.isEmpty()) {
            stream << " @" << error.source;
        }
        stream << "\n";
        file.close();
    }
}

void ErrorHandler::writeToConsole(const ErrorInfo &error)
{
    QString output = QString("%1 [%2] %3")
        .arg(error.timestamp.toString("HH:mm:ss.zzz"))
        .arg(severityToString(error.severity))
        .arg(error.message);

    if (!error.details.isEmpty()) {
        output += QString(" (%1)").arg(error.details);
    }

    switch (error.severity) {
    case Severity::Debug:
        qDebug().noquote() << output;
        break;
    case Severity::Info:
        qInfo().noquote() << output;
        break;
    case Severity::Warning:
        qWarning().noquote() << output;
        break;
    case Severity::Error:
    case Severity::Critical:
        qCritical().noquote() << output;
        break;
    }
}

} // namespace util
} // namespace qltfs
