/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright Blender Foundation */

/** \file
 * \ingroup bke
 */

#include "BLI_array.hh"
#include "BLI_math.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BKE_deform.h"
#include "BKE_lib_id.h"
#include "BKE_lib_query.h"
#include "BKE_mesh.hh"
#include "BKE_mesh_mirror.h"
#include "BKE_modifier.h"

#include "bmesh.h"
#include "bmesh_tools.h"

#include "MEM_guardedalloc.h"

#include "MOD_modifiertypes.hh"

Mesh *BKE_mesh_mirror_bisect_on_mirror_plane_for_modifier(MirrorModifierData *mmd,
                                                          const Mesh *mesh,
                                                          int axis,
                                                          const float plane_co[3],
                                                          float plane_no[3])
{
  bool do_bisect_flip_axis = ((axis == 0 && mmd->flag & MOD_MIR_BISECT_FLIP_AXIS_X) ||
                              (axis == 1 && mmd->flag & MOD_MIR_BISECT_FLIP_AXIS_Y) ||
                              (axis == 2 && mmd->flag & MOD_MIR_BISECT_FLIP_AXIS_Z));

  const float bisect_distance = mmd->bisect_threshold;

  Mesh *result;
  BMesh *bm;
  BMIter viter;
  BMVert *v, *v_next;

  BMeshCreateParams bmesh_create_params{false};

  BMeshFromMeshParams bmesh_from_mesh_params{};
  bmesh_from_mesh_params.calc_face_normal = true;
  bmesh_from_mesh_params.calc_vert_normal = true;
  bmesh_from_mesh_params.cd_mask_extra.vmask = CD_MASK_ORIGINDEX;
  bmesh_from_mesh_params.cd_mask_extra.emask = CD_MASK_ORIGINDEX;
  bmesh_from_mesh_params.cd_mask_extra.pmask = CD_MASK_ORIGINDEX;

  bm = BKE_mesh_to_bmesh_ex(mesh, &bmesh_create_params, &bmesh_from_mesh_params);

  /* Define bisecting plane (aka mirror plane). */
  float plane[4];
  if (!do_bisect_flip_axis) {
    /* That reversed condition is a little weird, but for some reason that's how you keep
     * the part of the mesh which is on the non-mirrored side when flip option is disabled.
     * I think this is the expected behavior. */
    negate_v3(plane_no);
  }
  plane_from_point_normal_v3(plane, plane_co, plane_no);

  BM_mesh_bisect_plane(bm, plane, true, false, 0, 0, bisect_distance);

  /* Plane definitions for vert killing. */
  float plane_offset[4];
  copy_v3_v3(plane_offset, plane);
  plane_offset[3] = plane[3] - bisect_distance;

  /* Delete verts across the mirror plane. */
  BM_ITER_MESH_MUTABLE (v, v_next, &viter, bm, BM_VERTS_OF_MESH) {
    if (plane_point_side_v3(plane_offset, v->co) > 0.0f) {
      BM_vert_kill(bm, v);
    }
  }

  result = BKE_mesh_from_bmesh_for_eval_nomain(bm, nullptr, mesh);
  BM_mesh_free(bm);

  return result;
}

void BKE_mesh_mirror_apply_mirror_on_axis(struct Main *bmain,
                                          Mesh *mesh,
                                          const int axis,
                                          const float dist)
{
  BMeshCreateParams bmesh_create_params{};
  bmesh_create_params.use_toolflags = true;

  BMeshFromMeshParams bmesh_from_mesh_params{};
  bmesh_from_mesh_params.calc_face_normal = true;
  bmesh_from_mesh_params.calc_vert_normal = true;
  bmesh_from_mesh_params.cd_mask_extra.vmask = CD_MASK_SHAPEKEY;

  BMesh *bm = BKE_mesh_to_bmesh_ex(mesh, &bmesh_create_params, &bmesh_from_mesh_params);
  BMO_op_callf(bm,
               (BMO_FLAG_DEFAULTS & ~BMO_FLAG_RESPECT_HIDE),
               "symmetrize input=%avef direction=%i dist=%f use_shapekey=%b",
               axis,
               dist,
               true);

  BMeshToMeshParams bmesh_to_mesh_params{};
  bmesh_to_mesh_params.calc_object_remap = true;

  BM_mesh_bm_to_me(bmain, bm, mesh, &bmesh_to_mesh_params);
  BM_mesh_free(bm);
}

