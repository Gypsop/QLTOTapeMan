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

#include "HashCalculator.h"

#include <QFile>
#include <QDebug>
#include <QtConcurrent>

namespace qltfs {

// Simple xxHash64 implementation for non-cryptographic fast hashing
// Based on xxHash algorithm by Yann Collet
class XXHash64
{
public:
    static constexpr quint64 PRIME1 = 0x9E3779B185EBCA87ULL;
    static constexpr quint64 PRIME2 = 0xC2B2AE3D27D4EB4FULL;
    static constexpr quint64 PRIME3 = 0x165667B19E3779F9ULL;
    static constexpr quint64 PRIME4 = 0x85EBCA77C2B2AE63ULL;
    static constexpr quint64 PRIME5 = 0x27D4EB2F165667C5ULL;

    explicit XXHash64(quint64 seed = 0)
        : m_seed(seed)
    {
        reset();
    }

    void reset()
    {
        m_totalLen = 0;
        m_v1 = m_seed + PRIME1 + PRIME2;
        m_v2 = m_seed + PRIME2;
        m_v3 = m_seed;
        m_v4 = m_seed - PRIME1;
        m_bufferSize = 0;
    }

    void update(const void *data, size_t length)
    {
        const quint8 *p = static_cast<const quint8 *>(data);
        const quint8 *end = p + length;

        m_totalLen += length;

        if (m_bufferSize + length < 32) {
            memcpy(m_buffer + m_bufferSize, p, length);
            m_bufferSize += static_cast<int>(length);
            return;
        }

        if (m_bufferSize > 0) {
            size_t fillLen = 32 - m_bufferSize;
            memcpy(m_buffer + m_bufferSize, p, fillLen);
            p += fillLen;
            processBlock(m_buffer);
            m_bufferSize = 0;
        }

        while (p + 32 <= end) {
            processBlock(p);
            p += 32;
        }

        if (p < end) {
            m_bufferSize = static_cast<int>(end - p);
            memcpy(m_buffer, p, m_bufferSize);
        }
    }

    quint64 finalize()
    {
        quint64 h64;

        if (m_totalLen >= 32) {
            h64 = rotl64(m_v1, 1) + rotl64(m_v2, 7) + rotl64(m_v3, 12) + rotl64(m_v4, 18);
            h64 = mergeRound(h64, m_v1);
            h64 = mergeRound(h64, m_v2);
            h64 = mergeRound(h64, m_v3);
            h64 = mergeRound(h64, m_v4);
        } else {
            h64 = m_seed + PRIME5;
        }

        h64 += m_totalLen;

        const quint8 *p = m_buffer;
        const quint8 *end = m_buffer + m_bufferSize;

        while (p + 8 <= end) {
            quint64 k1 = readLE64(p);
            k1 *= PRIME2;
            k1 = rotl64(k1, 31);
            k1 *= PRIME1;
            h64 ^= k1;
            h64 = rotl64(h64, 27) * PRIME1 + PRIME4;
            p += 8;
        }

        while (p + 4 <= end) {
            h64 ^= static_cast<quint64>(readLE32(p)) * PRIME1;
            h64 = rotl64(h64, 23) * PRIME2 + PRIME3;
            p += 4;
        }

        while (p < end) {
            h64 ^= static_cast<quint64>(*p) * PRIME5;
            h64 = rotl64(h64, 11) * PRIME1;
            p++;
        }

        h64 ^= h64 >> 33;
        h64 *= PRIME2;
        h64 ^= h64 >> 29;
        h64 *= PRIME3;
        h64 ^= h64 >> 32;

        return h64;
    }

    static quint64 hash(const void *data, size_t length, quint64 seed = 0)
    {
        XXHash64 hasher(seed);
        hasher.update(data, length);
        return hasher.finalize();
    }

private:
    static inline quint64 rotl64(quint64 x, int r)
    {
        return (x << r) | (x >> (64 - r));
    }

    static inline quint64 readLE64(const quint8 *p)
    {
        return static_cast<quint64>(p[0]) |
               (static_cast<quint64>(p[1]) << 8) |
               (static_cast<quint64>(p[2]) << 16) |
               (static_cast<quint64>(p[3]) << 24) |
               (static_cast<quint64>(p[4]) << 32) |
               (static_cast<quint64>(p[5]) << 40) |
               (static_cast<quint64>(p[6]) << 48) |
               (static_cast<quint64>(p[7]) << 56);
    }

