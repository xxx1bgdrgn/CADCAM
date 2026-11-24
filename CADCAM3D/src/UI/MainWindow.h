#pragma once

#include "Features/Project/TubePart.h"

#include <QMainWindow>

class QComboBox;
class QAction;
class QListWidget;
class QToolBar;

namespace cadcam3d {

class TubeView;

class MainWindow final : public QMainWindow
{
public:
    MainWindow();
    void loadExample();
    bool smoke();

private:
    void createPart();
    void openStep();
    void openStep(const QString& path);
    void editUnfolded();
    void addFileMenu(QToolBar* toolbar);
    void addPart(const QString& name, TubePart part);

    QListWidget* partsList_;
    QComboBox* typePicker_;
    QAction* unfoldAction_;
    TubeView* view_;
    std::vector<TubePart> parts_;
};

}
