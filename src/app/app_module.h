/****************************************************************************
** Copyright (c) 2020, Fougue Ltd. <http://www.fougue.pro>
** All rights reserved.
** See license at https://github.com/fougue/mayo/blob/master/LICENSE.txt
****************************************************************************/

#pragma once

#include "recent_files.h"

#include "../base/application_ptr.h"
#include "../base/io_parameters_provider.h"
#include "../base/property.h"
#include "../base/property_builtins.h"
#include "../base/property_enumeration.h"
#include "../base/settings_index.h"
#include "../base/string_utils.h"

#include <fougtools/qttools/core/qbytearray_hfunc.h>
#include <QtCore/QObject>
#include <unordered_map>
#include <vector>

namespace Mayo {

class GuiApplication;
class GuiDocument;

class AppModule :
        public QObject,
        public PropertyGroup,
        public IO::ParametersProvider
{
    Q_OBJECT
    MAYO_DECLARE_TEXT_ID_FUNCTIONS(Mayo::AppModule)
public:
    AppModule(Application* app);
    static AppModule* get(const ApplicationPtr& app);

    StringUtils::TextOptions defaultTextOptions() const;

    static QString qmFilePath(const QByteArray& languageCode);

    const PropertyGroup* findReaderParameters(const IO::Format& format) const override;
    const PropertyGroup* findWriterParameters(const IO::Format& format) const override;

    void prependRecentFile(const QString& filepath);
    const RecentFile* findRecentFile(const QString& filepath) const;
    void recordRecentFileThumbnail(GuiDocument* guiDoc);
    void recordRecentFileThumbnails(GuiApplication* guiApp);
    QSize recentFileThumbnailSize() const { return { 190, 150 }; }

    // System
    const Settings_GroupIndex groupId_system;
    const Settings_SectionIndex sectionId_systemUnits;
    PropertyInt unitSystemDecimals;
    PropertyEnumeration unitSystemSchema;
    // Application
    const Settings_GroupIndex groupId_application;
    PropertyEnumeration language;
    PropertyRecentFiles recentFiles;
    PropertyQString lastOpenDir;
    PropertyQString lastSelectedFormatFilter;
    PropertyBool linkWithDocumentSelector;
    // Graphics
    const Settings_GroupIndex groupId_graphics;
    PropertyBool defaultShowOriginTrihedron;
    // -- ClipPlanes
    const Settings_SectionIndex sectionId_graphicsClipPlanes;
    PropertyBool clipPlanesCappingOn;
    PropertyBool clipPlanesCappingHatchOn;
    // -- MeshDefaults
    const Settings_SectionIndex sectionId_graphicsMeshDefaults;
    PropertyOccColor meshDefaultsColor;
    PropertyOccColor meshDefaultsEdgeColor;
    PropertyEnumeration meshDefaultsMaterial;
    PropertyBool meshDefaultsShowEdges;
    PropertyBool meshDefaultsShowNodes;

protected:
    void onPropertyChanged(Property* prop) override;

private:
    Application* m_app = nullptr;
    std::vector<std::unique_ptr<PropertyGroup>> m_vecPtrPropertyGroup;
    std::unordered_map<QByteArray, PropertyGroup*> m_mapFormatReaderParameters;
    std::unordered_map<QByteArray, PropertyGroup*> m_mapFormatWriterParameters;
};

} // namespace Mayo
