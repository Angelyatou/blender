/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BKE_geometry_set.hh"

namespace blender::bke {

/**
 * \note This doesn't extract instances from the "dupli" system for non-geometry-nodes instances.
 */
GeometrySet object_get_evaluated_geometry_set(const Object &object);

}  // namespace blender::bke
