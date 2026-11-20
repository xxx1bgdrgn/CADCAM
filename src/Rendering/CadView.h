#pragma once

#include <QPoint>
#include <QWidget>
#include <Standard_Handle.hxx>

class AIS_InteractiveContext;
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

private:
    void initialize();
    void zoomBy(double factor);
    void panBy(int x, int y);

    Handle(AIS_InteractiveContext) context_;
    Handle(V3d_View) view_;
    QPoint previousMouse_;
};
