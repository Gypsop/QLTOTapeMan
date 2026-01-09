#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QList>
#include "DeviceManager.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_btnRefresh_clicked();
    void on_btnDirectRW_clicked();

private:
    void refreshDeviceList();

    Ui::MainWindow *ui;
    DeviceManager m_deviceManager;
    QList<TapeDeviceInfo> m_devices;
};

#endif // MAINWINDOW_H
