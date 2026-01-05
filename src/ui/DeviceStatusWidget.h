#ifndef DEVICESTATUSWIDGET_H
#define DEVICESTATUSWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QHBoxLayout>

class DeviceStatusWidget : public QWidget
{
    Q_OBJECT
public:
    explicit DeviceStatusWidget(QWidget *parent = nullptr);

    void setStatus(const QString &statusName, const QString &color, const QString &text = QString(), const QString &textColor = "white");
    void reset();

private:
    QLabel *m_ledOpStatus;
    QLabel *m_ledEncryption;
    QLabel *m_ledCleaning;
    QLabel *m_ledTapeStatus;
    QLabel *m_ledDriveStatus;
    QLabel *m_ledActivity;

    void setupLed(QLabel *label, const QString &text);
    void setLedStyle(QLabel *label, const QString &color, const QString &text, const QString &textColor);
};

#endif // DEVICESTATUSWIDGET_H
