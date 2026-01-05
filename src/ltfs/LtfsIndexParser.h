#ifndef LTFSINDEXPARSER_H
#define LTFSINDEXPARSER_H

#include "LtfsElements.h"
#include <QXmlStreamReader>
#include <QFile>

class LtfsIndexParser
{
public:
    LtfsIndexParser();
    
    // Parse an LTFS index XML file
    bool parse(const QString &filePath, LtfsIndex &index);
    
    QString errorString() const;

private:
    void readLtfsIndex(QXmlStreamReader &xml, LtfsIndex &index);
    void readDirectory(QXmlStreamReader &xml, LtfsDirectory &directory);
    void readFile(QXmlStreamReader &xml, LtfsFile &file);
    void readExtent(QXmlStreamReader &xml, LtfsExtent &extent);
    void readXattr(QXmlStreamReader &xml, QMap<QString, QString> &attributes);
    
    QString m_errorString;
};

#endif // LTFSINDEXPARSER_H
