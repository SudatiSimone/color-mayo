/****************************************************************************
** Copyright (c) 2020, Fougue Ltd. <http://www.fougue.pro>
** All rights reserved.
** See license at https://github.com/fougue/mayo/blob/master/LICENSE.txt
****************************************************************************/

#include "io_occ_step.h"
#include "io_occ_caf.h"
#include "occ_static_variables_rollback.h"
#include "property_builtins.h"
#include "property_enumeration.h"
#include "task_progress.h"
#include "tkernel_utils.h"
#include "enumeration_fromenum.h"

#include <Interface_Static.hxx>
#include <STEPCAFControl_Controller.hxx>

namespace Mayo {
namespace IO {

class OccStepReader::Properties : public PropertyGroup {
    MAYO_DECLARE_TEXT_ID_FUNCTIONS(Mayo::IO::OccStepReader_Properties)
public:
    Properties(PropertyGroup* parentGroup)
        : PropertyGroup(parentGroup),
          productContext(this, textId("productContext"), &enumProductContext),
          assemblyLevel(this, textId("assemblyLevel"), &enumAssemblyLevel),
          preferredShapeRepresentation(this, textId("preferredShapeRepresentation"), &enumShapeRepresentation()),
          readShapeAspect(this, textId("readShapeAspect")),
          readSubShapesNames(this, textId("readSubShapesNames")),
          encoding(this, textId("encoding"), &enumEncoding())
    {
        this->productContext.setDescription(
                    textIdTr("When reading AP 209 STEP files, allows selecting either only `design` "
                             "or `analysis`, or both types of products for translation\n"
                             "Note that in AP 203 and AP214 files all products should be marked as "
                             "`design`, so if this mode is set to `analysis`, nothing will be read"));
        this->assemblyLevel.setDescription(
                    textIdTr("Specifies which data should be read for the products found in the STEP file"));
        this->preferredShapeRepresentation.setDescription(
                    textIdTr("Specifies preferred type of representation of the shape of the product, in "
                             "case if a STEP file contains more than one representation (i.e. multiple "
                             "`PRODUCT_DEFINITION_SHAPE` entities) for a single product"));
        this->readShapeAspect.setDescription(
                    textIdTr("Defines whether shapes associated with the `PRODUCT_DEFINITION_SHAPE` entity "
                             "of the product via `SHAPE_ASPECT` should be translated.\n"
                             "This kind of association was used for the representation of hybrid models (i.e. models "
                             "whose shape is composed of different types of representations) in AP 203 files "
                             "before 1998, but it is also used to associate auxiliary information with the "
                             "sub-shapes of the part. Though STEP translator tries to recognize such cases "
                             "correctly, this parameter may be useful to avoid unconditionally translation "
                             "of shapes associated via `SHAPE_ASPECT` entities."));
        this->readSubShapesNames.setDescription(
                    textIdTr("Indicates whether to read sub-shape names from 'Name' attributes of "
                             "STEP Representation Items"));
    }

    void restoreDefaults() override {
        const OccStepReader::Parameters params;
        this->productContext.setValue(params.productContext);
        this->assemblyLevel.setValue(params.assemblyLevel);
        this->readShapeAspect.setValue(params.readShapeAspect);
        this->readSubShapesNames.setValue(params.readSubShapesNames);
        this->encoding.setValue(params.encoding);
    }

    inline static const Enumeration enumProductContext = {
        { int(ProductContext::Design), textId("Design"),
          textIdTr("Translate only products that have `PRODUCT_DEFINITION_CONTEXT` with field "
                   "`life_cycle_stage` set to `design`") },
        { int(ProductContext::Analysis), textId("Analysis"),
          textIdTr("Translate only products that have `PRODUCT_DEFINITION_CONTEXT` with field "
                   "`life_cycle_stage` set to `analysis`") },
        { int(ProductContext::Both), textId("Both"), textIdTr("Translates all products") }
    };

