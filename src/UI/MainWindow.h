#pragma once

#include "Features/Views/CurrentZone.h"

#include <QMainWindow>
#include <QString>
#include <QUndoStack>

class CadView;
class QListWidget;
class QTabWidget;

namespace cadcam {

class MainWindow final : public QMainWindow
{
public:
    MainWindow();

    void loadExample();
    bool smoke();

private:
    void reset(Project project);
    void addRectangle();
    void switchZone(ZoneSource source, const QString& title);

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
