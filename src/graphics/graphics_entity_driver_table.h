/****************************************************************************
** Copyright (c) 2020, Fougue Ltd. <http://www.fougue.pro>
** All rights reserved.
** See license at https://github.com/fougue/mayo/blob/master/LICENSE.txt
****************************************************************************/

#pragma once

#include "graphics_entity.h"
#include "graphics_entity_driver.h"
#include "../base/span.h"
#include <memory>
#include <vector>

namespace Mayo {

class GraphicsEntityDriver;

class GraphicsEntityDriverTable {
public:
    using DriverPtr = std::unique_ptr<GraphicsEntityDriver>;

    void addDriver(DriverPtr driver);
    Span<const DriverPtr> drivers() const { return m_vecDriver; }

    GraphicsEntity createEntity(const TDF_Label& label) const;

private:
    std::vector<DriverPtr> m_vecDriver;
};

} // namespace Mayo
