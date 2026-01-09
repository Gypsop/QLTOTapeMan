#include "file_browser_dialog.h"

#include "ui_file_browser_dialog.h"

#include <QClipboard>
#include <QDialogButtonBox>
#include <QDir>
#include <QGuiApplication>
#include <QHeaderView>
#include <QRegularExpression>
#include <QStandardItem>

namespace qlto {

FileBrowserDialog::FileBrowserDialog(QWidget *parent)
    : QDialog(parent), ui(std::make_unique<Ui::FileBrowserDialog>()) {
    ui->setupUi(this);

    model_.setHorizontalHeaderLabels({tr("Name"), tr("Size"), tr("Path")});
    ui->treeView->setModel(&model_);
    ui->treeView->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    ui->treeView->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    ui->treeView->header()->setSectionResizeMode(2, QHeaderView::Stretch);

    connect(ui->selectAllButton, &QPushButton::clicked, this, &FileBrowserDialog::selectAll);
    connect(ui->selectNoneButton, &QPushButton::clicked, this, &FileBrowserDialog::selectNone);
    connect(ui->selectBySizeButton, &QPushButton::clicked, this, &FileBrowserDialog::selectBySize);
    connect(ui->regexApplyButton, &QPushButton::clicked, this, &FileBrowserDialog::applyRegex);

    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &FileBrowserDialog::acceptAndCollect);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &FileBrowserDialog::reject);

    populateFileTree();
    ui->treeView->expandAll();
}

FileBrowserDialog::~FileBrowserDialog() = default;

QStringList FileBrowserDialog::selectedPaths() const { return selected_; }

bool FileBrowserDialog::copyToClipboard() const { return ui->clipboardCheck->isChecked(); }

void FileBrowserDialog::selectAll() { setCheckStateRecursive(model_.invisibleRootItem(), Qt::Checked); }

void FileBrowserDialog::selectNone() { setCheckStateRecursive(model_.invisibleRootItem(), Qt::Unchecked); }

void FileBrowserDialog::selectBySize() {
    const int thresholdKb = ui->sizeSpin->value();
    selectByPredicate([thresholdKb](QStandardItem *item) {
        if (!item || item->rowCount() > 0) return false; // directories skipped
        const QModelIndex idx = item->index();
        const QString sizeText = idx.sibling(idx.row(), 1).data(Qt::DisplayRole).toString();
        const QString numeric = sizeText.split(' ').first();
        bool ok = false;
        const int sizeVal = numeric.toInt(&ok);
        return ok && sizeVal >= thresholdKb;
    });
}

void FileBrowserDialog::applyRegex() {
    const QRegularExpression re(ui->regexEdit->text());
    if (!re.isValid()) return;
    selectByPredicate([&re](QStandardItem *item) {
        if (!item || item->rowCount() > 0) return false;
        return re.match(item->text()).hasMatch();
    });
}

void FileBrowserDialog::acceptAndCollect() {
    selected_.clear();
    QList<QStandardItem *> stack;
    stack.append(model_.invisibleRootItem());
    while (!stack.isEmpty()) {
        QStandardItem *node = stack.takeFirst();
        for (int i = 0; i < node->rowCount(); ++i) {
            auto *child = node->child(i, 0);
            if (!child) continue;
            if (child->checkState() == Qt::Checked && child->rowCount() == 0) {
                const QModelIndex idx = child->index();
                const QString path = idx.sibling(idx.row(), 2).data(Qt::DisplayRole).toString();
                selected_.push_back(path);
            }
            stack.append(child);
        }
    }
    if (copyToClipboard() && !selected_.isEmpty()) {
        QGuiApplication::clipboard()->setText(selected_.join('\n'));
    }
    accept();
}

void FileBrowserDialog::populateFileTree() {
    model_.removeRows(0, model_.rowCount());
    QDir base(QDir::homePath());
    auto *root = new QStandardItem(base.dirName().isEmpty() ? base.absolutePath() : base.dirName());
    root->setCheckable(true);
    root->setCheckState(Qt::Unchecked);
    root->setEditable(false);
    auto *sizeRoot = new QStandardItem(QString());
    auto *pathRoot = new QStandardItem(base.absolutePath());
    model_.appendRow({root, sizeRoot, pathRoot});

    std::function<void(QStandardItem *, const QDir &, int)> addDir = [&](QStandardItem *parent, const QDir &dir, int depth) {
        if (depth > 3) return; // limit depth for performance
        const auto entries = dir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries, QDir::DirsFirst | QDir::Name);
        for (const auto &info : entries) {
            auto *nameItem = new QStandardItem(info.fileName());
            nameItem->setCheckable(true);
            nameItem->setCheckState(Qt::Unchecked);
            nameItem->setEditable(false);
            auto *sizeItem = new QStandardItem(info.isDir() ? QString() : QString::number(info.size() / 1024));
            auto *pathItem = new QStandardItem(info.absoluteFilePath());
            parent->appendRow({nameItem, sizeItem, pathItem});
            if (info.isDir()) {
                addDir(nameItem, QDir(info.absoluteFilePath()), depth + 1);
            }
        }
    };

    addDir(root, base, 0);
}

void FileBrowserDialog::setCheckStateRecursive(QStandardItem *item, Qt::CheckState state) {
    if (!item) return;
    for (int i = 0; i < item->rowCount(); ++i) {
        auto *child = item->child(i, 0);
        if (!child) continue;
        child->setCheckState(state);
        setCheckStateRecursive(child, state);
    }
}

void FileBrowserDialog::selectByPredicate(const std::function<bool(QStandardItem *)> &pred) {
    setCheckStateRecursive(model_.invisibleRootItem(), Qt::Unchecked);
    QList<QStandardItem *> stack;
    stack.append(model_.invisibleRootItem());
    while (!stack.isEmpty()) {
        auto *node = stack.takeFirst();
        for (int i = 0; i < node->rowCount(); ++i) {
            auto *child = node->child(i, 0);
            if (!child) continue;
            if (pred(child)) child->setCheckState(Qt::Checked);
            stack.append(child);
        }
    }
}

} // namespace qlto
