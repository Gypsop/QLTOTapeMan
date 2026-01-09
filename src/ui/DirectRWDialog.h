#pragma once

#include <QDialog>
#include <QRadioButton>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QListWidget>
#include <QProgressBar>
#include <QLabel>
#include <QTextEdit>
#include <QCheckBox>
#include "../device/DeviceManager.h"
#include "../transfer/TransferEngine.h"

class DirectRWDialog : public QDialog {
    Q_OBJECT
public:
    explicit DirectRWDialog(DeviceManager* deviceManager, QWidget* parent = nullptr);
    ~DirectRWDialog();

private slots:
    void onModeChanged();
    void onBrowseFiles();
    void onRemoveFiles();
    void onBrowseFolder();
    void onStart();
    void onCancel();
    void onFinished();
    void onProgress(quint64 bytes, quint64 total, double speed);
    void onFileStarted(QString name);
    void onFileFinished(QString name, QString checksum);
    void onError(QString error);

private:
    enum class Mode { FileToTape, TapeToFile, TapeToTape };

    void buildUI();
    void refreshDevices();
    void applyModeUI();
    bool ensureReady();
    QString selectedSourceDevice() const;
    QString selectedDestDevice() const;

    DeviceManager* m_deviceManager = nullptr;
    TransferEngine* m_engine = nullptr;
    QList<TapeDeviceInfo> m_devices;

    // Widgets
    QRadioButton* m_radioFileToTape = nullptr;
    QRadioButton* m_radioTapeToFile = nullptr;
    QRadioButton* m_radioTapeToTape = nullptr;
    QComboBox* m_sourceDeviceCombo = nullptr;
    QComboBox* m_destDeviceCombo = nullptr;
    QCheckBox* m_chkRewindSource = nullptr;
    QCheckBox* m_chkRewindDest = nullptr;
    QListWidget* m_fileList = nullptr;
    QPushButton* m_btnAddFiles = nullptr;
    QPushButton* m_btnRemoveFiles = nullptr;
    QLineEdit* m_destFolderEdit = nullptr;
    QPushButton* m_btnBrowseFolder = nullptr;
    QLineEdit* m_prefixEdit = nullptr;
    QPushButton* m_btnStart = nullptr;
    QPushButton* m_btnCancel = nullptr;
    QPushButton* m_btnClose = nullptr;
    QProgressBar* m_progress = nullptr;
    QLabel* m_statusLabel = nullptr;
    QLabel* m_speedLabel = nullptr;
    QTextEdit* m_log = nullptr;

    Mode m_mode = Mode::FileToTape;
    bool m_running = false;
};
