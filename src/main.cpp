#include "CadView.h"
#include "Project.h"

#include <QApplication>
#include <QKeySequence>
#include <QLabel>
#include <QListWidget>
#include <QMainWindow>
#include <QSplitter>
#include <QStatusBar>
#include <QTabWidget>
#include <QTimer>
#include <QToolBar>
#include <QUndoStack>
#include <QVBoxLayout>

#include <cstdlib>
#include <iostream>
#include <utility>

namespace {

using namespace cadcam;

class AddShape final : public QUndoCommand
{
public:
    AddShape(Project& project, Shape shape)
        : QUndoCommand(QStringLiteral("添加矩形")), project_(project), shape_(std::move(shape))
    {
    }

    void undo() override { drawing().shapes.pop_back(); }
    void redo() override { drawing().shapes.push_back(shape_); }

private:
    BaseDrawing& drawing() { return std::get<BaseDrawing>(project_.nestingResults.front()); }

    Project& project_;
    Shape shape_;
};

class MainWindow final : public QMainWindow
{
public:
    MainWindow()
    {
        setWindowTitle(QStringLiteral("CADCAM · 二维激光工作台"));
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

    void loadExample() { reset(exampleProject()); }

    bool smoke()
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

private:
    void reset(Project project)
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

    void addRectangle()
    {
        lists_->setCurrentIndex(0);
        results_->setCurrentRow(0);
        switchZone({ZoneKind::BaseDrawing, 0}, QStringLiteral("底图"));
        auto& base = std::get<BaseDrawing>(project_.nestingResults.front());
        const double offset = 10.0 * base.shapes.size();
        undo_.push(new AddShape(project_, Shape{nextId_++,
            Primitive{Rectangle{{140 + offset, offset}, 35, 24}}, {}}));
    }

    void switchZone(ZoneSource source, const QString& title)
    {
        zone_ = buildZone(project_, source);
        zoneTitle_ = title;
        canvas_->draw(zone_);
        statusBar()->showMessage(QStringLiteral("%1  ·  %2 个显示对象  ·  双击列表切换视图")
            .arg(title).arg(zone_.items.size()));
    }

    Project project_;
    QUndoStack undo_;
    CurrentZone zone_{{ZoneKind::BaseDrawing, 0}, {}};
    QString zoneTitle_ = QStringLiteral("底图");
    Id nextId_ = 100;
    QListWidget* parts_;
    QListWidget* sheets_;
    QListWidget* results_;
    QTabWidget* lists_;
    CadView* canvas_;
};

}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("CADCAM"));
    MainWindow window;
    window.show();

    const bool smoke = app.arguments().contains(QStringLiteral("--smoke"));
    QTimer::singleShot(0, &window, [&] {
        try {
            window.loadExample();
            if (smoke) {
                app.exit(window.smoke() ? EXIT_SUCCESS : EXIT_FAILURE);
            }
        } catch (...) {
            std::cerr << "CADCAM failed to initialize or render a workspace view.\n";
            app.exit(EXIT_FAILURE);
        }
    });
    return app.exec();
}
