#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "DirectRWDialog.h"
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    connect(ui->btnRefresh, &QPushButton::clicked, this, &MainWindow::on_btnRefresh_clicked);
    connect(ui->btnDirectRW, &QPushButton::clicked, this, &MainWindow::on_btnDirectRW_clicked);
    on_btnRefresh_clicked();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::refreshDeviceList()
{
    ui->comboDevices->clear();
    for (const auto &dev : m_devices) {
        const QString label = dev.displayName.isEmpty() ? dev.devicePath : dev.displayName;
        ui->comboDevices->addItem(label, dev.devicePath);
    }
    ui->btnDirectRW->setEnabled(!m_devices.isEmpty() && ui->comboDevices->count() > 0);
}

void MainWindow::on_btnRefresh_clicked()
{
    m_devices = m_deviceManager.scanDevices();
    refreshDeviceList();
    ui->statusbar->showMessage(tr("Found %1 device(s)").arg(m_devices.size()), 3000);
}

void MainWindow::on_btnDirectRW_clicked()
{
    if (m_devices.isEmpty() || ui->comboDevices->currentIndex() < 0) {
        QMessageBox::information(this, tr("Info"), tr("Please scan and select a device first."));
        return;
    }
    DirectRWDialog dlg(m_devices, this);
    dlg.exec();
}




