#pragma once

#include <QPoint>
#include <QWidget>
#include <Standard_Handle.hxx>

class AIS_InteractiveContext;
class V3d_View;

namespace cadcam3d {

struct TubePart;

class TubeView final : public QWidget
{
public:
    explicit TubeView(QWidget* parent = nullptr);
    ~TubeView() override;
    void draw(const TubePart& part);

protected:
    QPaintEngine* paintEngine() const override;
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;

private:
    void initialize();

    Handle(AIS_InteractiveContext) context_;
    Handle(V3d_View) view_;
    QPoint previousMouse_;
};

}
