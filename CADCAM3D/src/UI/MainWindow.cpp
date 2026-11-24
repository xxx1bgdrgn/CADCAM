#include "UI/MainWindow.h"

#include "Features/Import/StepImport.h"
#include "Geometry/TubeGeometry.h"
#include "Geometry/TubeUnfold.h"
#include "Rendering/TubeView.h"
#include "UI/TubeWizard.h"
#include "UI/UnfoldEditor.h"

#include <BRepGProp.hxx>
#include <BRep_Tool.hxx>
#include <GProp_GProps.hxx>
#include <STEPControl_Writer.hxx>
#include <Standard_Failure.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>

#include <QComboBox>
#include <QAbstractButton>
#include <QAction>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QSettings>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStatusBar>
#include <QStyle>
#include <QTemporaryDir>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidgetAction>

#include <cmath>
#include <stdexcept>
#include <utility>

namespace cadcam3d {
namespace {

void showError(QWidget* parent, const char* detail)
{
    QMessageBox::warning(parent, QStringLiteral("管件操作失败"), QString::fromUtf8(detail));
}

double volume(const TopoDS_Shape& shape)
{
    GProp_GProps properties;
    BRepGProp::VolumeProperties(shape, properties);
    return properties.Mass();
}

QSettings userSettings()
{
    return QSettings(QStringLiteral("CADCAM"), QStringLiteral("CADCAM3D"));
}

const auto RecentSteps = QStringLiteral("recentStepFiles");

bool canUnfold(const TubePart& part)
{
    return part.source == TubeSource::Created && part.type == TubeType::BoxTube && part.ends.has_value();
}

}

MainWindow::MainWindow()
{
    setWindowTitle(QStringLiteral("CADCAM · 三维管材"));
    resize(1200, 760);

    auto* splitter = new QSplitter(this);
    auto* panel = new QWidget(splitter);
    auto* layout = new QVBoxLayout(panel);
    layout->addWidget(new QLabel(QStringLiteral("管件列表"), panel));
    partsList_ = new QListWidget(panel);
    layout->addWidget(partsList_);
    layout->addWidget(new QLabel(QStringLiteral("管材类型"), panel));
    typePicker_ = new QComboBox(panel);
    typePicker_->addItem(QStringLiteral("待确认"));
    for (const auto type : tubeTypes()) {
        typePicker_->addItem(tubeTypeName(type), static_cast<int>(type));
    }
    layout->addWidget(typePicker_);
    view_ = new TubeView(splitter);
    splitter->addWidget(panel);
    splitter->addWidget(view_);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({220, 980});
    setCentralWidget(splitter);

    auto* toolbar = addToolBar(QStringLiteral("管材"));
    toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    toolbar->setMovable(false);
    toolbar->setStyleSheet(QStringLiteral("QToolBar::separator { width: 1px; background: palette(mid); margin: 4px 7px; }"));
    addFileMenu(toolbar);
    toolbar->addAction(style()->standardIcon(QStyle::SP_FileIcon), QStringLiteral("新建管件"),
        this, [this] { createPart(); });
    toolbar->addAction(style()->standardIcon(QStyle::SP_DialogOpenButton), QStringLiteral("导入 STEP"),
        this, [this] { openStep(); });
    toolbar->addSeparator();
    unfoldAction_ = toolbar->addAction(QStringLiteral("二维编辑"), this, [this] { editUnfolded(); });
    unfoldAction_->setToolTip(QStringLiteral("沿顶面中点展开当前方矩管"));
    unfoldAction_->setEnabled(false);
    connect(partsList_, &QListWidget::currentRowChanged, this, [this](int row) {
        unfoldAction_->setEnabled(row >= 0 && canUnfold(parts_.at(row)));
        if (row < 0) {
            return;
        }
        const auto& part = parts_.at(row);
        const QSignalBlocker blocker(typePicker_);
        typePicker_->setCurrentIndex(part.type ? typePicker_->findData(static_cast<int>(*part.type)) : 0);
        typePicker_->setEnabled(part.source == TubeSource::Step);
        view_->draw(part);
        statusBar()->showMessage(QStringLiteral("%1  ·  按住滚轮拖动旋转  ·  右键拖动平移  ·  滚轮缩放")
            .arg(partsList_->item(row)->text()));
    });
    connect(typePicker_, &QComboBox::currentIndexChanged, this, [this](int index) {
        const int row = partsList_->currentRow();
        if (row >= 0) {
            auto& part = parts_.at(row);
            part.type = index > 0 ? std::optional<TubeType>(static_cast<TubeType>(typePicker_->itemData(index).toInt()))
                : std::nullopt;
        }
    });
}

void MainWindow::addPart(const QString& name, TubePart part)
{
    const auto detail = part.ends && part.type ? QStringLiteral("%1 · 长 %2 mm")
        .arg(tubeTypeName(*part.type)).arg(part.ends->length) : name;
    parts_.push_back(std::move(part));
    auto* item = new QListWidgetItem(name, partsList_);
    item->setToolTip(detail);
    partsList_->setCurrentRow(partsList_->count() - 1);
}

void MainWindow::loadExample()
{
    auto part = makeBoxTube({60, 40, 2, 240, 5});
    part.unfoldedShapes.push_back({1, cadcam::Primitive{cadcam::Rectangle{{90, -9}, 30, 18}}, {}});
    part.shapes.push_back(TubeUnfold(part).mapShape(part.unfoldedShapes.front()));
    addPart(QStringLiteral("示例圆角方矩管"), std::move(part));
}

void MainWindow::editUnfolded()
{
    const int row = partsList_->currentRow();
    if (row < 0 || !canUnfold(parts_.at(row))) {
        return;
    }
    auto& part = parts_.at(row);
    try {
        const TubeUnfold unfolded(part);
        UnfoldEditor editor(partsList_->item(row)->text(), unfolded, part, this);
        if (editor.exec() == QDialog::Accepted) {
            auto edits = editor.takeEdits();
            part.unfoldedShapes = std::move(edits.sketches);
            part.shapes = std::move(edits.mapped);
            view_->draw(part);
        }
    } catch (const Standard_Failure& error) {
        showError(this, error.GetMessageString());
    } catch (const std::exception& error) {
        showError(this, error.what());
    }
}

void MainWindow::createPart()
{
    TubeWizard wizard(this);
    if (wizard.exec() == QDialog::Accepted) {
        auto part = wizard.takePart();
        const auto name = QStringLiteral("P%1  %2 · %3 mm")
            .arg(parts_.size() + 1).arg(tubeTypeName(*part.type)).arg(part.ends->length);
        addPart(name, std::move(part));
    }
}

void MainWindow::openStep()
{
    const auto path = QFileDialog::getOpenFileName(this, QStringLiteral("导入 STEP 管件"), {},
        QStringLiteral("STEP 文件 (*.step *.stp)"));
    if (!path.isEmpty()) openStep(path);
}

void MainWindow::openStep(const QString& path)
{
    try {
        auto part = importStepTube(path.toUtf8().toStdString());
        addPart(QFileInfo(path).completeBaseName(), std::move(part));
        auto settings = userSettings();
        auto files = settings.value(RecentSteps).toStringList();
        files.removeAll(path);
        files.prepend(path);
        settings.setValue(RecentSteps, files.mid(0, 8));
    } catch (const Standard_Failure& error) {
        showError(this, error.GetMessageString());
    } catch (const std::exception& error) {
        showError(this, error.what());
    }
}

void MainWindow::addFileMenu(QToolBar* toolbar)
{
    auto* button = new QToolButton(toolbar);
    button->setText(QStringLiteral("文件"));
    button->setPopupMode(QToolButton::InstantPopup);
    auto* menu = new QMenu(button);
    button->setMenu(menu);
    auto* panel = new QWidget(menu);
    auto* columns = new QHBoxLayout(panel);
    auto* commands = new QVBoxLayout();
    columns->addLayout(commands);
    const auto command = [&](const QString& text, QStyle::StandardPixmap icon, void (MainWindow::*callback)()) {
        auto* item = new QToolButton(panel);
        item->setText(text);
        item->setIcon(style()->standardIcon(icon));
        item->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        commands->addWidget(item);
        connect(item, &QToolButton::clicked, this, [this, menu, callback] {
            menu->close();
            (this->*callback)();
        });
    };
    command(QStringLiteral("新建管件"), QStyle::SP_FileIcon, &MainWindow::createPart);
    command(QStringLiteral("打开 STEP"), QStyle::SP_DialogOpenButton,
        static_cast<void (MainWindow::*)()>(&MainWindow::openStep));
    commands->addStretch();

    auto* recent = new QVBoxLayout();
    columns->addLayout(recent);
    recent->addWidget(new QLabel(QStringLiteral("最近打开的 STEP"), panel));
    auto* files = new QListWidget(panel);
    files->setMinimumSize(275, 144);
    recent->addWidget(files);
    connect(menu, &QMenu::aboutToShow, this, [files] {
        files->clear();
        for (const auto& path : userSettings().value(RecentSteps).toStringList()) {
            auto* item = new QListWidgetItem(QFileInfo(path).fileName(), files);
            item->setData(Qt::UserRole, path);
            item->setToolTip(path);
        }
        if (files->count() == 0) {
            auto* empty = new QListWidgetItem(QStringLiteral("暂无记录"), files);
            empty->setFlags(Qt::NoItemFlags);
        }
    });
    connect(files, &QListWidget::itemClicked, this, [this, menu](QListWidgetItem* item) {
        const auto path = item->data(Qt::UserRole).toString();
        if (!path.isEmpty()) {
            menu->close();
            openStep(path);
        }
    });
    auto* content = new QWidgetAction(menu);
    content->setDefaultWidget(panel);
    menu->addAction(content);
    toolbar->addWidget(button);
}

bool MainWindow::smoke()
{
    if (parts_.size() != 1 || parts_.front().shapes.size() != 1) {
        return false;
    }
    const TubeUnfold roundedUnfold(parts_.front());
    const double roundedPerimeter = 160 + 10 * std::acos(-1.0);
    const auto seam = roundedUnfold.map({90, 0});
    const auto clockwise = roundedUnfold.map({90, 9});
    const auto wrapped = roundedUnfold.map({90, roundedUnfold.perimeter() - 9});
    const auto negative = roundedUnfold.map({90, -9});
    if (std::abs(roundedUnfold.perimeter() - roundedPerimeter) > 1e-5 || roundedUnfold.guides().size() != 8 ||
        std::abs(roundedUnfold.firstFaceHalfWidth() - 25) > 1e-5 ||
        std::abs(seam.x - 90) > 1e-5 || std::abs(seam.y) > 1e-5 || std::abs(seam.z - 20) > 1e-5 ||
        std::abs(clockwise.y + 9) > 1e-5 || std::abs(clockwise.z - 20) > 1e-5 ||
        std::abs(wrapped.y - 9) > 1e-5 || std::abs(wrapped.z - 20) > 1e-5 ||
        std::abs(negative.y - wrapped.y) > 1e-5 || std::abs(negative.z - wrapped.z) > 1e-5) {
        return false;
    }
    const auto& sample = std::get<Primitive>(parts_.front().shapes.front().content);
    if (!std::holds_alternative<Rectangle>(sample)) {
        return false;
    }
    const auto acrossCorner = roundedUnfold.mapShape({2, cadcam::Primitive{cadcam::Rectangle{{28, 23}, 10, 20}}, {}});
    if (!std::holds_alternative<TopoDS_Wire>(std::get<Primitive>(acrossCorner.content))) {
        return false;
    }
    const auto sharp = makeBoxTube({60, 40, 2, 240, 0});
    const TubeUnfold sharpUnfold(sharp);
    if (std::abs(sharpUnfold.perimeter() - 200) > 1e-5 || sharpUnfold.guides().size() != 4 ||
        std::abs(sharpUnfold.firstFaceHalfWidth() - 30) > 1e-5) {
        return false;
    }
    const SketchShape crossFaceText{3, cadcam::Primitive{cadcam::Text{{120, 22}, "MMMM", "Arial", 10, 90}}, {}};
    for (const auto* unfold : {&roundedUnfold, &sharpUnfold}) {
        const auto mapped = unfold->mapShape(crossFaceText);
        const auto& text = std::get<Text>(std::get<Primitive>(mapped.content));
        const double radius = unfold == &roundedUnfold ? 5 : 0;
        constexpr double tolerance = 1e-5;
        bool top = false, corner = false, side = false;
        for (TopExp_Explorer it(text.surfaceContours, TopAbs_VERTEX); it.More(); it.Next()) {
            const auto point = BRep_Tool::Pnt(TopoDS::Vertex(it.Current()));
            const double fromTopEdge = point.Y() + 30 - radius;
            const double fromSideEdge = point.Z() - 20 + radius;
            const bool onTop = std::abs(point.Z() - 20) < tolerance && fromTopEdge >= -tolerance;
            const bool onSide = std::abs(point.Y() + 30) < tolerance && fromSideEdge <= tolerance;
            const bool onCorner = radius > 0 && fromTopEdge < 0 && fromSideEdge > 0 &&
                std::abs(std::hypot(fromTopEdge, fromSideEdge) - radius) < tolerance;
            if (!onTop && !onSide && !onCorner) return false;
            top |= onTop && fromTopEdge > tolerance;
            side |= onSide && fromSideEdge < -tolerance;
            corner |= onCorner && fromTopEdge < -tolerance && fromSideEdge > tolerance;
        }
        if (text.utf8 != "MMMM" || !top || !side || (radius > 0 && !corner)) return false;
    }
    const double expected = (60 * 40 - 56 * 36) * 240;
    const double roundedVolume = volume(parts_.front().body);
    if (std::abs(volume(sharp.body) - expected) > 0.01 || roundedVolume <= 0 || roundedVolume >= expected) {
        return false;
    }
    const SectionDimensions dimensions;
    for (const auto type : tubeTypes()) {
        if (type == TubeType::CustomProfile) continue;
        const auto part = makeTube(type, dimensions);
        if (part.type != type || !part.ends || part.ends->length != dimensions.length || volume(part.body) <= 0) {
            return false;
        }
    }
    const double angleArea = (dimensions.width + dimensions.height - dimensions.wall) * dimensions.wall;
    const double channelArea = dimensions.height * dimensions.wall + 2 * dimensions.flange * (dimensions.width - dimensions.wall);
    const double obroundArea = 2 * (dimensions.width - dimensions.height) * dimensions.wall
        + std::acos(-1) * (dimensions.height - dimensions.wall) * dimensions.wall;
    for (const auto& [type, area] : {std::pair{TubeType::AngleSteel, angleArea},
             std::pair{TubeType::ChannelSteel, channelArea}, std::pair{TubeType::ObroundTube, obroundArea}}) {
        if (std::abs(volume(makeTube(type, dimensions).body) - area * dimensions.length) > 0.01) return false;
    }
    TubeWizard wizard(this);
    wizard.restart();
    wizard.next();
    wizard.button(QWizard::FinishButton)->click();
    if (wizard.result() != QDialog::Accepted || wizard.takePart().type != TubeType::BoxTube) return false;

    bool invalidRejected = false;
    try {
        makeBoxTube({60, 40, 20, 240, 0});
    } catch (const std::invalid_argument&) {
        invalidRejected = true;
    }
    const QTemporaryDir scratch;
    if (!invalidRejected || !scratch.isValid()) {
        return false;
    }
    const auto file = scratch.filePath(QStringLiteral("管件.step")).toUtf8();
    STEPControl_Writer writer;
    if (writer.Transfer(parts_.front().body, STEPControl_AsIs) != IFSelect_RetDone ||
        writer.Write(file.constData()) != IFSelect_RetDone) {
        return false;
    }
    auto imported = importStepTube(file.toStdString());
    if (imported.source != TubeSource::Step || imported.type ||
        std::abs(volume(imported.body) - roundedVolume) > 0.01) {
        return false;
    }
    addPart(QStringLiteral("STEP 测试件"), std::move(imported));
    partsList_->setCurrentRow(0);
    return parts_.size() == 2;
}

}
