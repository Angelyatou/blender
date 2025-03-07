/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2019 Blender Foundation. */

/** \file
 * \ingroup draw_engine
 *
 * Engine for drawing a selection map where the pixels indicate the selection indices.
 */

#include "DRW_engine.h"
#include "DRW_render.h"

#include "DEG_depsgraph_query.h"

#include "ED_view3d.h"

#include "UI_interface.h"

#include "BKE_duplilist.h"
#include "BKE_object.h"
#include "BKE_paint.h"

#include "GPU_capabilities.h"

#include "DNA_space_types.h"

#include "draw_manager.hh"
#include "overlay_next_instance.hh"

#include "overlay_engine.h"
#include "overlay_next_private.hh"

using namespace blender::draw;

using Instance = blender::draw::overlay::Instance;

/* -------------------------------------------------------------------- */
/** \name Engine Instance
 * \{ */

static void OVERLAY_next_engine_init(void *vedata)
{
  if (!GPU_shader_storage_buffer_objects_support()) {
    return;
  }

  OVERLAY_Data *ved = reinterpret_cast<OVERLAY_Data *>(vedata);

  if (ved->instance == nullptr) {
    ved->instance = new Instance(select::SelectionType::DISABLED);
  }

  reinterpret_cast<Instance *>(ved->instance)->init();
}

static void OVERLAY_next_cache_init(void *vedata)
{
  if (!GPU_shader_storage_buffer_objects_support()) {
    return;
  }
  reinterpret_cast<Instance *>(reinterpret_cast<OVERLAY_Data *>(vedata)->instance)->begin_sync();
}

static void OVERLAY_next_cache_populate(void *vedata, Object *object)
{
  if (!GPU_shader_storage_buffer_objects_support()) {
    return;
  }
  ObjectRef ref;
  ref.object = object;
  ref.dupli_object = DRW_object_get_dupli(object);
  ref.dupli_parent = DRW_object_get_dupli_parent(object);

  reinterpret_cast<Instance *>(reinterpret_cast<OVERLAY_Data *>(vedata)->instance)
      ->object_sync(ref, *DRW_manager_get());
}

static void OVERLAY_next_cache_finish(void *vedata)
{
  if (!GPU_shader_storage_buffer_objects_support()) {
    return;
  }
  reinterpret_cast<Instance *>(reinterpret_cast<OVERLAY_Data *>(vedata)->instance)->end_sync();
}

static void OVERLAY_next_draw_scene(void *vedata)
{
  if (!GPU_shader_storage_buffer_objects_support()) {
    return;
  }

  reinterpret_cast<Instance *>(reinterpret_cast<OVERLAY_Data *>(vedata)->instance)
      ->draw(*DRW_manager_get());
}

static void OVERLAY_next_instance_free(void *instance_)
{
  auto *instance = (Instance *)instance_;
  if (instance != nullptr) {
    delete instance;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Engine Type
 * \{ */

static const DrawEngineDataSize overlay_data_size = DRW_VIEWPORT_DATA_SIZE(OVERLAY_Data);

DrawEngineType draw_engine_overlay_next_type = {
    nullptr,
    nullptr,
    N_("Overlay"),
    &overlay_data_size,
    &OVERLAY_next_engine_init,
    nullptr,
    &OVERLAY_next_instance_free,
    &OVERLAY_next_cache_init,
    &OVERLAY_next_cache_populate,
    &OVERLAY_next_cache_finish,
    &OVERLAY_next_draw_scene,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
};

/** \} */
