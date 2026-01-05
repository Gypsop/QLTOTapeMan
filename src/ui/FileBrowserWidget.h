#ifndef FILEBROWSERWIDGET_H
#define FILEBROWSERWIDGET_H

#include <QWidget>
#include <QTreeWidget>
#include "../ltfs/LtfsElements.h"

namespace Ui {
class FileBrowserWidget;
}

class FileBrowserWidget : public QWidget
{
    Q_OBJECT

public:
    explicit FileBrowserWidget(QWidget *parent = nullptr);
    ~FileBrowserWidget();

    void loadIndex(const LtfsIndex &index);
    void clear();

signals:
    void filesDropped(const QStringList &files);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private slots:
    void on_txtSearch_textChanged(const QString &arg1);

private:
    Ui::FileBrowserWidget *ui;
    LtfsIndex m_currentIndex;

    void populateTree(const LtfsDirectory &dir, QTreeWidgetItem *parentItem);
    QString formatSize(long long size);
};

#endif // FILEBROWSERWIDGET_H
