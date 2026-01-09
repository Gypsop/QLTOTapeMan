#pragma once

#include <QMainWindow>
#include <QStandardItemModel>
#include <QTimer>
#include <memory>
#include <vector>

class QLabel;
class QProgressBar;

namespace Ui {
class LTFSWriterWindow;
}

namespace qlto {

class FileBrowserDialog;
class LtfsService;

class LTFSWriterWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit LTFSWriterWindow(QWidget *parent = nullptr);
    ~LTFSWriterWindow() override;

    void setDrivePath(const QString &path);

private slots:
    void startJob();
    void pauseJob();
    void resumeJob();
    void stopJob();
    void flushJob();
    void ejectTape();
    void browseFiles();
    void updateProgress();
    void handleTreeChanged(QStandardItem *item);
    void handleTreeSelectionChanged(const QModelIndex &index);

private:
    void setupStatusBar();
    void appendLog(const QString &text);
    void ensurePathItem(const QStringList &parts, qint64 sizeBytes);
    void refreshFileTable(const QModelIndex &dirIndex);
    void setStatusLight(const QString &text, const QString &color);
    void setCheckStateRecursive(QStandardItem *item, Qt::CheckState state);
    void updateParentState(QStandardItem *item);

    std::unique_ptr<Ui::LTFSWriterWindow> ui;
    QStandardItemModel treeModel_;
    QStandardItemModel fileModel_;
    QTimer progressTimer_;
    int progressValue_ = 0;
    bool paused_ = false;
    QProgressBar *progressBar_ = nullptr;
    QLabel *statusText_ = nullptr;
    QLabel *speedText_ = nullptr;
    QString drivePath_;
    std::unique_ptr<LtfsService> service_;
    std::vector<QString> selectedFilePaths() const;
    bool ensureServiceReady(std::string &err);
};

} // namespace qlto
