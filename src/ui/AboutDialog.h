#ifndef ABOUTDIALOG_H
#define ABOUTDIALOG_H

#include <QDialog>
#include <QTableWidget>

namespace Ui {
class AboutDialog;
}

class AboutDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AboutDialog(QWidget *parent = nullptr);
    ~AboutDialog();

private:
    Ui::AboutDialog *ui;
    QTableWidget *m_sysTable;
    QTableWidget *m_ltfsTable;
    
    void setupUiCustom();
    QTableWidget* createGroupTable(const QString &title, QWidget *parentLayoutWidget);
    void addRowToTable(QTableWidget *table, const QString &key, const QString &value, bool isError = false);
    
    void collectSystemInfo();
    QString getOsInfo();
    void getLtfsVersion();
    QString getHardwareInfo();
};

#endif // ABOUTDIALOG_H
