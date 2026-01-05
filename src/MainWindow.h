#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QProgressBar>
#include <QLabel>
#include <QFutureWatcher>
#include "device/DeviceManager.h"
#include "device/LtfsManager.h"

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

private:
    Ui::MainWindow *ui;
    DeviceManager *m_deviceManager;
    LtfsManager *m_ltfsManager;
    
    // Status Bar Widgets
    QProgressBar *m_progressBar;
    QLabel *m_statusLabel;
    
    // Async Watcher
    QFutureWatcher<bool> m_futureWatcher;
    QString m_currentAsyncOperation;
    
    QString getSelectedDevicePath();
    void logMessage(const QString &message);
    void setBusy(bool busy, const QString &message = QString());
};
#endif // MAINWINDOW_H
