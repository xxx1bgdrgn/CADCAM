#pragma once

#include "Features/Project/Project.h"

#include <QUndoCommand>

#include <utility>

namespace cadcam {

class AddShapeCommand final : public QUndoCommand
{
public:
    AddShapeCommand(Project& project, Shape shape)
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

}
