#pragma once

#include "Features/Project/TubePart.h"

#include <QDialog>

namespace cadcam3d {

class TubeUnfold;
class UnfoldCanvas;

struct UnfoldEdits
{
    std::vector<SketchShape> sketches;
    std::vector<Shape> mapped;
};

class UnfoldEditor final : public QDialog
{
public:
    UnfoldEditor(const QString& title, const TubeUnfold& unfold, const TubePart& part,
        QWidget* parent = nullptr);
    UnfoldEdits takeEdits();

protected:
    void accept() override;

private:
    const TubeUnfold& unfold_;
    UnfoldCanvas* canvas_;
    UnfoldEdits edits_;
};

}
