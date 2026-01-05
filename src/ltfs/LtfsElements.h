#ifndef LTFSELEMENTS_H
#define LTFSELEMENTS_H

#include <QString>
#include <QList>
#include <QDateTime>
#include <QVariant>

// Represents an extent (a chunk of data on tape)
struct LtfsExtent {
    quint64 startBlock; // Physical start block on tape
    quint64 byteOffset; // Offset within the file
    quint64 byteCount;  // Length of this extent
    QString partition;  // Partition ID (a or b)
    quint64 fileOffset; // Offset in the file where this extent belongs
};

// Represents a file in the LTFS index
struct LtfsFile {
    QString name;
    quint64 length;
    bool readonly;
    QDateTime creationTime;
    QDateTime changeTime;
    QDateTime modifyTime;
    QDateTime accessTime;
    QDateTime backupTime;
    QString sha1; // Hash
    QList<LtfsExtent> extents; // List of extents
    
    // Helper to get the first block (for sorting)
    quint64 firstBlock() const {
        if (extents.isEmpty()) return 0;
        return extents.first().startBlock;
    }
};

// Represents a directory in the LTFS index
struct LtfsDirectory {
    QString name;
    QDateTime creationTime;
    QDateTime changeTime;
    QDateTime modifyTime;
    QDateTime accessTime;
    QDateTime backupTime;
    bool readonly;
    
    QList<LtfsFile> files;
    QList<LtfsDirectory> subdirectories;
};

// Represents the entire LTFS Index
struct LtfsIndex {
    QString volumeUuid;
    QString creator;
    quint64 generationNumber;
    QDateTime updateTime;
    QString locationPartition;
    quint64 locationStartBlock;
    
    LtfsDirectory rootDirectory;
};

#endif // LTFSELEMENTS_H