    inline static const Enumeration enumAssemblyLevel = {
        { int(AssemblyLevel::Assembly), textId("Assembly"),
          textIdTr("Translate the assembly structure and shapes associated with parts only "
                   "(not with sub-assemblies)") },
        { int(AssemblyLevel::Structure), textId("Structure"),
          textIdTr("Translate only the assembly structure without shapes (a structure of "
                   "empty compounds). This mode can be useful as an intermediate step in "
                   "applications requiring specialized processing of assembly parts") },
        { int(AssemblyLevel::Shape), textId("Shape"),
          textIdTr("Translate only shapes associated with the product, ignoring the assembly "
                   "structure (if any). This can be useful to translate only a shape associated "
                   "with specific product, as a complement to assembly mode") },
        { int(AssemblyLevel::All), textId("All"),
          textIdTr("Translate both the assembly structure and all associated shapes. "
                   "If both shape and sub-assemblies are associated with the same product, "
                   "all of them are read and put in a single compound") }
    };

    static const Enumeration& enumShapeRepresentation() {
        static Enumeration enumObject = Enumeration::fromEnum<ShapeRepresentation>(textIdContext());
        enumObject.setDescription(
                    int(ShapeRepresentation::All),
                    textIdTr("Translate all representations(if more than one, put in compound)"));
        return enumObject;
    }

    static const Enumeration& enumEncoding() {
        static Enumeration enumObject = Enumeration::fromEnum<Encoding>(textIdContext());
        enumObject.setDescription(Encoding::Shift_JIS, textIdTr("Shift Japanese Industrial Standards"));
        enumObject.setDescription(
                    Encoding::EUC,
                    textIdTr("EUC (Extended Unix Code), multi-byte encoding primarily for Japanese, "
                             "Korean, and simplified Chinese"));
        enumObject.setDescription(Encoding::GB, textIdTr("GB (Guobiao) encoding for Simplified Chinese"));
        return enumObject;
    }

