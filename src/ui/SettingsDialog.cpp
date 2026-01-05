#include "SettingsDialog.h"
#include "ui_SettingsDialog.h"
#include "../utils/SettingsManager.h"
#include <QFileDialog>

SettingsDialog::SettingsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SettingsDialog)
{
    ui->setupUi(this);
    loadSettings();
}

SettingsDialog::~SettingsDialog()
{
    delete ui;
}

void SettingsDialog::loadSettings()
{
    ui->lineEditLtfs->setText(SettingsManager::instance().ltfsBinaryPath());
    ui->lineEditMkltfs->setText(SettingsManager::instance().mkltfsBinaryPath());
}

void SettingsDialog::saveSettings()
{
    SettingsManager::instance().setLtfsBinaryPath(ui->lineEditLtfs->text());
    SettingsManager::instance().setMkltfsBinaryPath(ui->lineEditMkltfs->text());
}

void SettingsDialog::on_btnBrowseLtfs_clicked()
{
    QString path = QFileDialog::getOpenFileName(this, "Select LTFS Binary");
    if (!path.isEmpty()) {
        ui->lineEditLtfs->setText(path);
    }
}

void SettingsDialog::on_btnBrowseMkltfs_clicked()
{
    QString path = QFileDialog::getOpenFileName(this, "Select mkltfs Binary");
    if (!path.isEmpty()) {
        ui->lineEditMkltfs->setText(path);
    }
}

void SettingsDialog::on_buttonBox_accepted()
{
    saveSettings();
    accept();
}
