#include "TapeModel.h"
#include <QDirIterator>
#include <QFileInfo>
#include <QDir>
#include <QSet>

namespace {

static TapeNode makeNode(const QFileInfo &info, const QString &basePath)
{
    TapeNode node;
    node.name = info.fileName();
    node.isDir = info.isDir();
    if (!node.isDir) node.size = info.size();
    node.sourcePath = info.absoluteFilePath();

    if (info.isDir()) {
        QDir dir(info.absoluteFilePath());
        const auto entries = dir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllDirs | QDir::Files, QDir::DirsFirst);
        for (const QFileInfo &childInfo : entries) {
            node.children.append(makeNode(childInfo, basePath));
        }
    }
    return node;
}

static QString concatPath(const QString &base, const QString &name)
{
    if (base.isEmpty()) return name;
    if (base == "/") return "/" + name;
    return base + "/" + name;
}

static void flatten(const TapeNode &node, const QString &path, QMap<QString, const TapeNode*> &out)
{
    const QString currentPath = concatPath(path, node.name);
    out.insert(currentPath, &node);
    if (node.isDir) {
        for (const auto &child : node.children) {
            flatten(child, currentPath, out);
        }
    }
}

static void collectPaths(const TapeNode &node, const QString &path, QStringList &outFiles, QStringList &outDirs)
{
    const QString currentPath = concatPath(path, node.name);
    if (node.isDir) {
        outDirs << currentPath;
        for (const auto &child : node.children) collectPaths(child, currentPath, outFiles, outDirs);
    } else {
        outFiles << currentPath;
    }
}

}

namespace TapeModel {

TapeNode makeSampleTree()
{
    TapeNode root;
    root.name = "/";
    root.isDir = true;

    TapeNode d1; d1.name = "Projects"; d1.isDir = true;
    TapeNode f1; f1.name = "hello.txt"; f1.isDir = false; f1.size = 128;
    TapeNode f2; f2.name = "readme.md"; f2.isDir = false; f2.size = 2048;
    d1.children.append(f1);
    d1.children.append(f2);

    TapeNode d2; d2.name = "Media"; d2.isDir = true;
    TapeNode f3; f3.name = "clip.mov"; f3.isDir = false; f3.size = 15 * 1024 * 1024;
    TapeNode f4; f4.name = "cover.jpg"; f4.isDir = false; f4.size = 240000;
    d2.children.append(f3);
    d2.children.append(f4);

    root.children.append(d1);
    root.children.append(d2);
    return root;
}

TapeNode fromDirectory(const QString &path, const QString &baseName)
{
    QFileInfo rootInfo(path);
    TapeNode root;
    root.name = baseName.isEmpty() ? rootInfo.fileName() : baseName;
    root.isDir = true;
    root.sourcePath = rootInfo.absoluteFilePath();

    QDir dir(path);
    const auto entries = dir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllDirs | QDir::Files, QDir::DirsFirst);
    for (const QFileInfo &info : entries) {
        root.children.append(makeNode(info, path));
    }
    return root;
}

TapeDiffResult diff(const TapeNode &oldTree, const TapeNode &newTree)
{
    TapeDiffResult res;
    QMap<QString, const TapeNode*> oldMap;
    QMap<QString, const TapeNode*> newMap;
    flatten(oldTree, QString(), oldMap);
    flatten(newTree, QString(), newMap);

    QStringList oldDirs, oldFiles, newDirs, newFiles;
    collectPaths(oldTree, QString(), oldFiles, oldDirs);
    collectPaths(newTree, QString(), newFiles, newDirs);

    const QSet<QString> oldFileSet = QSet<QString>(oldFiles.begin(), oldFiles.end());
    const QSet<QString> newFileSet = QSet<QString>(newFiles.begin(), newFiles.end());
    const QSet<QString> oldDirSet = QSet<QString>(oldDirs.begin(), oldDirs.end());
    const QSet<QString> newDirSet = QSet<QString>(newDirs.begin(), newDirs.end());

    for (const QString &p : newDirSet) {
        if (!oldDirSet.contains(p)) {
            res.entries.append({TapeDiffEntry::AddedDirectory, p, 0});
        }
    }
    for (const QString &p : oldDirSet) {
        if (!newDirSet.contains(p)) {
            res.entries.append({TapeDiffEntry::RemovedDirectory, p, 0});
        }
    }

    for (const QString &p : newFileSet) {
        if (!oldFileSet.contains(p)) {
            const auto *n = newMap.value(p);
            res.entries.append({TapeDiffEntry::AddedFile, p, n ? n->size : 0});
        }
    }
    for (const QString &p : oldFileSet) {
        if (!newFileSet.contains(p)) {
            res.entries.append({TapeDiffEntry::RemovedFile, p, 0});
        } else {
            const auto *oldNode = oldMap.value(p);
            const auto *newNode = newMap.value(p);
            if (oldNode && newNode && oldNode->size != newNode->size) {
                res.entries.append({TapeDiffEntry::ModifiedFile, p, newNode->size});
            }
        }
    }
    return res;
}

}
