#include "Features/Import/StepImport.h"

#include <BRepCheck_Analyzer.hxx>
#include <STEPCAFControl_Reader.hxx>
#include <TDocStd_Document.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>

#include <stdexcept>

namespace cadcam3d {

TubePart importStepTube(const std::string& utf8Path)
{
    STEPCAFControl_Reader reader;
    if (reader.ReadFile(utf8Path.c_str()) != IFSelect_RetDone) {
        throw std::runtime_error("The STEP file could not be read.");
    }
    Handle(TDocStd_Document) document = new TDocStd_Document(TCollection_ExtendedString("BinXCAF"));
    if (!reader.Transfer(document)) {
        throw std::runtime_error("STEP geometry transfer failed.");
    }

    const auto tool = XCAFDoc_DocumentTool::ShapeTool(document->Main());
    TDF_LabelSequence roots;
    tool->GetFreeShapes(roots);
    TopTools_IndexedMapOfShape solids;
    for (int i = 1; i <= roots.Length(); ++i) {
        const auto shape = XCAFDoc_ShapeTool::GetShape(roots.Value(i));
        if (!shape.IsNull()) {
            TopExp::MapShapes(shape, TopAbs_SOLID, solids);
        }
    }
    if (solids.Extent() != 1 || !BRepCheck_Analyzer(solids(1)).IsValid()) {
        throw std::runtime_error("The STEP file must contain one valid solid.");
    }
    return {TubeSource::Step, std::nullopt, solids(1)};
}

}