    PropertyEnumeration productContext;
    PropertyEnumeration assemblyLevel;
    PropertyEnumeration preferredShapeRepresentation;
    PropertyBool readShapeAspect;
    PropertyBool readSubShapesNames;
    PropertyEnumeration encoding;
};

OccStepReader::OccStepReader()
{
    STEPCAFControl_Controller::Init();
    m_reader.SetColorMode(true);
    m_reader.SetNameMode(true);
    m_reader.SetLayerMode(true);
    m_reader.SetPropsMode(true);
    m_reader.SetGDTMode(true);
    m_reader.SetMatMode(true);
    m_reader.SetViewMode(true);
}

bool OccStepReader::readFile(const QString& filepath, TaskProgress* progress)
{
    MayoIO_CafGlobalScopedLock(cafLock);
    OccStaticVariablesRollback rollback;
    this->changeStaticVariables(&rollback);
    return Private::cafReadFile(m_reader, filepath, progress);
}

bool OccStepReader::transfer(DocumentPtr doc, TaskProgress* progress)
{
    MayoIO_CafGlobalScopedLock(cafLock);
    OccStaticVariablesRollback rollback;
    this->changeStaticVariables(&rollback);
    return Private::cafTransfer(m_reader, doc, progress);
}

std::unique_ptr<PropertyGroup> OccStepReader::createProperties(PropertyGroup* parentGroup)
{
    return std::make_unique<Properties>(parentGroup);
}

void OccStepReader::applyProperties(const PropertyGroup* group)
{
    auto ptr = dynamic_cast<const Properties*>(group);
    if (ptr) {
        m_params.productContext = ptr->productContext.valueAs<ProductContext>();
        m_params.assemblyLevel = ptr->assemblyLevel.valueAs<AssemblyLevel>();
        m_params.preferredShapeRepresentation = ptr->preferredShapeRepresentation.valueAs<ShapeRepresentation>();
        m_params.readShapeAspect = ptr->readShapeAspect.value();
        m_params.readSubShapesNames = ptr->readSubShapesNames.value();
        m_params.encoding = ptr->encoding.valueAs<Encoding>();
    }
}

void OccStepReader::changeStaticVariables(OccStaticVariablesRollback* rollback) const
{
    auto fnOccEncoding = [](Encoding code) {
        switch (code) {
        case Encoding::Shift_JIS: return "SJIS";
        case Encoding::EUC: return "EUC";
        case Encoding::ANSI: return "ANSI";
        case Encoding::GB: return "GB";
        case Encoding::UTF8: return "UTF8";
#if OCC_VERSION_HEX >= OCC_VERSION_CHECK(7, 5, 0)
        // Windows-native ("ANSI") 8-bit code pages
        case Encoding::CP_1250: return "CP1250";
        case Encoding::CP_1251: return "CP1251";
        case Encoding::CP_1252: return "CP1252";
        case Encoding::CP_1253: return "CP1253";
        case Encoding::CP_1254: return "CP1254";
        case Encoding::CP_1255: return "CP1255";
        case Encoding::CP_1256: return "CP1256";
        case Encoding::CP_1257: return "CP1257";
        case Encoding::CP_1258: return "CP1258";
        // ISO8859 8-bit code pages
        case Encoding::ISO_8859_1: return "iso8859-1";
        case Encoding::ISO_8859_2: return "iso8859-2";
        case Encoding::ISO_8859_3: return "iso8859-3";
        case Encoding::ISO_8859_4: return "iso8859-4";
        case Encoding::ISO_8859_5: return "iso8859-5";
        case Encoding::ISO_8859_6: return "iso8859-6";
        case Encoding::ISO_8859_7: return "iso8859-7";
        case Encoding::ISO_8859_8: return "iso8859-8";
        case Encoding::ISO_8859_9: return "iso8859-9";
#endif
        }
        Q_UNREACHABLE();
    };

    const char strKeyReadStepCodePage[] =
#if OCC_VERSION_HEX >= OCC_VERSION_CHECK(7, 5, 0)
        "read.step.codepage";
#else
        "read.stepcaf.codepage";
#endif

    rollback->change("read.step.product.context", int(m_params.productContext));
    rollback->change("read.step.assembly.level", int(m_params.assemblyLevel));
    rollback->change("read.step.shape.repr", int(m_params.preferredShapeRepresentation));
    rollback->change("read.step.shape.aspect", int(m_params.readShapeAspect ? 1 : 0));
    rollback->change("read.stepcaf.subshapes.name", int(m_params.readSubShapesNames ? 1 : 0));
    rollback->change(strKeyReadStepCodePage, fnOccEncoding(m_params.encoding));
}

class OccStepWriter::Properties : public PropertyGroup {
    MAYO_DECLARE_TEXT_ID_FUNCTIONS(Mayo::IO::OccStepWriter_Properties)
public:
    Properties(PropertyGroup* parentGroup)
        : PropertyGroup(parentGroup),
          schema(this, textId("schema"), &enumSchema),
          lengthUnit(this, textId("lengthUnit"), &OccCommon::enumerationLengthUnit()),
          assemblyMode(this, textId("assemblyMode"), &enumAssemblyMode),
          freeVertexMode(this, textId("freeVertexMode"), &enumFreeVertexMode),
          writePCurves(this, textId("writeParametericCurves")),
          writeSubShapesNames(this, textId("writeSubShapesNames"))
    {
        this->schema.setDescription(
                    textIdTr("Version of schema used for the output STEP file"));
        this->lengthUnit.setDescription(
                    textIdTr("Defines a unit in which the STEP file should be written. If set to "
                             "unit other than millimeter, the model is converted to these units "
                             "during the translation"));
        this->freeVertexMode.setDescription(
                    textIdTr("Parameter to write all free vertices in one SDR (name and style of "
                             "vertex are lost) or each vertex in its own SDR (name and style of "
                             "vertex are exported)"));
        // The behavior at import is controlled by new write.step.vertex.mode parameter,
        // which can be equal to:
        // 0 - One Compound: All free vertices are united into one compound and exported
        // in one shape definition representation (vertex name and style are lost). This
        // mode is used by default.
        // 1 - Single Vertex: Each vertex is exported in its own SHAPE DEFINITION
        // REPRESENTATION (vertex name and style are not lost, but the STEP file size
        // increases).
        this->writePCurves.setDescription(
                    textIdTr("Indicates whether parametric curves (curves in parametric space of surface) should be "
                             "written into the STEP file.\n"
                             "It can be disabled in order to minimize the size of the resulting file."));
        this->writeSubShapesNames.setDescription(
                    textIdTr("Indicates whether to write sub-shape names to 'Name' attributes of "
                             "STEP Representation Items"));
    }

