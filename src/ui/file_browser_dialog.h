#pragma once

#include <QDialog>
#include <QStandardItemModel>
#include <functional>
#include <memory>

namespace Ui {
class FileBrowserDialog;
}

namespace qlto {

class FileBrowserDialog : public QDialog {
    Q_OBJECT
public:
    explicit FileBrowserDialog(QWidget *parent = nullptr);
    ~FileBrowserDialog() override;

    QStringList selectedPaths() const;
    bool copyToClipboard() const;

private slots:
    void selectAll();
    void selectNone();
    void selectBySize();
    void applyRegex();
    void acceptAndCollect();

private:
     void populateFileTree();
    void setCheckStateRecursive(QStandardItem *item, Qt::CheckState state);
    void selectByPredicate(const std::function<bool(QStandardItem *)> &pred);

    std::unique_ptr<Ui::FileBrowserDialog> ui;
    QStandardItemModel model_;
    QStringList selected_;
};

} // namespace qlto