    static inline quint32 readLE32(const quint8 *p)
    {
        return static_cast<quint32>(p[0]) |
               (static_cast<quint32>(p[1]) << 8) |
               (static_cast<quint32>(p[2]) << 16) |
               (static_cast<quint32>(p[3]) << 24);
    }

    inline quint64 round(quint64 acc, quint64 input)
    {
        acc += input * PRIME2;
        acc = rotl64(acc, 31);
        acc *= PRIME1;
        return acc;
    }

    inline quint64 mergeRound(quint64 acc, quint64 val)
    {
        val = round(0, val);
        acc ^= val;
        acc = acc * PRIME1 + PRIME4;
        return acc;
    }

    void processBlock(const quint8 *p)
    {
        m_v1 = round(m_v1, readLE64(p));
        m_v2 = round(m_v2, readLE64(p + 8));
        m_v3 = round(m_v3, readLE64(p + 16));
        m_v4 = round(m_v4, readLE64(p + 24));
    }

    quint64 m_seed;
    quint64 m_v1, m_v2, m_v3, m_v4;
    quint64 m_totalLen = 0;
    quint8 m_buffer[32];
    int m_bufferSize = 0;
};

// ============================================================================
// HashResult Implementation
// ============================================================================

bool HashResult::matches(const HashResult &other) const
{
    return success && other.success &&
           mode == other.mode &&
           hash == other.hash;
}

bool HashResult::matchesHex(const QString &hex) const
{
    return success && hexString.compare(hex, Qt::CaseInsensitive) == 0;
}

// ============================================================================
// HashCalculator Private Implementation
// ============================================================================

class HashCalculator::Private
{
public:
    HashMode mode = HashMode::SHA256;
    QCryptographicHash *qtHash = nullptr;
    XXHash64 *xxHash = nullptr;
    qint64 bytesProcessed = 0;

    void createHasher()
    {
        delete qtHash;
        qtHash = nullptr;
        delete xxHash;
        xxHash = nullptr;

        switch (mode) {
        case HashMode::None:
            break;
        case HashMode::MD5:
            qtHash = new QCryptographicHash(QCryptographicHash::Md5);
            break;
        case HashMode::SHA1:
            qtHash = new QCryptographicHash(QCryptographicHash::Sha1);
            break;
        case HashMode::SHA256:
            qtHash = new QCryptographicHash(QCryptographicHash::Sha256);
            break;
        case HashMode::SHA512:
            qtHash = new QCryptographicHash(QCryptographicHash::Sha512);
            break;
        case HashMode::XXH64:
            xxHash = new XXHash64();
            break;
        }
    }

    void reset()
    {
        if (qtHash) {
            qtHash->reset();
        }
        if (xxHash) {
            xxHash->reset();
        }
        bytesProcessed = 0;
    }

    void addData(const char *data, qint64 length)
    {
        if (qtHash) {
            qtHash->addData(QByteArrayView(data, static_cast<qsizetype>(length)));
        }
        if (xxHash) {
            xxHash->update(data, static_cast<size_t>(length));
        }
        bytesProcessed += length;
    }

    HashResult getResult() const
    {
        HashResult result;
        result.mode = mode;
        result.bytesProcessed = bytesProcessed;
        result.success = true;

        if (qtHash) {
            result.hash = qtHash->result();
            result.hexString = QString::fromLatin1(result.hash.toHex());
            result.base64String = QString::fromLatin1(result.hash.toBase64());
        } else if (xxHash) {
            quint64 h = const_cast<XXHash64 *>(xxHash)->finalize();
            result.hash.resize(8);
            for (int i = 0; i < 8; ++i) {
                result.hash[i] = static_cast<char>((h >> (i * 8)) & 0xFF);
            }
            result.hexString = QStringLiteral("%1").arg(h, 16, 16, QLatin1Char('0'));
            result.base64String = QString::fromLatin1(result.hash.toBase64());
        } else if (mode == HashMode::None) {
            result.success = true;
        } else {
            result.success = false;
            result.errorMessage = QStringLiteral("Hash not initialized");
        }

        return result;
    }