Mesh *BKE_mesh_mirror_apply_mirror_on_axis_for_modifier(MirrorModifierData *mmd,
                                                        Object *ob,
                                                        const Mesh *mesh,
                                                        const int axis,
                                                        const bool use_correct_order_on_merge,
                                                        int **r_vert_merge_map,
                                                        int *r_vert_merge_map_len)
{
  const float tolerance_sq = mmd->tolerance * mmd->tolerance;
  const bool do_vtargetmap = (mmd->flag & MOD_MIR_NO_MERGE) == 0 && r_vert_merge_map != nullptr;

  const bool do_bisect = ((axis == 0 && mmd->flag & MOD_MIR_BISECT_AXIS_X) ||
                          (axis == 1 && mmd->flag & MOD_MIR_BISECT_AXIS_Y) ||
                          (axis == 2 && mmd->flag & MOD_MIR_BISECT_AXIS_Z));

  float mtx[4][4];
  float plane_co[3], plane_no[3];
  int a, totshape;
  int *vtmap_a = nullptr, *vtmap_b = nullptr;

  /* mtx is the mirror transformation */
  unit_m4(mtx);
  mtx[axis][axis] = -1.0f;

  Object *mirror_ob = mmd->mirror_ob;
  if (mirror_ob != nullptr) {
    float tmp[4][4];
    float itmp[4][4];

    /* tmp is a transform from coords relative to the object's own origin,
     * to coords relative to the mirror object origin */
    invert_m4_m4(tmp, mirror_ob->object_to_world);
    mul_m4_m4m4(tmp, tmp, ob->object_to_world);

    /* itmp is the reverse transform back to origin-relative coordinates */
    invert_m4_m4(itmp, tmp);

    /* combine matrices to get a single matrix that translates coordinates into
     * mirror-object-relative space, does the mirror, and translates back to
     * origin-relative space */
    mul_m4_series(mtx, itmp, mtx, tmp);

    if (do_bisect) {
      copy_v3_v3(plane_co, itmp[3]);
      copy_v3_v3(plane_no, itmp[axis]);

      /* Account for non-uniform scale in `ob`, see: #87592. */
      float ob_scale[3] = {
          len_squared_v3(ob->object_to_world[0]),
          len_squared_v3(ob->object_to_world[1]),
          len_squared_v3(ob->object_to_world[2]),
      };
      /* Scale to avoid precision loss with extreme values. */
      const float ob_scale_max = max_fff(UNPACK3(ob_scale));
      if (LIKELY(ob_scale_max != 0.0f)) {
        mul_v3_fl(ob_scale, 1.0f / ob_scale_max);
        mul_v3_v3(plane_no, ob_scale);
      }
    }
  }
  else if (do_bisect) {
    copy_v3_v3(plane_co, mtx[3]);
    /* Need to negate here, since that axis is inverted (for mirror transform). */
    negate_v3_v3(plane_no, mtx[axis]);
  }

  Mesh *mesh_bisect = nullptr;
  if (do_bisect) {
    mesh_bisect = BKE_mesh_mirror_bisect_on_mirror_plane_for_modifier(
        mmd, mesh, axis, plane_co, plane_no);
    mesh = mesh_bisect;
  }

  const int src_verts_num = mesh->totvert;
  const int src_edges_num = mesh->totedge;
  const blender::OffsetIndices src_polys = mesh->polys();
  const int src_loops_num = mesh->totloop;

  Mesh *result = BKE_mesh_new_nomain_from_template(
      mesh, src_verts_num * 2, src_edges_num * 2, src_polys.size() * 2, src_loops_num * 2);

  /* Copy custom-data to original geometry. */
  CustomData_copy_data(&mesh->vdata, &result->vdata, 0, 0, src_verts_num);
  CustomData_copy_data(&mesh->edata, &result->edata, 0, 0, src_edges_num);
  CustomData_copy_data(&mesh->pdata, &result->pdata, 0, 0, src_polys.size());
  CustomData_copy_data(&mesh->ldata, &result->ldata, 0, 0, src_loops_num);

  /* Copy custom data to mirrored geometry. Loops are copied later. */
  CustomData_copy_data(&mesh->vdata, &result->vdata, 0, src_verts_num, src_verts_num);
  CustomData_copy_data(&mesh->edata, &result->edata, 0, src_edges_num, src_edges_num);
  CustomData_copy_data(&mesh->pdata, &result->pdata, 0, src_polys.size(), src_polys.size());

  if (do_vtargetmap) {
    /* second half is filled with -1 */
    *r_vert_merge_map = static_cast<int *>(
        MEM_malloc_arrayN(src_verts_num, sizeof(int[2]), "MOD_mirror tarmap"));

    vtmap_a = *r_vert_merge_map;
    vtmap_b = *r_vert_merge_map + src_verts_num;

    *r_vert_merge_map_len = 0;
  }

  /* mirror vertex coordinates */
  float(*positions)[3] = BKE_mesh_vert_positions_for_write(result);
  for (int i = 0; i < src_verts_num; i++) {
    const int vert_index_prev = i;
    const int vert_index = src_verts_num + i;
    mul_m4_v3(mtx, positions[vert_index]);

    if (do_vtargetmap) {
      /* Compare location of the original and mirrored vertex,
       * to see if they should be mapped for merging.
       *
       * Always merge from the copied into the original vertices so it's possible to
       * generate a 1:1 mapping by scanning vertices from the beginning of the array
       * as is done in #BKE_editmesh_vert_coords_when_deformed. Without this,
       * the coordinates returned will sometimes point to the copied vertex locations, see:
       * #91444.
       *
       * However, such a change also affects non-versionable things like some modifiers binding, so
       * we cannot enforce that behavior on existing modifiers, in which case we keep using the
       * old, incorrect behavior of merging the source vertex into its copy.
       */
      if (use_correct_order_on_merge) {
        if (UNLIKELY(len_squared_v3v3(positions[vert_index_prev], positions[vert_index]) <
                     tolerance_sq)) {
          *vtmap_b = i;
          (*r_vert_merge_map_len)++;

          /* average location */
          mid_v3_v3v3(positions[vert_index], positions[vert_index_prev], positions[vert_index]);
          copy_v3_v3(positions[vert_index_prev], positions[vert_index]);
        }
        else {
          *vtmap_b = -1;
        }

        /* Fill here to avoid 2x loops. */
        *vtmap_a = -1;
      }
      else {
        if (UNLIKELY(len_squared_v3v3(positions[vert_index_prev], positions[vert_index]) <
                     tolerance_sq)) {
          *vtmap_a = src_verts_num + i;
          (*r_vert_merge_map_len)++;

          /* average location */
          mid_v3_v3v3(positions[vert_index], positions[vert_index_prev], positions[vert_index]);
          copy_v3_v3(positions[vert_index_prev], positions[vert_index]);
        }
        else {
          *vtmap_a = -1;
        }

        /* Fill here to avoid 2x loops. */
        *vtmap_b = -1;
      }

      vtmap_a++;
      vtmap_b++;
    }
  }

  /* handle shape keys */
  totshape = CustomData_number_of_layers(&result->vdata, CD_SHAPEKEY);
  for (a = 0; a < totshape; a++) {
    float(*cos)[3] = static_cast<float(*)[3]>(
        CustomData_get_layer_n_for_write(&result->vdata, CD_SHAPEKEY, a, result->totvert));
    for (int i = src_verts_num; i < result->totvert; i++) {
      mul_m4_v3(mtx, cos[i]);
    }
  }

  blender::MutableSpan<blender::int2> result_edges = result->edges_for_write();
  blender::MutableSpan<int> result_poly_offsets = result->poly_offsets_for_write();
  blender::MutableSpan<int> result_corner_verts = result->corner_verts_for_write();
  blender::MutableSpan<int> result_corner_edges = result->corner_edges_for_write();

  /* adjust mirrored edge vertex indices */
  for (const int i : result_edges.index_range().drop_front(src_edges_num)) {
    result_edges[i] += src_verts_num;
  }

  result_poly_offsets.take_front(src_polys.size()).copy_from(mesh->poly_offsets().drop_back(1));
  for (const int i : src_polys.index_range()) {
    result_poly_offsets[src_polys.size() + i] = src_polys[i].start() + src_loops_num;
  }
  const blender::OffsetIndices result_polys = result->polys();

  /* reverse loop order (normals) */
  for (const int i : src_polys.index_range()) {
    const blender::IndexRange src_poly = src_polys[i];
    const int mirror_i = src_polys.size() + i;
    const blender::IndexRange mirror_poly = result_polys[mirror_i];

    /* reverse the loop, but we keep the first vertex in the face the same,
     * to ensure that quads are split the same way as on the other side */
    CustomData_copy_data(&mesh->ldata, &result->ldata, src_poly.start(), mirror_poly.start(), 1);

    for (int j = 1; j < mirror_poly.size(); j++) {
      CustomData_copy_data(&mesh->ldata, &result->ldata, src_poly[j], mirror_poly.last(j - 1), 1);
    }

    blender::MutableSpan<int> mirror_poly_edges = result_corner_edges.slice(mirror_poly);
    const int e = mirror_poly_edges.first();
    for (int j = 0; j < mirror_poly.size() - 1; j++) {
      mirror_poly_edges[j] = mirror_poly_edges[j + 1];
    }
    mirror_poly_edges.last() = e;
  }

  /* adjust mirrored loop vertex and edge indices */
  for (const int i : result_corner_verts.index_range().drop_front(src_loops_num)) {
    result_corner_verts[i] += src_verts_num;
  }
  for (const int i : result_corner_edges.index_range().drop_front(src_loops_num)) {
    result_corner_edges[i] += src_edges_num;
  }

  if (!mesh->runtime->subsurf_optimal_display_edges.is_empty()) {
    const blender::BoundedBitSpan src = mesh->runtime->subsurf_optimal_display_edges;
    result->runtime->subsurf_optimal_display_edges.resize(result->totedge);
    blender::MutableBoundedBitSpan dst = result->runtime->subsurf_optimal_display_edges;
    dst.take_front(src.size()).copy_from(src);
    dst.take_back(src.size()).copy_from(src);
  }

  /* handle uvs,
   * let tessface recalc handle updating the MTFace data */
  if (mmd->flag & (MOD_MIR_MIRROR_U | MOD_MIR_MIRROR_V) ||
      (is_zero_v2(mmd->uv_offset_copy) == false))
  {
    const bool do_mirr_u = (mmd->flag & MOD_MIR_MIRROR_U) != 0;
    const bool do_mirr_v = (mmd->flag & MOD_MIR_MIRROR_V) != 0;
    /* If set, flip around center of each tile. */
    const bool do_mirr_udim = (mmd->flag & MOD_MIR_MIRROR_UDIM) != 0;

    const int totuv = CustomData_number_of_layers(&result->ldata, CD_PROP_FLOAT2);

    for (a = 0; a < totuv; a++) {
      float(*dmloopuv)[2] = static_cast<float(*)[2]>(
          CustomData_get_layer_n_for_write(&result->ldata, CD_PROP_FLOAT2, a, result->totloop));
      int j = src_loops_num;
      dmloopuv += j; /* second set of loops only */
      for (; j-- > 0; dmloopuv++) {
        if (do_mirr_u) {
          float u = (*dmloopuv)[0];
          if (do_mirr_udim) {
            (*dmloopuv)[0] = ceilf(u) - fmodf(u, 1.0f) + mmd->uv_offset[0];
          }
          else {
            (*dmloopuv)[0] = 1.0f - u + mmd->uv_offset[0];
          }
        }
        if (do_mirr_v) {
          float v = (*dmloopuv)[1];
          if (do_mirr_udim) {
            (*dmloopuv)[1] = ceilf(v) - fmodf(v, 1.0f) + mmd->uv_offset[1];
          }
          else {
            (*dmloopuv)[1] = 1.0f - v + mmd->uv_offset[1];
          }
        }
        (*dmloopuv)[0] += mmd->uv_offset_copy[0];
        (*dmloopuv)[1] += mmd->uv_offset_copy[1];
      }
    }
  }

  /* handle custom split normals */
  if (ob->type == OB_MESH && (((Mesh *)ob->data)->flag & ME_AUTOSMOOTH) &&
      CustomData_has_layer(&result->ldata, CD_CUSTOMLOOPNORMAL) && result->totpoly > 0)
  {
    blender::Array<blender::float3> loop_normals(result_corner_verts.size());
    CustomData *ldata = &result->ldata;
    blender::short2 *clnors = static_cast<blender::short2 *>(
        CustomData_get_layer_for_write(ldata, CD_CUSTOMLOOPNORMAL, result->totloop));
    blender::bke::mesh::CornerNormalSpaceArray lnors_spacearr;

    /* The transform matrix of a normal must be
     * the transpose of inverse of transform matrix of the geometry... */
    float mtx_nor[4][4];
    invert_m4_m4(mtx_nor, mtx);
    transpose_m4(mtx_nor);

    /* calculate custom normals into loop_normals, then mirror first half into second half */

    const bool *sharp_edges = static_cast<const bool *>(
        CustomData_get_layer_named(&result->edata, CD_PROP_BOOL, "sharp_edge"));
    const bool *sharp_faces = static_cast<const bool *>(
        CustomData_get_layer_named(&result->pdata, CD_PROP_BOOL, "sharp_face"));
    blender::bke::mesh::normals_calc_loop(result->vert_positions(),
                                          result_edges,
                                          result_polys,
                                          result_corner_verts,
                                          result_corner_edges,
                                          {},
                                          result->vert_normals(),
                                          result->poly_normals(),
                                          sharp_edges,
                                          sharp_faces,
                                          true,
                                          result->smoothresh,
                                          clnors,
                                          &lnors_spacearr,
                                          loop_normals);

    /* mirroring has to account for loops being reversed in polys in second half */
    for (const int i : src_polys.index_range()) {
      const blender::IndexRange src_poly = src_polys[i];
      const int mirror_i = src_polys.size() + i;

      for (const int j : src_poly) {
        int mirrorj = result_polys[mirror_i].start();
        if (j > src_poly.start()) {
          mirrorj += result_polys[mirror_i].size() - (j - src_poly.start());
        }

        const blender::float3 orig_normal = loop_normals[mirrorj];
        copy_v3_v3(loop_normals[mirrorj], loop_normals[j]);
        mul_m4_v3(mtx_nor, loop_normals[mirrorj]);

        const int space_index = lnors_spacearr.corner_space_indices[mirrorj];
        blender::bke::mesh::lnor_space_custom_normal_to_data(&lnors_spacearr.spaces[space_index],
                                                             orig_normal,
                                                             loop_normals[mirrorj],
                                                             clnors[mirrorj]);
      }
    }
  }

  /* handle vgroup stuff */
  if (BKE_object_supports_vertex_groups(ob)) {
    if ((mmd->flag & MOD_MIR_VGROUP) && CustomData_has_layer(&result->vdata, CD_MDEFORMVERT)) {
      MDeformVert *dvert = BKE_mesh_deform_verts_for_write(result) + src_verts_num;
      int flip_map_len = 0;
      int *flip_map = BKE_object_defgroup_flip_map(ob, false, &flip_map_len);
      if (flip_map) {
        for (int i = 0; i < src_verts_num; dvert++, i++) {
          /* merged vertices get both groups, others get flipped */
          if (use_correct_order_on_merge && do_vtargetmap &&
              ((*r_vert_merge_map)[i + src_verts_num] != -1)) {
            BKE_defvert_flip_merged(dvert - src_verts_num, flip_map, flip_map_len);
          }
          else if (!use_correct_order_on_merge && do_vtargetmap && ((*r_vert_merge_map)[i] != -1))
          {
            BKE_defvert_flip_merged(dvert, flip_map, flip_map_len);
          }
          else {
            BKE_defvert_flip(dvert, flip_map, flip_map_len);
          }
        }

        MEM_freeN(flip_map);
      }
    }
  }

  if (mesh_bisect != nullptr) {
    BKE_id_free(nullptr, mesh_bisect);
  }
  return result;
}
