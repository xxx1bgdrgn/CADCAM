#pragma once

#include "Project.h"

#include <AIS_InteractiveContext.hxx>
#include <V3d_View.hxx>
#include <QPoint>
#include <QWidget>

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

    Handle(AIS_InteractiveContext) context_;
    Handle(V3d_View) view_;
    QPoint previousMouse_;
};
