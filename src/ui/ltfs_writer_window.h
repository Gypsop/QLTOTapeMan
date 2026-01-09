#pragma once

#include <QMainWindow>
#include <QStandardItemModel>
#include <QTimer>
#include <memory>

class QLabel;
class QProgressBar;

namespace Ui {
class LTFSWriterWindow;
}

namespace qlto {

class FileBrowserDialog;

class LTFSWriterWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit LTFSWriterWindow(QWidget *parent = nullptr);
    ~LTFSWriterWindow() override;

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
    void populateStubTree();
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
};

} // namespace qlto
