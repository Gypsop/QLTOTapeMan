#pragma once

#include <QMainWindow>
#include <QStandardItemModel>
#include <memory>

#include "../io/tape_device.h"

namespace Ui {
class MainWindow;
}

namespace qlto {

class LTFSWriterWindow;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void refreshDevices();
    void toggleDebugPanel();
    void openWriter();
    void onDeviceSelected();
    void onActionTriggered();
    void exportLogSense();
    void sendScsi();

private:
    void appendLog(const QString &text);
    void populateDevices();
    void openCurrentDevice();

    std::unique_ptr<Ui::MainWindow> ui;
    QStandardItemModel deviceModel_;
    std::unique_ptr<LTFSWriterWindow> writerWindow_;
    std::unique_ptr<class TapeEnumerator> enumerator_;
    std::unique_ptr<class TapeDevice> device_;
    SenseData sense_{};
    QString openedPath_;
};

} // namespace qlto
