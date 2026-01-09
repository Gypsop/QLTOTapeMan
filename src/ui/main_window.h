#pragma once

#include <QMainWindow>
#include <QStandardItemModel>
#include <memory>

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
    void populateStubDevices();

    std::unique_ptr<Ui::MainWindow> ui;
    QStandardItemModel deviceModel_;
    std::unique_ptr<LTFSWriterWindow> writerWindow_;
};

} // namespace qlto
