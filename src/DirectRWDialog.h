#ifndef DIRECTRWDIALOG_H
#define DIRECTRWDIALOG_H

#include <QDialog>
#include <QList>
#include <QModelIndex>
#include <QStandardItemModel>
#include "DeviceManager.h"
#include "TapeModel.h"

namespace Ui {
class DirectRWDialog;
}

class DirectRWDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DirectRWDialog(const QList<TapeDeviceInfo>& devices, QWidget *parent = nullptr);
    ~DirectRWDialog();

private slots:
    void on_btnRefresh_clicked();
    void on_btnRewind_clicked();
    void on_btnLocate_clicked();
    void on_btnEject_clicked();
    void on_btnAddFiles_clicked();
    void on_btnAddFolder_clicked();
    void on_btnRename_clicked();
    void on_btnDelete_clicked();
    void on_btnImportDir_clicked();
    void on_treeTapeCurrent_clicked(const QModelIndex &index);

private:
    void populateDevices();
    void loadCurrentTape();
    void rebuildTree();
    void rebuildChildren(const QString &dirPath);
    QStandardItem *buildItemFromNode(const TapeNode &node, const QString &currentPath);
    QString itemPath(QStandardItem *item) const;
    void appendLog(const QString &line);
    QString currentDevicePath() const;
    const TapeNode *findNode(const QString &path) const;
    bool ensureMountValid();

    Ui::DirectRWDialog *ui;
    QList<TapeDeviceInfo> m_devices;
    DeviceManager m_deviceManager;
    TapeNode m_currentTree;
    QStandardItemModel *m_treeModel = nullptr;
    QStandardItemModel *m_childrenModel = nullptr;
    QString m_selectedPath;
};

#endif // DIRECTRWDIALOG_H