    void restoreDefaults() override {
        const OccStepWriter::Parameters params;
        this->schema.setValue(params.schema);
        this->lengthUnit.setValue(params.lengthUnit);
        this->assemblyMode.setValue(params.assemblyMode);
        this->freeVertexMode.setValue(params.freeVertexMode);
        this->writePCurves.setValue(params.writeParametricCurves);
        this->writeSubShapesNames.setValue(params.writeSubShapesNames);
    }

    inline static const auto enumSchema = Enumeration::fromEnum<Schema>(textIdContext());
    inline static const auto enumAssemblyMode = Enumeration::fromEnum<AssemblyMode>(textIdContext());
    inline static const auto enumFreeVertexMode = Enumeration::fromEnum<FreeVertexMode>(textIdContext());

    PropertyEnumeration schema;
    PropertyEnumeration lengthUnit;
    PropertyEnumeration assemblyMode;
    PropertyEnumeration freeVertexMode;
    PropertyBool writePCurves;
    PropertyBool writeSubShapesNames;
};

OccStepWriter::OccStepWriter()
{
    STEPCAFControl_Controller::Init();
    m_writer.SetColorMode(true);
    m_writer.SetNameMode(true);
    m_writer.SetLayerMode(true);
    m_writer.SetPropsMode(true);
    m_writer.SetDimTolMode(true);
    m_writer.SetMaterialMode(true);
}

bool OccStepWriter::transfer(Span<const ApplicationItem> appItems, TaskProgress* progress)
{
    MayoIO_CafGlobalScopedLock(cafLock);
    OccStaticVariablesRollback rollback;
    this->changeStaticVariables(&rollback);
    return Private::cafTransfer(m_writer, appItems, progress);
}

bool OccStepWriter::writeFile(const QString& filepath, TaskProgress* progress)
{
    MayoIO_CafGlobalScopedLock(cafLock);
    OccStaticVariablesRollback rollback;
    this->changeStaticVariables(&rollback);
    const IFSelect_ReturnStatus err = m_writer.Write(filepath.toUtf8().constData());
    progress->setValue(100);
    return err == IFSelect_RetDone;
}

std::unique_ptr<PropertyGroup> OccStepWriter::createProperties(PropertyGroup* parentGroup)
{
    return std::make_unique<Properties>(parentGroup);
}

void OccStepWriter::applyProperties(const PropertyGroup* group)
{
    auto ptr = dynamic_cast<const Properties*>(group);
    if (ptr) {
        m_params.schema = ptr->schema.valueAs<Schema>();
        m_params.lengthUnit = ptr->lengthUnit.valueAs<LengthUnit>();
        m_params.assemblyMode = ptr->assemblyMode.valueAs<AssemblyMode>();
        m_params.freeVertexMode = ptr->freeVertexMode.valueAs<FreeVertexMode>();
        m_params.writeParametricCurves = ptr->writePCurves.value();
        m_params.writeSubShapesNames = ptr->writeSubShapesNames.value();
    }
}

void OccStepWriter::changeStaticVariables(OccStaticVariablesRollback* rollback)
{
    const int previousSchema = Interface_Static::IVal("write.step.schema");
    rollback->change("write.step.schema", int(m_params.schema));
    if (int(m_params.schema) != previousSchema) {
        // NOTE from $OCC_7.4.0_DIR/doc/pdf/user_guides/occt_step.pdf (page 26)
        // For the parameter "write.step.schema" to take effect, method STEPControl_Writer::Model(true)
        // should be called after changing this parameter (corresponding command in DRAW is "newmodel")
        m_writer.ChangeWriter().Model(true);
    }

    rollback->change("write.step.unit", OccCommon::toCafString(m_params.lengthUnit));
    rollback->change("write.step.assembly", int(m_params.assemblyMode));
    rollback->change("write.step.vertex.mode", int(m_params.freeVertexMode));
    rollback->change("write.surfacecurve.mode", int(m_params.writeParametricCurves ? 1 : 0));
    rollback->change("write.stepcaf.subshapes.name", int(m_params.writeSubShapesNames ? 1 : 0));
}

} // namespace IO
} // namespace Mayo
