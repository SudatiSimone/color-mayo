/****************************************************************************
** Copyright (c) 2020, Fougue Ltd. <http://www.fougue.pro>
** All rights reserved.
** See license at https://github.com/fougue/mayo/blob/master/LICENSE.txt
****************************************************************************/

#include "gui_document.h"

#include "../app/theme.h" // TODO Remove this dependency
#include "../base/application_item.h"
#include "../base/bnd_utils.h"
#include "../base/document.h"
#include "../base/tkernel_utils.h"
#include "../gui/gui_application.h"
#include "../graphics/graphics_entity_driver_table.h"
#include "../graphics/graphics_utils.h"
#include "../graphics/v3d_view_camera_animation.h"

#include <fougtools/occtools/qt_utils.h>

#include <QtCore/QtDebug>
#if OCC_VERSION_HEX >= OCC_VERSION_CHECK(7, 4, 0)
#  include <AIS_ViewCube.hxx>
#endif
#include <AIS_Trihedron.hxx>
#include <Geom_Axis2Placement.hxx>
#include <Graphic3d_GraphicDriver.hxx>
#include <V3d_TypeOfOrientation.hxx>

namespace Mayo {

namespace Internal {

// Defined in gui_create_gfx_driver.cpp
Handle_Graphic3d_GraphicDriver createGfxDriver();

static Handle_AIS_Trihedron createOriginTrihedron()
{
    Handle_Geom_Axis2Placement axis = new Geom_Axis2Placement(gp::XOY());
    Handle_AIS_Trihedron aisTrihedron = new AIS_Trihedron(axis);
    aisTrihedron->SetDatumDisplayMode(Prs3d_DM_WireFrame);
    aisTrihedron->SetDrawArrows(false);
    aisTrihedron->Attributes()->DatumAspect()->LineAspect(Prs3d_DP_XAxis)->SetWidth(2.5);
    aisTrihedron->Attributes()->DatumAspect()->LineAspect(Prs3d_DP_YAxis)->SetWidth(2.5);
    aisTrihedron->Attributes()->DatumAspect()->LineAspect(Prs3d_DP_ZAxis)->SetWidth(2.5);
    aisTrihedron->SetDatumPartColor(Prs3d_DP_XAxis, Quantity_NOC_RED2);
    aisTrihedron->SetDatumPartColor(Prs3d_DP_YAxis, Quantity_NOC_GREEN2);
    aisTrihedron->SetDatumPartColor(Prs3d_DP_ZAxis, Quantity_NOC_BLUE2);
    aisTrihedron->SetLabel(Prs3d_DP_XAxis, "");
    aisTrihedron->SetLabel(Prs3d_DP_YAxis, "");
    aisTrihedron->SetLabel(Prs3d_DP_ZAxis, "");
    //aisTrihedron->SetTextColor(Quantity_NOC_GRAY40);
    aisTrihedron->SetSize(60);
    aisTrihedron->SetTransformPersistence(
                new Graphic3d_TransformPers(Graphic3d_TMF_ZoomPers, axis->Ax2().Location()));
    aisTrihedron->Attributes()->SetZLayer(Graphic3d_ZLayerId_Topmost);
    aisTrihedron->SetInfiniteState(true);
    return aisTrihedron;
}

} // namespace Internal

GuiDocument::GuiDocument(const DocumentPtr& doc, GuiApplication* guiApp)
    : QObject(guiApp),
      m_guiApp(guiApp),
      m_document(doc),
      m_gfxScene(this),
      m_v3dView(m_gfxScene.createV3dView()),
      m_aisOriginTrihedron(Internal::createOriginTrihedron()),
      m_cameraAnimation(new V3dViewCameraAnimation(m_v3dView, this))
{
    Expects(!doc.IsNull());

#if OCC_VERSION_HEX >= OCC_VERSION_CHECK(7, 4, 0)
    this->setViewTrihedronMode(ViewTrihedronMode::AisViewCube);
    this->setViewTrihedronCorner(Qt::TopLeftCorner);
#else
    this->setViewTrihedronMode(ViewTrihedronMode::V3dViewZBuffer);
    this->setViewTrihedronCorner(Qt::BottomLeftCorner);
#endif

    //m_v3dView->SetShadingModel(V3d_PHONG);
    // 3D view - Enable anti-aliasing with MSAA
    m_v3dView->ChangeRenderingParams().IsAntialiasingEnabled = true;
    m_v3dView->ChangeRenderingParams().NbMsaaSamples = 4;
    // 3D view - Set gradient background
    m_v3dView->SetBgGradientColors(
                occ::QtUtils::toOccColor(
                    mayoTheme()->color(Theme::Color::View3d_BackgroundGradientStart)),
                occ::QtUtils::toOccColor(
                    mayoTheme()->color(Theme::Color::View3d_BackgroundGradientEnd)),
                Aspect_GFM_VER);

    m_cameraAnimation->setEasingCurve(QEasingCurve::OutExpo);

    for (int i = 0; i < doc->entityCount(); ++i)
        this->mapGraphics(doc->entityTreeNodeId(i));

    QObject::connect(doc.get(), &Document::colorChanged, this, &GuiDocument::onDocumentColorChanged);
    QObject::connect(doc.get(), &Document::entityAdded, this, &GuiDocument::onDocumentEntityAdded);
    QObject::connect(
                doc.get(), &Document::entityAboutToBeDestroyed,
                this, &GuiDocument::onDocumentEntityAboutToBeDestroyed);
}

GraphicsEntity GuiDocument::findGraphicsEntity(TreeNodeId entityTreeNodeId) const
{
    const GraphicsItem* gfxItem = this->findGraphicsItem(entityTreeNodeId);
    return gfxItem ? gfxItem->graphicsEntity : GraphicsEntity();
}

void GuiDocument::toggleItemSelected(const ApplicationItem& appItem)
{
    const DocumentPtr doc = appItem.document();
    if (doc != this->document())
        return;

    if (appItem.isDocumentTreeNode()) {
        const DocumentTreeNode& docTreeNode = appItem.documentTreeNode();
        const TreeNodeId entityNodeId = doc->modelTree().nodeRoot(docTreeNode.id());

        // Add/remove graphics owner
        const GraphicsItem* gfxItem = this->findGraphicsItem(entityNodeId);
        if (gfxItem && gfxItem->gpxTreeNodeMapping) {
            auto vecGfxOwner = gfxItem->gpxTreeNodeMapping->findGraphicsOwners(docTreeNode);
            for (const GraphicsOwnerPtr& gfxOwner : vecGfxOwner)
                m_gfxScene.toggleOwnerSelection(gfxOwner);
        }
    }
}

bool GuiDocument::isOriginTrihedronVisible() const
{
    return m_gfxScene.isObjectVisible(m_aisOriginTrihedron);
}

void GuiDocument::toggleOriginTrihedronVisibility()
{
    const bool visible = !this->isOriginTrihedronVisible();
    m_gfxScene.setObjectVisible(m_aisOriginTrihedron, visible);
}

void GuiDocument::processAction(const GraphicsOwnerPtr& graphicsOwner)
{
    if (graphicsOwner.IsNull())
        return;

#if OCC_VERSION_HEX >= OCC_VERSION_CHECK(7, 4, 0)
    auto viewCubeOwner = opencascade::handle<AIS_ViewCubeOwner>::DownCast(graphicsOwner);
    if (!viewCubeOwner.IsNull())
        this->setViewCameraOrientation(viewCubeOwner->MainOrientation());
#endif
}

void GuiDocument::setViewCameraOrientation(V3d_TypeOfOrientation projection)
{
    this->runViewCameraAnimation([=](Handle_V3d_View view) {
        view->SetProj(projection);
        GraphicsUtils::V3dView_fitAll(view);
    });
}

void GuiDocument::runViewCameraAnimation(const std::function<void (Handle_V3d_View)>& fnViewChange)
{
    m_cameraAnimation->configure(fnViewChange);
    m_cameraAnimation->start(QAbstractAnimation::KeepWhenStopped);
}

void GuiDocument::stopViewCameraAnimation()
{
    m_cameraAnimation->stop();
}

static Aspect_TypeOfTriedronPosition toOccCorner(Qt::Corner corner)
{
    switch (corner) {
    case Qt::TopLeftCorner: return Aspect_TOTP_LEFT_UPPER;
    case Qt::TopRightCorner: return Aspect_TOTP_RIGHT_UPPER;
    case Qt::BottomLeftCorner: return Aspect_TOTP_LEFT_LOWER;
    case Qt::BottomRightCorner: return Aspect_TOTP_RIGHT_LOWER;
    }

    return Aspect_TOTP_LEFT_UPPER; // Fallback
}

void GuiDocument::setViewTrihedronMode(ViewTrihedronMode mode)
{
    if (mode == m_viewTrihedronMode)
        return;

    auto fnViewCubeSetVisible = [&](bool on) {
        m_gfxScene.setObjectVisible(m_aisViewCube, on);
    };

    switch (mode) {
    case ViewTrihedronMode::None: {
        m_v3dView->TriedronErase();
        fnViewCubeSetVisible(false);
        break;
    }
    case ViewTrihedronMode::V3dViewZBuffer: {
        this->v3dViewTrihedronDisplay(m_viewTrihedronCorner);
        fnViewCubeSetVisible(false);
        break;
    }
    case ViewTrihedronMode::AisViewCube: {
        if (m_aisViewCube.IsNull()) {
#if OCC_VERSION_HEX >= OCC_VERSION_CHECK(7, 4, 0)
            opencascade::handle<AIS_ViewCube> aisViewCube = new AIS_ViewCube;
            aisViewCube->SetBoxColor(Quantity_NOC_GRAY75);
            //aisViewCube->SetFixedAnimationLoop(false);
            aisViewCube->SetSize(55);
            aisViewCube->SetFontHeight(12);
            aisViewCube->SetAxesLabels("", "", "");
            aisViewCube->SetTransformPersistence(
                        new Graphic3d_TransformPers(
                            Graphic3d_TMF_TriedronPers,
                            toOccCorner(m_viewTrihedronCorner),
                            Graphic3d_Vec2i(85, 85)));
            m_gfxScene.addObject(aisViewCube);
            //aisViewCube->Attributes()->DatumAspect()->LineAspect(Prs3d_DP_XAxis)->SetColor(Quantity_NOC_RED2);
            const Handle_Prs3d_DatumAspect& datumAspect = aisViewCube->Attributes()->DatumAspect();
            datumAspect->ShadingAspect(Prs3d_DP_XAxis)->SetColor(Quantity_NOC_RED2);
            datumAspect->ShadingAspect(Prs3d_DP_YAxis)->SetColor(Quantity_NOC_GREEN2);
            datumAspect->ShadingAspect(Prs3d_DP_ZAxis)->SetColor(Quantity_NOC_BLUE2);
            m_aisViewCube = aisViewCube;
#endif
        }

        m_v3dView->TriedronErase();
        fnViewCubeSetVisible(true);
        break;
    }
    } // endswitch

    m_viewTrihedronMode = mode;
    emit this->viewTrihedronModeChanged(mode);
}

void GuiDocument::setViewTrihedronCorner(Qt::Corner corner)
{
    if (corner == m_viewTrihedronCorner)
        return;

    switch (m_viewTrihedronMode) {
    case ViewTrihedronMode::None: {
        break; // Nothing to do
    }
    case ViewTrihedronMode::V3dViewZBuffer: {
        this->v3dViewTrihedronDisplay(corner);
        break;
    }
    case ViewTrihedronMode::AisViewCube: {
        if (m_aisViewCube)
            m_aisViewCube->TransformPersistence()->SetCorner2d(toOccCorner(corner));

        break;
    }
    } // endswitch

    m_viewTrihedronCorner = corner;
    emit this->viewTrihedronCornerChanged(corner);
}

int GuiDocument::aisViewCubeBoundingSize() const
{
    if (m_aisViewCube.IsNull())
        return 0;

#if OCC_VERSION_HEX >= OCC_VERSION_CHECK(7, 4, 0)
    auto hnd = opencascade::handle<AIS_ViewCube>::DownCast(m_aisViewCube);
    return 2 * (hnd->Size()
                + hnd->BoxFacetExtension()
                + hnd->BoxEdgeGap()
                + hnd->BoxEdgeMinSize()
                + hnd->BoxCornerMinSize()
                + hnd->RoundRadius())
            + hnd->AxesPadding()
            + hnd->FontHeight();
#else
    return 0;
#endif
}

void GuiDocument::onDocumentColorChanged(TreeNodeId treeNodeId)
{
    const TreeNodeId entityTreeNodeId = m_document->modelTree().nodeRoot(treeNodeId);
    const GraphicsItem* gfxItem = this->findGraphicsItem(entityTreeNodeId);
    if (gfxItem) {
        const GraphicsEntity& gfxEntity = gfxItem->graphicsEntity;
        gfxEntity.driverPtr()->handleColorChanged(gfxEntity, { m_document, treeNodeId });
    }
}

void GuiDocument::onDocumentEntityAdded(TreeNodeId entityTreeNodeId)
{
    this->mapGraphics(entityTreeNodeId);
    emit graphicsBoundingBoxChanged(m_gpxBoundingBox);
}

void GuiDocument::onDocumentEntityAboutToBeDestroyed(TreeNodeId entityTreeNodeId)
{
    const GraphicsItem* gfxItem = this->findGraphicsItem(entityTreeNodeId);
    if (gfxItem) {
        const GraphicsEntity& gfxEntity = gfxItem->graphicsEntity;
        m_gfxScene.eraseObject(gfxEntity.aisObject());
        m_vecGraphicsItem.erase(m_vecGraphicsItem.begin() + (gfxItem - &m_vecGraphicsItem.front()));
        m_gfxScene.redraw();

        // Recompute bounding box
        m_gpxBoundingBox.SetVoid();
        for (const GraphicsItem& item : m_vecGraphicsItem) {
            const Bnd_Box entityBndBox = GraphicsUtils::AisObject_boundingBox(item.graphicsEntity.aisObject());
            BndUtils::add(&m_gpxBoundingBox, entityBndBox);
        }

        emit graphicsBoundingBoxChanged(m_gpxBoundingBox);
    }
}

void GuiDocument::mapGraphics(TreeNodeId entityTreeNodeId)
{
    GraphicsItem item;
    const DocumentTreeNode entityTreeNode(m_document, entityTreeNodeId);
    GraphicsEntity& gfxEntity = item.graphicsEntity;
    gfxEntity = m_guiApp->graphicsEntityDriverTable()->createEntity(entityTreeNode.label());
    if (gfxEntity.aisObject().IsNull())
        return;

    gfxEntity.setScene(&m_gfxScene);
    gfxEntity.setVisible(true);
    item.entityTreeNodeId = entityTreeNodeId;
    m_gfxScene.redraw();

    item.gpxTreeNodeMapping = m_guiApp->graphicsTreeNodeMappingDriverTable()->createMapping(entityTreeNode);
    if (item.gpxTreeNodeMapping) {
        const int selectMode = item.gpxTreeNodeMapping->selectionMode();
        if (selectMode != -1) {
            m_gfxScene.activateObjectSelection(gfxEntity.aisObject(), selectMode);
            m_gfxScene.foreachOwner(gfxEntity.aisObject(), selectMode, [&](const GraphicsOwnerPtr& ptr) {
                if (!item.gpxTreeNodeMapping->mapGraphicsOwner(ptr))
                    qDebug() << "Insertion failed";
            });

            //m_aisContext->Deactivate(gfxEntity.aisObject(), selectMode);
        }
    }

    GraphicsUtils::V3dView_fitAll(m_v3dView);
    const Bnd_Box itemBndBox = GraphicsUtils::AisObject_boundingBox(item.graphicsEntity.aisObject());
    BndUtils::add(&m_gpxBoundingBox, itemBndBox);
    m_vecGraphicsItem.emplace_back(std::move(item));
}

const GuiDocument::GraphicsItem* GuiDocument::findGraphicsItem(TreeNodeId entityTreeNodeId) const
{
    auto itFound = std::find_if(
                m_vecGraphicsItem.cbegin(),
                m_vecGraphicsItem.cend(),
                [=](const GraphicsItem& item) { return item.entityTreeNodeId == entityTreeNodeId; });
    return itFound != m_vecGraphicsItem.end() ? &(*itFound) : nullptr;
}

void GuiDocument::v3dViewTrihedronDisplay(Qt::Corner corner)
{
    constexpr double scale = 0.075;
    m_v3dView->TriedronDisplay(toOccCorner(corner), Quantity_NOC_GRAY50, scale, V3d_ZBUFFER);
}

} // namespace Mayo
