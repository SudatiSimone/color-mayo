/****************************************************************************
** Copyright (c) 2020, Fougue Ltd. <http://www.fougue.pro>
** All rights reserved.
** See license at https://github.com/fougue/mayo/blob/master/LICENSE.txt
****************************************************************************/

#include "graphics_entity_driver.h"

#include "../base/document.h"
#include "../base/brep_utils.h"
#include "../base/caf_utils.h"
#include "graphics_entity_base_property_group.h"
#include "graphics_mesh_data_source.h"
#include "graphics_scene.h"

#include <AIS_ColoredShape.hxx>
#include <AIS_DisplayMode.hxx>
#include <BRep_TFace.hxx>
#include <MeshVS_DisplayModeFlags.hxx>
#include <MeshVS_DrawerAttribute.hxx>
#include <MeshVS_Drawer.hxx>
#include <MeshVS_Mesh.hxx>
#include <MeshVS_MeshPrsBuilder.hxx>
#include <Prs3d_LineAspect.hxx>
#include <V3d_View.hxx>
#include <V3d_Viewer.hxx>
#include <TDataXtd_Triangulation.hxx>
#include <XCAFPrs_AISObject.hxx>
#include <XSDRAWSTLVRML_DataSource.hxx>
#include <stdexcept>

namespace Mayo {

namespace { struct GraphicsEntityDriverI18N { MAYO_DECLARE_TEXT_ID_FUNCTIONS(Mayo::GraphicsEntityDriver) }; }

void GraphicsEntityDriver::throwIf_invalidDisplayMode(Enumeration::Value mode) const
{
    if (this->displayModes().findIndex(mode) == -1)
        throw std::invalid_argument("Invalid display mode");
}

void GraphicsEntityDriver::throwIf_differentDriver(const GraphicsEntity& entity) const
{
    if (entity.driverPtr() != this)
        throw std::invalid_argument("Invalid driver for graphics entity");
}

void GraphicsEntityDriver::initEntity(GraphicsEntity* ptrEntity, const TDF_Label& label) const
{
    Expects(ptrEntity != nullptr);
    ptrEntity->m_label = label;
    ptrEntity->m_driverPtr = this;
}

void GraphicsEntityDriver::setEntityAisObject(
        GraphicsEntity* ptrEntity, const Handle_AIS_InteractiveObject& obj)
{
    Expects(ptrEntity != nullptr);
    ptrEntity->m_aisObject = obj;
}

GraphicsShapeEntityDriver::GraphicsShapeEntityDriver()
{
    this->setDisplayModes({
        { DisplayMode_Wireframe, GraphicsEntityDriverI18N::textId("WIREFRAME"), {} },
        { DisplayMode_HiddenLineRemoval, GraphicsEntityDriverI18N::textId("HLR"), {} },
        { DisplayMode_Shaded, GraphicsEntityDriverI18N::textId("SHADED"), {} },
        { DisplayMode_ShadedWithFaceBoundary, GraphicsEntityDriverI18N::textId("SHADED_FACE_BNDS"), {} }
    });
}

GraphicsEntityDriver::Support GraphicsShapeEntityDriver::supportStatus(const TDF_Label& label) const
{
    if (XCaf::isShape(label))
        return Support::Complete;

    // TODO TNaming_Shape ?
    // TDataXtd_Shape ?

    if (CafUtils::hasAttribute<TDataXtd_Triangulation>(label)) {
        //return Support::Partial;
    }

    return Support::None;
}

GraphicsEntity GraphicsShapeEntityDriver::createEntity(const TDF_Label& label) const
{
    GraphicsEntity entity;
    this->initEntity(&entity, label);
    if (XCaf::isShape(label)) {
        Handle_XCAFPrs_AISObject gpx = new XCAFPrs_AISObject(label);
        gpx->SetDisplayMode(AIS_Shaded);
        gpx->Attributes()->SetFaceBoundaryDraw(true);
        gpx->Attributes()->SetFaceBoundaryAspect(
                    new Prs3d_LineAspect(Quantity_NOC_BLACK, Aspect_TOL_SOLID, 1.));
        gpx->Attributes()->SetIsoOnTriangulation(true);
        GraphicsEntityDriver::setEntityAisObject(&entity, gpx);
    }
    else if (CafUtils::hasAttribute<TDataXtd_Triangulation>(label)) {
    }

    return entity;
}

void GraphicsShapeEntityDriver::applyDisplayMode(GraphicsEntity* entity, Enumeration::Value mode) const
{
    this->throwIf_differentDriver(*entity);
    this->throwIf_invalidDisplayMode(mode);
    const GraphicsScene* gfxScene = entity->graphicsScene();
    auto fnSetViewComputedMode = [=](bool on) {
        V3d_ListOfViewIterator viewIter = gfxScene->v3dViewer()->DefinedViewIterator();
        while (viewIter.More()) {
            viewIter.Value()->SetComputedMode(on);
            viewIter.Next();
        }
    };
    if (mode == DisplayMode_HiddenLineRemoval) {
        gfxScene->defaultPrs3dDrawer()->SetTypeOfHLR(Prs3d_TOH_PolyAlgo);
        gfxScene->defaultPrs3dDrawer()->EnableDrawHiddenLine();
        fnSetViewComputedMode(true);
    }
    else {
        gfxScene->defaultPrs3dDrawer()->SetTypeOfHLR(Prs3d_TOH_NotSet);
        gfxScene->defaultPrs3dDrawer()->DisableDrawHiddenLine();
        fnSetViewComputedMode(false);
        const AIS_DisplayMode aisDispMode = mode == DisplayMode_Wireframe ? AIS_WireFrame : AIS_Shaded;
        const bool showFaceBounds = mode == DisplayMode_ShadedWithFaceBoundary;
        const Handle_AIS_InteractiveObject& aisObject = entity->aisObject();
        if (aisObject->DisplayMode() != aisDispMode)
            entity->setDisplayMode(aisDispMode);

        if (aisObject->Attributes()->FaceBoundaryDraw() != showFaceBounds) {
            aisObject->Attributes()->SetFaceBoundaryDraw(showFaceBounds);
            aisObject->Redisplay(true);
        }
    }

    //entity->aisContext()->UpdateCurrentViewer();
}

Enumeration::Value GraphicsShapeEntityDriver::currentDisplayMode(const GraphicsEntity& entity) const
{
    this->throwIf_differentDriver(entity);
    if (entity.graphicsScene()->hiddenLineDrawingOn())
        return DisplayMode_HiddenLineRemoval;

    const int displayMode = entity.displayMode();
    if (displayMode == AIS_WireFrame)
        return DisplayMode_Wireframe;

    if (displayMode == AIS_Shaded) {
        return entity.aisObject()->Attributes()->FaceBoundaryDraw() ?
                    DisplayMode_ShadedWithFaceBoundary :
                    DisplayMode_Shaded;
    }

    return -1;
}

std::unique_ptr<PropertyGroupSignals> GraphicsShapeEntityDriver::properties(const GraphicsEntity& entity) const
{
    this->throwIf_differentDriver(entity);
    return std::make_unique<GraphicsEntityBasePropertyGroup>(entity);
}

void GraphicsShapeEntityDriver::handleColorChanged(
        const GraphicsEntity& entity, const DocumentTreeNode& docTreeNode) const
{
    this->throwIf_differentDriver(entity);
    if (!docTreeNode.isValid())
        return;

    // AIS_ColoredShape is the base class of XCAFPrs_AISObject
    auto gfx = Handle_AIS_ColoredShape::DownCast(entity.aisObject());
    if (!gfx)
        return;

    // Retrieve color
    const DocumentPtr& doc = docTreeNode.document();
    Quantity_Color color;
    if (!doc->xcaf().colorTool()->GetColor(docTreeNode.label(), XCAFDoc_ColorSurf, color))
        return;

    // Helper function
    auto fnChangeColor = [=](const TopoDS_Shape& shape){
        gfx->SetCustomColor(shape, color);
        gfx->SynchronizeAspects();
        entity.graphicsScene()->redraw();
    };

    // Retrieve shape
    const TopLoc_Location shapeLoc = doc->xcaf().shapeAbsoluteLocation(docTreeNode.id());
    const TopoDS_Shape shape = XCaf::shape(docTreeNode.label()).Located(shapeLoc);

    // Maybe the shape has "direct" custom aspects
    if (gfx->CustomAspectsMap().IsBound(shape)) {
        fnChangeColor(shape);
        return;
    }

    // If not then it has "shared" custom aspects with others in a TopoDS_Compound
    for (AIS_DataMapOfShapeDrawer::Iterator it(gfx->CustomAspectsMap()); it.More(); it.Next()) {
        const TopoDS_Shape& baseShape = it.Key();
        bool exitRequested = false;
        BRepUtils::forEachSubShape(baseShape, TopAbs_SOLID, [&](const TopoDS_Shape& subShape) {
            if (!exitRequested && subShape.IsSame(shape)) {
                fnChangeColor(baseShape);
                exitRequested = true;
            }
        });

        if (exitRequested)
            return;
    }
}

GraphicsMeshEntityDriver::GraphicsMeshEntityDriver()
{
    this->setDisplayModes({
        { MeshVS_DMF_WireFrame, GraphicsEntityDriverI18N::textId("WIREFRAME"), {} },
        { MeshVS_DMF_Shading, GraphicsEntityDriverI18N::textId("SHADED"), {} },
        { MeshVS_DMF_Shrink, GraphicsEntityDriverI18N::textId("SHRINK"), {} } // MeshVS_DA_ShrinkCoeff
    });
}

GraphicsEntityDriver::Support GraphicsMeshEntityDriver::supportStatus(const TDF_Label& label) const
{
    if (CafUtils::hasAttribute<TDataXtd_Triangulation>(label))
        return Support::Complete;

    if (XCaf::isShape(label)) {
        const TopoDS_Shape shape = XCaf::shape(label);
        if (shape.ShapeType() == TopAbs_FACE)
            return Support::Partial;
    }

    return Support::None;
}

GraphicsEntity GraphicsMeshEntityDriver::createEntity(const TDF_Label& label) const
{
    GraphicsEntity entity;
    this->initEntity(&entity, label);
    Handle_Poly_Triangulation polyTri;
    //const TopLoc_Location* ptrLocationPolyTri = nullptr;
    auto attrTriangulation = CafUtils::findAttribute<TDataXtd_Triangulation>(label);
    if (!attrTriangulation.IsNull()) {
        polyTri = attrTriangulation->Get();
    }
    else if (XCaf::isShape(label)) {
        const TopoDS_Shape shape = XCaf::shape(label);
        if (shape.ShapeType() == TopAbs_FACE) {
            const auto& tface = Handle_BRep_TFace::DownCast(shape.TShape());
            if (!tface.IsNull()) {
                polyTri = tface->Triangulation();
                //ptrLocationPolyTri = &shape.Location();
            }
        }
    }

    if (!polyTri.IsNull()) {
        Handle_MeshVS_Mesh gpx = new MeshVS_Mesh;
        gpx->SetDataSource(new GraphicsMeshDataSource(polyTri));
        // meshVisu->AddBuilder(..., false); -> No selection
        gpx->AddBuilder(new MeshVS_MeshPrsBuilder(gpx), true);

        // -- MeshVS_DrawerAttribute
        gpx->GetDrawer()->SetBoolean(MeshVS_DA_ShowEdges, defaultValues().showEdges);
        gpx->GetDrawer()->SetBoolean(MeshVS_DA_DisplayNodes, defaultValues().showNodes);
        gpx->GetDrawer()->SetMaterial(MeshVS_DA_FrontMaterial, Graphic3d_NOM_PLASTIC);
        gpx->GetDrawer()->SetColor(MeshVS_DA_InteriorColor, defaultValues().color);
        gpx->GetDrawer()->SetMaterial(
                    MeshVS_DA_FrontMaterial, Graphic3d_MaterialAspect(defaultValues().material));
        gpx->GetDrawer()->SetColor(MeshVS_DA_EdgeColor, defaultValues().edgeColor);
        gpx->SetDisplayMode(MeshVS_DMF_Shading);

        //gpx->SetHilightMode(MeshVS_DMF_WireFrame);
        gpx->SetMeshSelMethod(MeshVS_MSM_PRECISE);
        GraphicsEntityDriver::setEntityAisObject(&entity, gpx);
    }

    return entity;
}

void GraphicsMeshEntityDriver::applyDisplayMode(GraphicsEntity* entity, Enumeration::Value mode) const
{
    this->throwIf_differentDriver(*entity);
    this->throwIf_invalidDisplayMode(mode);
    entity->setDisplayMode(mode);
}

Enumeration::Value GraphicsMeshEntityDriver::currentDisplayMode(const GraphicsEntity& entity) const
{
    this->throwIf_differentDriver(entity);
    return entity.displayMode();
}

class GraphicsMeshEntityDriver::EntityProperties : public GraphicsEntityBasePropertyGroup {
    MAYO_DECLARE_TEXT_ID_FUNCTIONS(Mayo::GraphicsMeshEntityDriver_EntityProperties)
public:
    EntityProperties(const GraphicsEntity& entity)
        : GraphicsEntityBasePropertyGroup(entity),
          m_meshVisu(Handle_MeshVS_Mesh::DownCast(entity.aisObject())),
          m_propertyColor(this, textId("color")),
          m_propertyEdgeColor(this, textId("edgeColor")),
          m_propertyShowEdges(this, textId("showEdges")),
          m_propertyShowNodes(this, textId("showNodes"))
    {
        // Init properties
        Mayo_PropertyChangedBlocker(this);

        // -- Color
        Quantity_Color color;
        m_meshVisu->GetDrawer()->GetColor(MeshVS_DA_InteriorColor, color);
        m_propertyColor.setValue(color);
        // -- Edge color
        Quantity_Color edgeColor;
        m_meshVisu->GetDrawer()->GetColor(MeshVS_DA_EdgeColor, edgeColor);
        m_propertyEdgeColor.setValue(edgeColor);
        // -- Show edges
        bool boolVal;
        m_meshVisu->GetDrawer()->GetBoolean(MeshVS_DA_ShowEdges, boolVal);
        m_propertyShowEdges.setValue(boolVal);
        // -- Show nodes
        m_meshVisu->GetDrawer()->GetBoolean(MeshVS_DA_DisplayNodes, boolVal);
        m_propertyShowNodes.setValue(boolVal);
    }

