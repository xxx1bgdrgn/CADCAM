#pragma once

#include <QPoint>
#include <QWidget>
#include <Standard_Handle.hxx>

#include <optional>
#include <vector>

class AIS_InteractiveContext;
class AIS_InteractiveObject;
class AIS_RubberBand;
class Graphic3d_Structure;
class V3d_View;

namespace cadcam {
struct CurrentZone;
}

class CadView final : public QWidget
{
public:
    explicit CadView(QWidget* parent = nullptr);
    ~CadView() override;

    void draw(const cadcam::CurrentZone& zone);

protected:
    QPaintEngine* paintEngine() const override;
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    void initialize();
    void zoomBy(double factor);
    void panBy(int x, int y);
    void showSelectionBox(QPoint start, QPoint end);
    void selectAt(QPoint position);
    void selectBox(QPoint start, QPoint end);
    void excludePartialGroups(const QRect& rect);
    void refreshSelectionAppearance();

    Handle(AIS_InteractiveContext) context_;
    Handle(V3d_View) view_;
    Handle(AIS_RubberBand) selectionBox_;
    Handle(Graphic3d_Structure) selectionGrips_;
    std::vector<Handle(AIS_InteractiveObject)> styledSelection_;
    std::optional<QPoint> selectionStart_;
    bool draggingSelection_ = false;
    QPoint previousMouse_;
};
