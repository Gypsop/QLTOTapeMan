#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>

namespace Ui {
class SettingsDialog;
}

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);
    ~SettingsDialog();

private slots:
    void on_btnBrowseLtfs_clicked();
    void on_btnBrowseMkltfs_clicked();
    void on_btnBrowseLtfsck_clicked();
    void on_buttonBox_accepted();

private:
    Ui::SettingsDialog *ui;
    void loadSettings();
    void saveSettings();
};

#endif // SETTINGSDIALOG_H
