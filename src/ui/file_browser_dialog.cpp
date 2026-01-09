#include "file_browser_dialog.h"

#include "ui_file_browser_dialog.h"

#include <QClipboard>
#include <QDialogButtonBox>
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

    populateStubTree();
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

void FileBrowserDialog::populateStubTree() {
    model_.removeRows(0, model_.rowCount());

    auto *root = model_.invisibleRootItem();

    auto addFile = [](QStandardItem *parent, const QString &name, qint64 size, const QString &path) {
        auto *nameItem = new QStandardItem(name);
        nameItem->setCheckable(true);
        nameItem->setCheckState(Qt::Unchecked);
        nameItem->setEditable(false);

        auto *sizeItem = new QStandardItem(QString::number(size / 1024));
        sizeItem->setEditable(false);
        auto *pathItem = new QStandardItem(path);
        pathItem->setEditable(false);
        parent->appendRow({nameItem, sizeItem, pathItem});
    };

    auto *dirA = new QStandardItem(QStringLiteral("ProjectA"));
    dirA->setCheckable(true);
    dirA->setCheckState(Qt::Unchecked);
    dirA->setEditable(false);
    root->appendRow({dirA, new QStandardItem(QString()), new QStandardItem(QStringLiteral("/ProjectA"))});
    addFile(dirA, QStringLiteral("clip001.mov"), 4'000'000, QStringLiteral("/ProjectA/clip001.mov"));
    addFile(dirA, QStringLiteral("clip002.mov"), 2'400'000, QStringLiteral("/ProjectA/clip002.mov"));

    auto *dirB = new QStandardItem(QStringLiteral("ProjectB"));
    dirB->setCheckable(true);
    dirB->setCheckState(Qt::Unchecked);
    dirB->setEditable(false);
    root->appendRow({dirB, new QStandardItem(QString()), new QStandardItem(QStringLiteral("/ProjectB"))});
    addFile(dirB, QStringLiteral("assets.zip"), 150'000, QStringLiteral("/ProjectB/assets.zip"));
    addFile(dirB, QStringLiteral("notes.txt"), 12'000, QStringLiteral("/ProjectB/notes.txt"));
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
