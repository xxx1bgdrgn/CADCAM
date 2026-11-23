#include "UI/TubeWizard.h"

#include <BRepAdaptor_Curve.hxx>
#include <BRepTools_WireExplorer.hxx>
#include <Standard_Failure.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>

#include <QButtonGroup>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <exception>
#include <utility>

namespace cadcam3d {
namespace {

SectionDimensions defaults(TubeType type)
{
    SectionDimensions dims;
    if (type == TubeType::BoxTube) dims.radius = 4;
    if (type == TubeType::RoundTube) dims.height = dims.width;
    return dims;
}

QPixmap sectionIcon(TubeType type)
{
    const auto dims = defaults(type);
    const auto section = makeSection(type, dims);
    const double scale = 52 / std::max(dims.width, dims.height);
    QPainterPath shape;
    shape.setFillRule(Qt::OddEvenFill);
    for (TopExp_Explorer wire(section, TopAbs_WIRE); wire.More(); wire.Next()) {
        bool first = true;
        for (BRepTools_WireExplorer edge(TopoDS::Wire(wire.Current()), section); edge.More(); edge.Next()) {
            const BRepAdaptor_Curve curve(edge.Current());
            const bool reverse = edge.Orientation() == TopAbs_REVERSED;
            const double start = reverse ? curve.LastParameter() : curve.FirstParameter();
            const double end = reverse ? curve.FirstParameter() : curve.LastParameter();
            const int segments = curve.GetType() == GeomAbs_Line ? 1 : 32;
            for (int i = 0; i <= segments; ++i) {
                const auto p = curve.Value(start + (end - start) * i / segments);
                const QPointF xy(42 + p.Y() * scale, 33 - p.Z() * scale);
                if (first) { shape.moveTo(xy); first = false; }
                else { shape.lineTo(xy); }
            }
        }
        shape.closeSubpath();
    }
    QPixmap icon(84, 66);
    icon.fill(Qt::transparent);
    QPainter paint(&icon);
    paint.setRenderHint(QPainter::Antialiasing);
    paint.setPen(QPen(QColor(74, 177, 188), 1.5));
    paint.setBrush(QColor(166, 229, 230));
    paint.drawPath(shape);
    return icon;
}

constexpr int Width = 0, Height = 1, Wall = 2, Length = 3, Radius = 4, Flange = 5,
    InnerWidth = 6, InnerHeight = 7;

}

const std::array<TubeType, 10>& tubeTypes()
{
    static const std::array<TubeType, 10> all{TubeType::BoxTube, TubeType::RoundTube, TubeType::OvalTube,
        TubeType::ObroundTube, TubeType::AngleSteel, TubeType::ChannelSteel, TubeType::IBeam,
        TubeType::TSection, TubeType::HBeam, TubeType::CustomProfile};
    return all;
}

QString tubeTypeName(TubeType type)
{
    switch (type) {
    case TubeType::BoxTube: return QStringLiteral("方矩管");
    case TubeType::RoundTube: return QStringLiteral("圆管");
    case TubeType::OvalTube: return QStringLiteral("椭圆管");
    case TubeType::ObroundTube: return QStringLiteral("腰形管");
    case TubeType::AngleSteel: return QStringLiteral("角钢");
    case TubeType::ChannelSteel: return QStringLiteral("槽钢");
    case TubeType::IBeam: return QStringLiteral("工字钢");
    case TubeType::TSection: return QStringLiteral("T 型钢");
    case TubeType::HBeam: return QStringLiteral("H 型钢");
    case TubeType::CustomProfile: return QStringLiteral("异型管材");
    }
    return {};
}

TubeWizard::TubeWizard(QWidget* parent) : QWizard(parent)
{
    setWindowTitle(QStringLiteral("新建管件"));
    setWizardStyle(QWizard::ModernStyle);
    setButtonText(BackButton, QStringLiteral("返回"));
    setButtonText(NextButton, QStringLiteral("下一步"));
    setButtonText(FinishButton, QStringLiteral("创建"));
    setButtonText(CancelButton, QStringLiteral("取消"));
    resize(570, 485);

    auto* selection = new QWizardPage(this);
    selection->setTitle(QStringLiteral("选择截面"));
    auto* grid = new QGridLayout(selection);
    types_ = new QButtonGroup(selection);
    int count = 0;
    for (const auto type : tubeTypes()) {
        if (type == TubeType::CustomProfile) continue;
        auto* card = new QToolButton(selection);
        card->setText(tubeTypeName(type));
        card->setIcon(sectionIcon(type));
        card->setIconSize(QSize(84, 66));
        card->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        card->setCheckable(true);
        card->setMinimumSize(140, 87);
        card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        card->setStyleSheet(QStringLiteral("QToolButton { border: 1px solid palette(mid); border-radius: 6px; color: %1; }"
            "QToolButton:checked { border: 2px solid #208b93; background: palette(alternate-base); color: %1; }")
            .arg(card->palette().color(QPalette::WindowText).name()));
        types_->addButton(card, static_cast<int>(type));
        grid->addWidget(card, count / 3, count % 3);
        ++count;
    }
    types_->button(static_cast<int>(TubeType::BoxTube))->setChecked(true);
    setPage(0, selection);

    auto* dimensions = new QWizardPage(this);
    dimensions->setTitle(QStringLiteral("输入截面与长度"));
    auto* layout = new QVBoxLayout(dimensions);
    form_ = new QFormLayout();
    layout->addLayout(form_);
    const QStringList names{QStringLiteral("外宽"), QStringLiteral("外高"), QStringLiteral("壁厚"),
        QStringLiteral("长度"), QStringLiteral("外圆角半径（0 为直角）"), QStringLiteral("翼缘厚度"),
        QStringLiteral("内宽"), QStringLiteral("内高")};
    for (std::size_t i = 0; i < inputs_.size(); ++i) {
        auto* field = new QDoubleSpinBox(dimensions);
        field->setDecimals(3);
        field->setRange(i == Radius ? 0 : 0.001, 100000);
        field->setSuffix(QStringLiteral(" mm"));
        form_->addRow(names.at(i), field);
        inputs_[i] = field;
    }
    error_ = new QLabel(dimensions);
    error_->setWordWrap(true);
    error_->setStyleSheet(QStringLiteral("color: #bc4545;"));
    layout->addWidget(error_);
    layout->addStretch();
    setPage(1, dimensions);
}

void TubeWizard::initializePage(int id)
{
    QWizard::initializePage(id);
    if (id != 1) return;

    const auto type = static_cast<TubeType>(types_->checkedId());
    const auto dims = defaults(type);
    const std::array<double, 8> values{dims.width, dims.height, dims.wall, dims.length,
        dims.radius, dims.flange, dims.innerWidth, dims.innerHeight};
    for (std::size_t i = 0; i < inputs_.size(); ++i) inputs_[i]->setValue(values[i]);
    const bool round = type == TubeType::RoundTube;
    const bool oval = type == TubeType::OvalTube;
    const bool flange = type == TubeType::IBeam || type == TubeType::TSection ||
        type == TubeType::HBeam || type == TubeType::ChannelSteel;
    form_->setRowVisible(inputs_[Height], !round);
    form_->setRowVisible(inputs_[Wall], !oval);
    form_->setRowVisible(inputs_[Radius], type == TubeType::BoxTube);
    form_->setRowVisible(inputs_[Flange], flange);
    form_->setRowVisible(inputs_[InnerWidth], oval);
    form_->setRowVisible(inputs_[InnerHeight], oval);
    static_cast<QLabel*>(form_->labelForField(inputs_[Width]))->setText(round ? QStringLiteral("外径") : QStringLiteral("外宽"));
    static_cast<QLabel*>(form_->labelForField(inputs_[Wall]))->setText(flange ? QStringLiteral("腹板厚度")
        : type == TubeType::AngleSteel ? QStringLiteral("边厚") : QStringLiteral("壁厚"));
    page(1)->setSubTitle(tubeTypeName(type));
    error_->clear();
}

bool TubeWizard::validateCurrentPage()
{
    if (currentId() != 1) return QWizard::validateCurrentPage();
    try {
        const auto type = static_cast<TubeType>(types_->checkedId());
        const auto value = [&](int field) { return inputs_.at(field)->value(); };
        part_ = makeTube(type, {value(Width), value(Height), value(Wall), value(Length),
            value(Radius), value(Flange), value(InnerWidth), value(InnerHeight)});
        return true;
    } catch (const Standard_Failure& error) {
        error_->setText(QString::fromUtf8(error.GetMessageString()));
    } catch (const std::exception& error) {
        error_->setText(QString::fromUtf8(error.what()));
    }
    return false;
}

TubePart TubeWizard::takePart()
{
    return std::move(part_.value());
}

}
