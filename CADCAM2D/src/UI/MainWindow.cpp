#include "UI/MainWindow.h"

#include "Features/Editing/AddShapeCommand.h"
#include "Rendering/CadView.h"

#include <QKeySequence>
#include <QLabel>
#include <QListWidget>
#include <QSplitter>
#include <QStatusBar>
#include <QTabWidget>
#include <QToolBar>
#include <QVBoxLayout>

#include <utility>

namespace cadcam {

MainWindow::MainWindow()
{
    setWindowTitle(QStringLiteral("CADCAM · 二维板材"));
    resize(1260, 820);

    auto* workspace = new QSplitter(this);
    auto* partPanel = new QWidget(workspace);
    auto* partLayout = new QVBoxLayout(partPanel);
    partLayout->addWidget(new QLabel(QStringLiteral("零件列表"), partPanel));
    parts_ = new QListWidget(partPanel);
    partLayout->addWidget(parts_);

    canvas_ = new CadView(workspace);
    lists_ = new QTabWidget(workspace);
    results_ = new QListWidget(lists_);
    sheets_ = new QListWidget(lists_);
    lists_->addTab(results_, QStringLiteral("排样结果"));
    lists_->addTab(sheets_, QStringLiteral("板材"));
    workspace->addWidget(partPanel);
    workspace->addWidget(canvas_);
    workspace->addWidget(lists_);
    workspace->setStretchFactor(1, 1);
    workspace->setSizes({200, 820, 240});
    setCentralWidget(workspace);

    auto* toolbar = addToolBar(QStringLiteral("工程"));
    toolbar->setMovable(false);
    toolbar->addAction(QStringLiteral("新建工程"), this, [this] { reset(Project{}); });
    toolbar->addAction(QStringLiteral("示例工程"), this, [this] { reset(exampleProject()); });
    toolbar->addSeparator();
    toolbar->addAction(QStringLiteral("在底图添加矩形"), this, [this] { addRectangle(); });
    toolbar->addSeparator();
    QAction* undo = undo_.createUndoAction(this, QStringLiteral("撤销"));
    undo->setShortcut(QKeySequence::Undo);
    toolbar->addAction(undo);
    QAction* redo = undo_.createRedoAction(this, QStringLiteral("重做"));
    redo->setShortcut(QKeySequence::Redo);
    toolbar->addAction(redo);

    connect(parts_, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) {
        switchZone({ZoneKind::Part, project_.parts.at(parts_->row(item)).id}, item->text());
    });
    connect(sheets_, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) {
        switchZone({ZoneKind::Sheet, project_.sheets.at(sheets_->row(item)).id}, item->text());
    });
    connect(results_, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) {
        const int row = results_->row(item);
        const auto source = row == 0
            ? ZoneSource{ZoneKind::BaseDrawing, 0}
            : ZoneSource{ZoneKind::NestingResult, std::get<NestedSheet>(project_.nestingResults.at(row)).id};
        switchZone(source, item->text());
    });
    connect(&undo_, &QUndoStack::indexChanged, this, [this] { switchZone(zone_.source, zoneTitle_); });
}

void MainWindow::loadExample()
{
    reset(exampleProject());
}

bool MainWindow::smoke()
{
    if (zone_.items.size() != 3) {
        return false;
    }
    switchZone({ZoneKind::Part, 11}, QStringLiteral("安装板"));
    if (zone_.items.size() != 3) {
        return false;
    }
    switchZone({ZoneKind::Sheet, 21}, QStringLiteral("板材 A"));
    if (zone_.items.size() != 1) {
        return false;
    }
    switchZone({ZoneKind::NestingResult, 31}, QStringLiteral("排样 1"));
    if (zone_.items.size() != 7) {
        return false;
    }
    addRectangle();
    if (zone_.items.size() != 4) {
        return false;
    }
    undo_.undo();
    if (zone_.items.size() != 3) {
        return false;
    }
    undo_.redo();
    return zone_.items.size() == 4;
}

void MainWindow::reset(Project project)
{
    // Discard commands before replacing the document their records refer to.
    undo_.clear();
    project_ = std::move(project);
    parts_->clear();
    sheets_->clear();
    results_->clear();
    results_->addItem(QStringLiteral("底图"));
    for (const Part& part : project_.parts) {
        parts_->addItem(QString::fromStdString(part.name));
    }
    for (const Sheet& sheet : project_.sheets) {
        sheets_->addItem(QString::fromStdString(sheet.name));
    }
    for (std::size_t i = 1; i < project_.nestingResults.size(); ++i) {
        results_->addItem(QString::fromStdString(std::get<NestedSheet>(project_.nestingResults[i]).name));
    }
    lists_->setCurrentIndex(0);
    results_->setCurrentRow(0);
    switchZone({ZoneKind::BaseDrawing, 0}, QStringLiteral("底图"));
}

void MainWindow::addRectangle()
{
    lists_->setCurrentIndex(0);
    results_->setCurrentRow(0);
    switchZone({ZoneKind::BaseDrawing, 0}, QStringLiteral("底图"));
    auto& base = std::get<BaseDrawing>(project_.nestingResults.front());
    const double offset = 10.0 * base.shapes.size();
    undo_.push(new AddShapeCommand(project_, Shape{nextId_++,
        Primitive{Rectangle{{140 + offset, offset}, 35, 24}}, {}}));
}

void MainWindow::switchZone(ZoneSource source, const QString& title)
{
    zone_ = buildZone(project_, source);
    zoneTitle_ = title;
    canvas_->draw(zone_);
    statusBar()->showMessage(QStringLiteral("%1  ·  %2 个显示对象  ·  双击列表切换视图")
        .arg(title).arg(zone_.items.size()));
}

}