    ~Private()
    {
        delete qtHash;
        delete xxHash;
    }
};

// ============================================================================
// HashCalculator Implementation
// ============================================================================

HashCalculator::HashCalculator(HashMode mode, QObject *parent)
    : QObject(parent)
    , d(new Private)
{
    d->mode = mode;
    d->createHasher();
}

HashCalculator::~HashCalculator()
{
    delete d;
}

HashMode HashCalculator::mode() const
{
    return d->mode;
}

void HashCalculator::setMode(HashMode mode)
{
    if (d->mode != mode) {
        d->mode = mode;
        d->createHasher();
    }
}

void HashCalculator::reset()
{
    d->reset();
}

void HashCalculator::addData(const QByteArray &data)
{
    d->addData(data.constData(), data.size());
}

void HashCalculator::addData(const char *data, qint64 length)
{
    d->addData(data, length);
}

HashResult HashCalculator::result() const
{
    return d->getResult();
}

qint64 HashCalculator::bytesProcessed() const
{
    return d->bytesProcessed;
}

HashResult HashCalculator::hash(const QByteArray &data)
{
    reset();
    addData(data);
    return result();
}

HashResult HashCalculator::hashFile(const QString &filePath, HashProgressCallback callback)
{
    HashResult result;
    result.mode = d->mode;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        result.success = false;
        result.errorMessage = QStringLiteral("Failed to open file: %1").arg(file.errorString());
        return result;
    }

    qint64 totalSize = file.size();
    reset();

    // Read in 1MB chunks
    static constexpr qint64 BUFFER_SIZE = 1024 * 1024;
    QByteArray buffer;
    buffer.reserve(static_cast<int>(BUFFER_SIZE));

    while (!file.atEnd()) {
        buffer = file.read(BUFFER_SIZE);
        if (buffer.isEmpty() && file.error() != QFile::NoError) {
            result.success = false;
            result.errorMessage = QStringLiteral("Read error: %1").arg(file.errorString());
            return result;
        }

        addData(buffer);

        if (callback) {
            callback(d->bytesProcessed, totalSize);
        }

        emit progressChanged(d->bytesProcessed, totalSize);
    }

    return this->result();
}

QFuture<HashResult> HashCalculator::hashFileAsync(const QString &filePath)
{
    HashMode mode = d->mode;

    return QtConcurrent::run([filePath, mode]() {
        HashCalculator calc(mode);
        return calc.hashFile(filePath);
    });
}

QString HashCalculator::md5(const QByteArray &data)
{
    return QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Md5).toHex());
}

QString HashCalculator::sha256(const QByteArray &data)
{
    return QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
}

QString HashCalculator::sha256File(const QString &filePath)
{
    HashCalculator calc(HashMode::SHA256);
    HashResult result = calc.hashFile(filePath);
    return result.success ? result.hexString : QString();
}

quint64 HashCalculator::xxhash64(const QByteArray &data, quint64 seed)
{
    return XXHash64::hash(data.constData(), static_cast<size_t>(data.size()), seed);
}

QString HashCalculator::modeToString(HashMode mode)
{
    switch (mode) {
    case HashMode::None:   return QStringLiteral("none");
    case HashMode::MD5:    return QStringLiteral("md5");
    case HashMode::SHA1:   return QStringLiteral("sha1");
    case HashMode::SHA256: return QStringLiteral("sha256");
    case HashMode::SHA512: return QStringLiteral("sha512");
    case HashMode::XXH64:  return QStringLiteral("xxh64");
    }
    return QStringLiteral("unknown");
}

HashMode HashCalculator::stringToMode(const QString &str)
{
    QString lower = str.toLower().trimmed();
    if (lower == QLatin1String("md5")) return HashMode::MD5;
    if (lower == QLatin1String("sha1")) return HashMode::SHA1;
    if (lower == QLatin1String("sha256") || lower == QLatin1String("sha-256")) return HashMode::SHA256;
    if (lower == QLatin1String("sha512") || lower == QLatin1String("sha-512")) return HashMode::SHA512;
    if (lower == QLatin1String("xxh64") || lower == QLatin1String("xxhash64")) return HashMode::XXH64;
    return HashMode::None;
}

int HashCalculator::hashLength(HashMode mode)
{
    switch (mode) {
    case HashMode::None:   return 0;
    case HashMode::MD5:    return 16;
    case HashMode::SHA1:   return 20;
    case HashMode::SHA256: return 32;
    case HashMode::SHA512: return 64;
    case HashMode::XXH64:  return 8;
    }
    return 0;
}

} // namespace qltfs
