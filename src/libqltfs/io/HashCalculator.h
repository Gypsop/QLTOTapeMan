/*
 * QLTOTapeMan - Qt-based LTO Tape Manager
 * libqltfs - LTFS Core Library
 *
 * Copyright (c) 2026 Jeffrey ZHU (zhxsh1225@gmail.com)
 * https://github.com/Gypsop/QLTOTapeMan
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include "libqltfs_global.h"
#include "core/LtfsTypes.h"

#include <QObject>
#include <QByteArray>
#include <QString>
#include <QCryptographicHash>
#include <QFuture>
#include <functional>

namespace qltfs {

/**
 * @brief Hash calculation modes
 */
enum class HashMode {
    None,       ///< No hashing
    MD5,        ///< MD5 hash (128-bit)
    SHA1,       ///< SHA-1 hash (160-bit)
    SHA256,     ///< SHA-256 hash (256-bit)
    SHA512,     ///< SHA-512 hash (512-bit)
    XXH64       ///< xxHash 64-bit (fast, non-cryptographic)
};

/**
 * @brief Result of hash calculation
 */
struct LIBQLTFS_EXPORT HashResult {
    bool success = false;
    HashMode mode = HashMode::None;
    QByteArray hash;            ///< Raw hash bytes
    QString hexString;          ///< Hex-encoded hash string
    QString base64String;       ///< Base64-encoded hash string
    qint64 bytesProcessed = 0;
    QString errorMessage;

    /**
     * @brief Check if hash matches another hash
     */
    bool matches(const HashResult &other) const;

    /**
     * @brief Check if hash matches a hex string
     */
    bool matchesHex(const QString &hex) const;
};

/**
 * @brief Progress callback for hash operations
 */
using HashProgressCallback = std::function<void(qint64 bytesProcessed, qint64 totalBytes)>;

/**
 * @brief Hash calculator with streaming support
 *
 * Supports various hash algorithms for file verification.
 * Can calculate hashes incrementally or from complete data.
 */
class LIBQLTFS_EXPORT HashCalculator : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param mode Hash algorithm to use
     * @param parent Parent object
     */
    explicit HashCalculator(HashMode mode = HashMode::SHA256, QObject *parent = nullptr);
    ~HashCalculator() override;

    // Disable copy
    HashCalculator(const HashCalculator &) = delete;
    HashCalculator &operator=(const HashCalculator &) = delete;

    /**
     * @brief Get current hash mode
     */
    HashMode mode() const;

    /**
     * @brief Set hash mode (resets any pending calculation)
     */
    void setMode(HashMode mode);

    // === Incremental hashing ===

    /**
     * @brief Reset hash state for new calculation
     */
    void reset();

    /**
     * @brief Add data to hash calculation
     * @param data Data to include in hash
     */
    void addData(const QByteArray &data);

    /**
     * @brief Add data from raw pointer
     * @param data Pointer to data
     * @param length Length of data
     */
    void addData(const char *data, qint64 length);

    /**
     * @brief Get final hash result
     * @return Calculated hash
     */
    HashResult result() const;

    /**
     * @brief Get bytes processed so far
     */
    qint64 bytesProcessed() const;

    // === One-shot hashing ===

    /**
     * @brief Calculate hash of byte array
     * @param data Data to hash
     * @return Hash result
     */
    HashResult hash(const QByteArray &data);

    /**
     * @brief Calculate hash of file
     * @param filePath Path to file
     * @param callback Progress callback
     * @return Hash result
     */
    HashResult hashFile(const QString &filePath, HashProgressCallback callback = nullptr);

    /**
     * @brief Calculate hash of file asynchronously
     * @param filePath Path to file
     * @return Future that will contain the hash result
     */
    QFuture<HashResult> hashFileAsync(const QString &filePath);

    // === Static convenience methods ===

    /**
     * @brief Calculate MD5 hash of data
     */
    static QString md5(const QByteArray &data);

    /**
     * @brief Calculate SHA-256 hash of data
     */
    static QString sha256(const QByteArray &data);

    /**
     * @brief Calculate SHA-256 hash of file
     */
    static QString sha256File(const QString &filePath);

    /**
     * @brief Calculate xxHash64 of data
     */
    static quint64 xxhash64(const QByteArray &data, quint64 seed = 0);

    /**
     * @brief Convert hash mode to string
     */
    static QString modeToString(HashMode mode);

    /**
     * @brief Convert string to hash mode
     */
    static HashMode stringToMode(const QString &str);

    /**
     * @brief Get hash length in bytes for a mode
     */
    static int hashLength(HashMode mode);

signals:
    /**
     * @brief Emitted during file hashing
     */
    void progressChanged(qint64 bytesProcessed, qint64 totalBytes);

    /**
     * @brief Emitted when async hash completes
     */
    void hashCompleted(const HashResult &result);

private:
    class Private;
    Private *d;
};

} // namespace qltfs

Q_DECLARE_METATYPE(qltfs::HashResult)
Q_DECLARE_METATYPE(qltfs::HashMode)
