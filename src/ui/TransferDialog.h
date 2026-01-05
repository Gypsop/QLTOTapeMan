#pragma once

#include <QDialog>
#include <QProgressBar>
#include <QLabel>
#include <QTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include "../transfer/TransferEngine.h"

#include "../device/DeviceManager.h"

class TransferDialog : public QDialog {
    Q_OBJECT
public:
    TransferDialog(const QStringList& files, const QString& dest, DeviceManager* deviceManager = nullptr, QWidget* parent = nullptr);
    ~TransferDialog();

private slots:
    void onProgress(quint64 bytes, quint64 total, double speed);
    void onFileStarted(QString name);
    void onFileFinished(QString name, QString checksum);
    void onError(QString error);
    void onFinished();
    void onCancel();

private:
    TransferEngine* m_engine;
    QProgressBar* m_progressBar;
    QLabel* m_statusLabel;
    QLabel* m_speedLabel;
    QTextEdit* m_logView;
    QPushButton* m_cancelButton;
    QPushButton* m_closeButton;
};
