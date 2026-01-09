#ifndef TAPEMODEL_H
#define TAPEMODEL_H

#include <QString>
#include <QList>
#include <QMap>

struct TapeNode {
    QString name;
    bool isDir = true;
    qint64 size = 0;
    QString sourcePath; // absolute path for staged files (host filesystem); empty for existing tape entries
    QList<TapeNode> children;
};

struct TapeDiffEntry {
    enum Kind {
        AddedFile,
        AddedDirectory,
        RemovedFile,
        RemovedDirectory,
        ModifiedFile
    } kind;
    QString path;
    qint64 size = 0;
};

struct TapeDiffResult {
    QList<TapeDiffEntry> entries;
};

namespace TapeModel {

TapeNode makeSampleTree();
TapeNode fromDirectory(const QString &path, const QString &baseName = QString());
TapeDiffResult diff(const TapeNode &oldTree, const TapeNode &newTree);

}

#endif // TAPEMODEL_H