    void onPropertyChanged(Property* prop) override {
        auto fnRedisplay = [](const Handle_AIS_InteractiveObject& gfx) {
            gfx->Redisplay(true); // All modes
        };

        if (prop == &m_propertyShowEdges) {
            m_meshVisu->GetDrawer()->SetBoolean(MeshVS_DA_ShowEdges, m_propertyShowEdges.value());
            fnRedisplay(m_meshVisu);
        }
        else if (prop == &m_propertyShowNodes) {
            m_meshVisu->GetDrawer()->SetBoolean(MeshVS_DA_DisplayNodes, m_propertyShowNodes.value());
            fnRedisplay(m_meshVisu);
        }
        else if (prop == &m_propertyColor) {
            m_meshVisu->GetDrawer()->SetColor(MeshVS_DA_InteriorColor, m_propertyColor.value());
            fnRedisplay(m_meshVisu);
        }
        else if (prop == &m_propertyEdgeColor) {
            m_meshVisu->GetDrawer()->SetColor(MeshVS_DA_EdgeColor, m_propertyEdgeColor.value());
            fnRedisplay(m_meshVisu);
        }

        GraphicsEntityBasePropertyGroup::onPropertyChanged(prop);
    }

    Handle_MeshVS_Mesh m_meshVisu;
    PropertyOccColor m_propertyColor;
    PropertyOccColor m_propertyEdgeColor;
    PropertyBool m_propertyShowEdges;
    PropertyBool m_propertyShowNodes;
};

std::unique_ptr<PropertyGroupSignals> GraphicsMeshEntityDriver::properties(const GraphicsEntity& entity) const
{
    this->throwIf_differentDriver(entity);
    return std::make_unique<EntityProperties>(entity);
}

namespace Internal {

Q_GLOBAL_STATIC(GraphicsMeshEntityDriver::DefaultValues, graphicsMeshDefaultValues)

} // namespace Internal

const GraphicsMeshEntityDriver::DefaultValues& GraphicsMeshEntityDriver::defaultValues()
{
    return *Internal::graphicsMeshDefaultValues;
}

void GraphicsMeshEntityDriver::setDefaultValues(const DefaultValues& values)
{
    *Internal::graphicsMeshDefaultValues = values;
}

} // namespace Mayo
