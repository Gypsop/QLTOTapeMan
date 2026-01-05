#ifndef CHECKDIALOG_H
#define CHECKDIALOG_H

#include <QDialog>
#include <QCheckBox>
#include "../device/LtfsManager.h"

class CheckDialog : public QDialog
{
    Q_OBJECT
public:
    explicit CheckDialog(QWidget *parent = nullptr);
    LtfsCheckOptions getOptions() const;

private:
    QCheckBox *m_chkDeepRecovery;
    QCheckBox *m_chkFullRecovery;
    
    void setupUi();
};

#endif // CHECKDIALOG_H
