#pragma once

#include "Geometry/TubeGeometry.h"

#include <QWizard>

class QButtonGroup;
class QDoubleSpinBox;
class QFormLayout;
class QLabel;

namespace cadcam3d {

const std::array<TubeType, 10>& tubeTypes();
QString tubeTypeName(TubeType type);

class TubeWizard final : public QWizard
{
public:
    explicit TubeWizard(QWidget* parent = nullptr);
    TubePart takePart();

protected:
    void initializePage(int id) override;
    bool validateCurrentPage() override;

private:
    QButtonGroup* types_;
    QFormLayout* form_;
    QLabel* error_;
    std::array<QDoubleSpinBox*, 8> inputs_;
    std::optional<TubePart> part_;
};

}
