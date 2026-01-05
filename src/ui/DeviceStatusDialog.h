#pragma once

#include <QDialog>
#include <QLabel>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QProgressBar>
#include "../device/DeviceManager.h"

class DeviceStatusDialog : public QDialog {
    Q_OBJECT
public:
    explicit DeviceStatusDialog(const TapeStatus &status, QWidget *parent = nullptr);

private:
    void setupUi(const TapeStatus &status);
    QString formatSize(uint64_t bytes);
};
