#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QProgressBar>
#include <QLabel>
#include <QFutureWatcher>
#include "device/DeviceManager.h"
#include "device/LtfsManager.h"
#include "ltfs/IndexManager.h"
#include "ui/FileBrowserWidget.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_btnScan_clicked();
    void on_treeDevices_itemSelectionChanged();
    
    void on_btnStatus_clicked();
    void on_btnRewind_clicked();
    void on_btnEject_clicked();
    void on_btnFormat_clicked();
    void on_btnMount_clicked();
    void on_btnUnmount_clicked();

    void onLtfsOperationFinished(const QString &operation, bool success, const QString &message);
    void onLtfsOutputReceived(const QString &text);
    
    // Async slots
    void onAsyncOperationFinished();

    // Menu slots
    void on_actionSettings_triggered();
    void on_actionExit_triggered();
    void on_actionAbout_triggered();
    
    void on_tabWidget_currentChanged(int index);
    
    void onFilesDropped(const QStringList &files);

private:
    Ui::MainWindow *ui;
    DeviceManager *m_deviceManager;
    LtfsManager *m_ltfsManager;
    IndexManager *m_indexManager;
    FileBrowserWidget *m_fileBrowser;
    
    // Status Bar Widgets
    QProgressBar *m_progressBar;
    QLabel *m_statusLabel;
    
    // Async Watcher
    QFutureWatcher<bool> m_futureWatcher;
    
    QString m_currentMountSerial;
    QString m_currentMountPoint;

    QString m_currentAsyncOperation;
    
    QString getSelectedDevicePath();
    void logMessage(const QString &message);
    void setBusy(bool busy, const QString &message = QString());
};
#endif // MAINWINDOW_H
