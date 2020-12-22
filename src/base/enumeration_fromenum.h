/****************************************************************************
** Copyright (c) 2020, Fougue Ltd. <http://www.fougue.pro>
** All rights reserved.
** See license at https://github.com/fougue/mayo/blob/master/LICENSE.txt
****************************************************************************/

#pragma once

#include "meta_enum.h"
#include "property_enumeration.h"

#include <QtCore/QMetaEnum>

// --
// -- Implementation of template functions Enumeration::fromQENUM() and Enumeration::fromQENUM()
// --

namespace Mayo {

template<typename QENUM>
Enumeration Enumeration::fromQENUM(const QByteArray& textIdContext)
{
    auto fnQByteArrayFrowRawData = [](const char* str) {
        return QByteArray::fromRawData(str, int(std::strlen(str)));
    };

    Enumeration enumObject;
    const QMetaEnum metaEnum = QMetaEnum::fromType<QENUM>();
    for (int i = 0; i < metaEnum.keyCount(); ++i) {
        const char* strKey = metaEnum.key(i);
        const TextId keyTextId = { textIdContext, fnQByteArrayFrowRawData(strKey) };
        enumObject.addItem(metaEnum.value(i), keyTextId);
    }

    return enumObject;
}

template<typename ENUM>
Enumeration Enumeration::fromEnum(const QByteArray& textIdContext)
{
    auto fnQByteArrayFrowRawData = [](std::string_view str) {
        return QByteArray::fromRawData(str.data(), int(str.size()));
    };

    Enumeration enumObject;
    for (const auto& entry : MetaEnum::entries<ENUM>()) {
        const TextId keyTextId = { textIdContext, fnQByteArrayFrowRawData(entry.second) };
        enumObject.addItem(int(entry.first), keyTextId);
    }

    return enumObject;
}

} // namespace Mayo
