/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include <float.h>
#include <limits.h>
#include <stdlib.h>

#include "DNA_armature_types.h"
#include "DNA_cachefile_types.h"
#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_force_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_math.h"

#include "BLT_translation.h"

#include "BKE_animsys.h"
#include "BKE_attribute.h"
#include "BKE_curveprofile.h"
#include "BKE_data_transfer.h"
#include "BKE_dynamicpaint.h"
#include "BKE_effect.h"
#include "BKE_fluid.h" /* For BKE_fluid_modifier_free & BKE_fluid_modifier_create_type_data */
#include "BKE_idprop.h"
#include "BKE_mesh_mapping.h"
#include "BKE_mesh_remap.h"
#include "BKE_multires.h"
#include "BKE_ocean.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "rna_internal.h"

#include "WM_api.h"
#include "WM_types.h"

#include "MOD_nodes.h"

const EnumPropertyItem rna_enum_object_modifier_type_items[] = {
    RNA_ENUM_ITEM_HEADING(N_("Modify"), NULL),
    {eModifierType_DataTransfer,
     "DATA_TRANSFER",
     ICON_MOD_DATA_TRANSFER,
     "Data Transfer",
     "Transfer several types of data (vertex groups, UV maps, vertex colors, custom normals) from "
     "one mesh to another"},
    {eModifierType_MeshCache,
     "MESH_CACHE",
     ICON_MOD_MESHDEFORM,
     "Mesh Cache",
     "Deform the mesh using an external frame-by-frame vertex transform cache"},
    {eModifierType_MeshSequenceCache,
     "MESH_SEQUENCE_CACHE",
     ICON_MOD_MESHDEFORM,
     "Mesh Sequence Cache",
     "Deform the mesh or curve using an external mesh cache in Alembic format"},
    {eModifierType_NormalEdit,
     "NORMAL_EDIT",
     ICON_MOD_NORMALEDIT,
     "Normal Edit",
     "Modify the direction of the surface normals"},
    {eModifierType_WeightedNormal,
     "WEIGHTED_NORMAL",
     ICON_MOD_NORMALEDIT,
     "Weighted Normal",
     "Modify the direction of the surface normals using a weighting method"},
    {eModifierType_UVProject,
     "UV_PROJECT",
     ICON_MOD_UVPROJECT,
     "UV Project",
     "Project the UV map coordinates from the negative Z axis of another object"},
    {eModifierType_UVWarp,
     "UV_WARP",
     ICON_MOD_UVPROJECT,
     "UV Warp",
     "Transform the UV map using the difference between two objects"},
    {eModifierType_WeightVGEdit,
     "VERTEX_WEIGHT_EDIT",
     ICON_MOD_VERTEX_WEIGHT,
     "Vertex Weight Edit",
     "Modify of the weights of a vertex group"},
    {eModifierType_WeightVGMix,
     "VERTEX_WEIGHT_MIX",
     ICON_MOD_VERTEX_WEIGHT,
     "Vertex Weight Mix",
     "Mix the weights of two vertex groups"},
    {eModifierType_WeightVGProximity,
     "VERTEX_WEIGHT_PROXIMITY",
     ICON_MOD_VERTEX_WEIGHT,
     "Vertex Weight Proximity",
     "Set the vertex group weights based on the distance to another target object"},

    RNA_ENUM_ITEM_HEADING(N_("Generate"), NULL),
    {eModifierType_Array,
     "ARRAY",
     ICON_MOD_ARRAY,
     "Array",
     "Create copies of the shape with offsets"},
    {eModifierType_Bevel,
     "BEVEL",
     ICON_MOD_BEVEL,
     "Bevel",
     "Generate sloped corners by adding geometry to the mesh's edges or vertices"},
    {eModifierType_Boolean,
     "BOOLEAN",
     ICON_MOD_BOOLEAN,
     "Boolean",
     "Use another shape to cut, combine or perform a difference operation"},
    {eModifierType_Build,
     "BUILD",
     ICON_MOD_BUILD,
     "Build",
     "Cause the faces of the mesh object to appear or disappear one after the other over time"},
    {eModifierType_Decimate,
     "DECIMATE",
     ICON_MOD_DECIM,
     "Decimate",
     "Reduce the geometry density"},
    {eModifierType_EdgeSplit,
     "EDGE_SPLIT",
     ICON_MOD_EDGESPLIT,
     "Edge Split",
     "Split away joined faces at the edges"},
    {eModifierType_Nodes, "NODES", ICON_GEOMETRY_NODES, "Geometry Nodes", ""},
    {eModifierType_Mask,
     "MASK",
     ICON_MOD_MASK,
     "Mask",
     "Dynamically hide vertices based on a vertex group or armature"},
    {eModifierType_Mirror,
     "MIRROR",
     ICON_MOD_MIRROR,
     "Mirror",
     "Mirror along the local X, Y and/or Z axes, over the object origin"},
    {eModifierType_MeshToVolume,
     "MESH_TO_VOLUME",
     ICON_VOLUME_DATA,
     "Mesh to Volume",
     ""}, /* TODO: Use correct icon. */
    {eModifierType_Multires,
     "MULTIRES",
     ICON_MOD_MULTIRES,
     "Multiresolution",
     "Subdivide the mesh in a way that allows editing the higher subdivision levels"},
    {eModifierType_Remesh,
     "REMESH",
     ICON_MOD_REMESH,
     "Remesh",
     "Generate new mesh topology based on the current shape"},
    {eModifierType_Screw,
     "SCREW",
     ICON_MOD_SCREW,
     "Screw",
     "Lathe around an axis, treating the input mesh as a profile"},
    {eModifierType_Skin,
     "SKIN",
     ICON_MOD_SKIN,
     "Skin",
     "Create a solid shape from vertices and edges, using the vertex radius to define the "
     "thickness"},
    {eModifierType_Solidify, "SOLIDIFY", ICON_MOD_SOLIDIFY, "Solidify", "Make the surface thick"},
    {eModifierType_Subsurf,
     "SUBSURF",
     ICON_MOD_SUBSURF,
     "Subdivision Surface",
     "Split the faces into smaller parts, giving it a smoother appearance"},
    {eModifierType_Triangulate,
     "TRIANGULATE",
     ICON_MOD_TRIANGULATE,
     "Triangulate",
     "Convert all polygons to triangles"},
    {eModifierType_VolumeToMesh,
     "VOLUME_TO_MESH",
     ICON_VOLUME_DATA,
     "Volume to Mesh",
     ""}, /* TODO: Use correct icon. */
    {eModifierType_Weld,
     "WELD",
     ICON_AUTOMERGE_OFF,
     "Weld",
     "Find groups of vertices closer than dist and merge them together"},
    {eModifierType_Wireframe,
     "WIREFRAME",
     ICON_MOD_WIREFRAME,
     "Wireframe",
     "Convert faces into thickened edges"},

    RNA_ENUM_ITEM_HEADING(N_("Deform"), NULL),
    {eModifierType_Armature,
     "ARMATURE",
     ICON_MOD_ARMATURE,
     "Armature",
     "Deform the shape using an armature object"},
    {eModifierType_Cast,
     "CAST",
     ICON_MOD_CAST,
     "Cast",
     "Shift the shape towards a predefined primitive"},
    {eModifierType_Curve, "CURVE", ICON_MOD_CURVE, "Curve", "Bend the mesh using a curve object"},
    {eModifierType_Displace,
     "DISPLACE",
     ICON_MOD_DISPLACE,
     "Displace",
     "Offset vertices based on a texture"},
    {eModifierType_Hook, "HOOK", ICON_HOOK, "Hook", "Deform specific points using another object"},
    {eModifierType_LaplacianDeform,
     "LAPLACIANDEFORM",
     ICON_MOD_MESHDEFORM,
     "Laplacian Deform",
     "Deform based a series of anchor points"},
    {eModifierType_Lattice,
     "LATTICE",
     ICON_MOD_LATTICE,
     "Lattice",
     "Deform using the shape of a lattice object"},
    {eModifierType_MeshDeform,
     "MESH_DEFORM",
     ICON_MOD_MESHDEFORM,
     "Mesh Deform",
     "Deform using a different mesh, which acts as a deformation cage"},
    {eModifierType_Shrinkwrap,
     "SHRINKWRAP",
     ICON_MOD_SHRINKWRAP,
     "Shrinkwrap",
     "Project the shape onto another object"},
    {eModifierType_SimpleDeform,
     "SIMPLE_DEFORM",
     ICON_MOD_SIMPLEDEFORM,
     "Simple Deform",
     "Deform the shape by twisting, bending, tapering or stretching"},
    {eModifierType_Smooth,
     "SMOOTH",
     ICON_MOD_SMOOTH,
     "Smooth",
     "Smooth the mesh by flattening the angles between adjacent faces"},
    {eModifierType_CorrectiveSmooth,
     "CORRECTIVE_SMOOTH",
     ICON_MOD_SMOOTH,
     "Smooth Corrective",
     "Smooth the mesh while still preserving the volume"},
    {eModifierType_LaplacianSmooth,
     "LAPLACIANSMOOTH",
     ICON_MOD_SMOOTH,
     "Smooth Laplacian",
     "Reduce the noise on a mesh surface with minimal changes to its shape"},
    {eModifierType_SurfaceDeform,
     "SURFACE_DEFORM",
     ICON_MOD_MESHDEFORM,
     "Surface Deform",
     "Transfer motion from another mesh"},
    {eModifierType_Warp,
     "WARP",
     ICON_MOD_WARP,
     "Warp",
     "Warp parts of a mesh to a new location in a very flexible way thanks to 2 specified "
     "objects"},
    {eModifierType_Wave,
     "WAVE",
     ICON_MOD_WAVE,
     "Wave",
     "Adds a ripple-like motion to an object's geometry"},
    {eModifierType_VolumeDisplace,
     "VOLUME_DISPLACE",
     ICON_VOLUME_DATA,
     "Volume Displace",
     "Deform volume based on noise or other vector fields"}, /* TODO: Use correct icon. */

    RNA_ENUM_ITEM_HEADING(N_("Physics"), NULL),
    {eModifierType_Cloth, "CLOTH", ICON_MOD_CLOTH, "Cloth", ""},
    {eModifierType_Collision, "COLLISION", ICON_MOD_PHYSICS, "Collision", ""},
    {eModifierType_DynamicPaint, "DYNAMIC_PAINT", ICON_MOD_DYNAMICPAINT, "Dynamic Paint", ""},
    {eModifierType_Explode,
     "EXPLODE",
     ICON_MOD_EXPLODE,
     "Explode",
     "Break apart the mesh faces and let them follow particles"},
    {eModifierType_Fluid, "FLUID", ICON_MOD_FLUIDSIM, "Fluid", ""},
    {eModifierType_Ocean, "OCEAN", ICON_MOD_OCEAN, "Ocean", "Generate a moving ocean surface"},
    {eModifierType_ParticleInstance,
     "PARTICLE_INSTANCE",
     ICON_MOD_PARTICLE_INSTANCE,
     "Particle Instance",
     ""},
    {eModifierType_ParticleSystem,
     "PARTICLE_SYSTEM",
     ICON_MOD_PARTICLES,
     "Particle System",
     "Spawn particles from the shape"},
    {eModifierType_Softbody, "SOFT_BODY", ICON_MOD_SOFT, "Soft Body", ""},
    {eModifierType_Surface, "SURFACE", ICON_MODIFIER, "Surface", ""},
    {0, NULL, 0, NULL, NULL},
};

const EnumPropertyItem rna_enum_modifier_triangulate_quad_method_items[] = {
    {MOD_TRIANGULATE_QUAD_BEAUTY,
     "BEAUTY",
     0,
     "Beauty",
     "Split the quads in nice triangles, slower method"},
    {MOD_TRIANGULATE_QUAD_FIXED,
     "FIXED",
     0,
     "Fixed",
     "Split the quads on the first and third vertices"},
    {MOD_TRIANGULATE_QUAD_ALTERNATE,
     "FIXED_ALTERNATE",
     0,
     "Fixed Alternate",
     "Split the quads on the 2nd and 4th vertices"},
    {MOD_TRIANGULATE_QUAD_SHORTEDGE,
     "SHORTEST_DIAGONAL",
     0,
     "Shortest Diagonal",
     "Split the quads along their shortest diagonal"},
    {MOD_TRIANGULATE_QUAD_LONGEDGE,
     "LONGEST_DIAGONAL",
     0,
     "Longest Diagonal",
     "Split the quads along their longest diagonal"},
    {0, NULL, 0, NULL, NULL},
};

const EnumPropertyItem rna_enum_modifier_triangulate_ngon_method_items[] = {
    {MOD_TRIANGULATE_NGON_BEAUTY,
     "BEAUTY",
     0,
     "Beauty",
     "Arrange the new triangles evenly (slow)"},
    {MOD_TRIANGULATE_NGON_EARCLIP,
     "CLIP",
     0,
     "Clip",
     "Split the polygons with an ear clipping algorithm"},
    {0, NULL, 0, NULL, NULL},
};

const EnumPropertyItem rna_enum_modifier_shrinkwrap_mode_items[] = {
    {MOD_SHRINKWRAP_ON_SURFACE,
     "ON_SURFACE",
     0,
     "On Surface",
     "The point is constrained to the surface of the target object, "
     "with distance offset towards the original point location"},
    {MOD_SHRINKWRAP_INSIDE,
     "INSIDE",
     0,
     "Inside",
     "The point is constrained to be inside the target object"},
    {MOD_SHRINKWRAP_OUTSIDE,
     "OUTSIDE",
     0,
     "Outside",
     "The point is constrained to be outside the target object"},
    {MOD_SHRINKWRAP_OUTSIDE_SURFACE,
     "OUTSIDE_SURFACE",
     0,
     "Outside Surface",
     "The point is constrained to the surface of the target object, "
     "with distance offset always to the outside, towards or away from the original location"},
    {MOD_SHRINKWRAP_ABOVE_SURFACE,
     "ABOVE_SURFACE",
     0,
     "Above Surface",
     "The point is constrained to the surface of the target object, "
     "with distance offset applied exactly along the target normal"},
    {0, NULL, 0, NULL, NULL},
};

const EnumPropertyItem rna_enum_shrinkwrap_type_items[] = {
    {MOD_SHRINKWRAP_NEAREST_SURFACE,
     "NEAREST_SURFACEPOINT",
     0,
     "Nearest Surface Point",
     "Shrink the mesh to the nearest target surface"},
    {MOD_SHRINKWRAP_PROJECT,
     "PROJECT",
     0,
     "Project",
     "Shrink the mesh to the nearest target surface along a given axis"},
    {MOD_SHRINKWRAP_NEAREST_VERTEX,
     "NEAREST_VERTEX",
     0,
     "Nearest Vertex",
     "Shrink the mesh to the nearest target vertex"},
    {MOD_SHRINKWRAP_TARGET_PROJECT,
     "TARGET_PROJECT",
     0,
     "Target Normal Project",
     "Shrink the mesh to the nearest target surface "
     "along the interpolated vertex normals of the target"},
    {0, NULL, 0, NULL, NULL},
};

const EnumPropertyItem rna_enum_shrinkwrap_face_cull_items[] = {
    {0, "OFF", 0, "Off", "No culling"},
    {MOD_SHRINKWRAP_CULL_TARGET_FRONTFACE,
     "FRONT",
     0,
     "Front",
     "No projection when in front of the face"},
    {MOD_SHRINKWRAP_CULL_TARGET_BACKFACE, "BACK", 0, "Back", "No projection when behind the face"},
    {0, NULL, 0, NULL, NULL},
};

#ifndef RNA_RUNTIME
/* use eWarp_Falloff_*** & eHook_Falloff_***, they're in sync */
static const EnumPropertyItem modifier_warp_falloff_items[] = {
    {eWarp_Falloff_None, "NONE", 0, "No Falloff", ""},
    {eWarp_Falloff_Curve, "CURVE", 0, "Curve", ""},
    {eWarp_Falloff_Smooth, "SMOOTH", ICON_SMOOTHCURVE, "Smooth", ""},
    {eWarp_Falloff_Sphere, "SPHERE", ICON_SPHERECURVE, "Sphere", ""},
    {eWarp_Falloff_Root, "ROOT", ICON_ROOTCURVE, "Root", ""},
    {eWarp_Falloff_InvSquare, "INVERSE_SQUARE", ICON_ROOTCURVE, "Inverse Square", ""},
    {eWarp_Falloff_Sharp, "SHARP", ICON_SHARPCURVE, "Sharp", ""},
    {eWarp_Falloff_Linear, "LINEAR", ICON_LINCURVE, "Linear", ""},
    {eWarp_Falloff_Const, "CONSTANT", ICON_NOCURVE, "Constant", ""},
    {0, NULL, 0, NULL, NULL},
};
#endif

/* ***** Data Transfer ***** */

const EnumPropertyItem rna_enum_dt_method_vertex_items[] = {
    {MREMAP_MODE_TOPOLOGY, "TOPOLOGY", 0, "Topology", "Copy from identical topology meshes"},
    {MREMAP_MODE_VERT_NEAREST, "NEAREST", 0, "Nearest Vertex", "Copy from closest vertex"},
    {MREMAP_MODE_VERT_EDGE_NEAREST,
     "EDGE_NEAREST",
     0,
     "Nearest Edge Vertex",
     "Copy from closest vertex of closest edge"},
    {MREMAP_MODE_VERT_EDGEINTERP_NEAREST,
     "EDGEINTERP_NEAREST",
     0,
     "Nearest Edge Interpolated",
     "Copy from interpolated values of vertices from closest point on closest edge"},
    {MREMAP_MODE_VERT_POLY_NEAREST,
     "POLY_NEAREST",
     0,
     "Nearest Face Vertex",
     "Copy from closest vertex of closest face"},
    {MREMAP_MODE_VERT_POLYINTERP_NEAREST,
     "POLYINTERP_NEAREST",
     0,
     "Nearest Face Interpolated",
     "Copy from interpolated values of vertices from closest point on closest face"},
    {MREMAP_MODE_VERT_POLYINTERP_VNORPROJ,
     "POLYINTERP_VNORPROJ",
     0,
     "Projected Face Interpolated",
     "Copy from interpolated values of vertices from point on closest face hit by "
     "normal-projection"},
    {0, NULL, 0, NULL, NULL},
};

const EnumPropertyItem rna_enum_dt_method_edge_items[] = {
    {MREMAP_MODE_TOPOLOGY, "TOPOLOGY", 0, "Topology", "Copy from identical topology meshes"},
    {MREMAP_MODE_EDGE_VERT_NEAREST,
     "VERT_NEAREST",
     0,
     "Nearest Vertices",
     "Copy from most similar edge (edge which vertices are the closest of destination edge's "
     "ones)"},
    {MREMAP_MODE_EDGE_NEAREST,
     "NEAREST",
     0,
     "Nearest Edge",
     "Copy from closest edge (using midpoints)"},
    {MREMAP_MODE_EDGE_POLY_NEAREST,
     "POLY_NEAREST",
     0,
     "Nearest Face Edge",
     "Copy from closest edge of closest face (using midpoints)"},
    {MREMAP_MODE_EDGE_EDGEINTERP_VNORPROJ,
     "EDGEINTERP_VNORPROJ",
     0,
     "Projected Edge Interpolated",
     "Interpolate all source edges hit by the projection of destination one along its own normal "
     "(from vertices)"},
    {0, NULL, 0, NULL, NULL},
};

const EnumPropertyItem rna_enum_dt_method_loop_items[] = {
    {MREMAP_MODE_TOPOLOGY, "TOPOLOGY", 0, "Topology", "Copy from identical topology meshes"},
    {MREMAP_MODE_LOOP_NEAREST_LOOPNOR,
     "NEAREST_NORMAL",
     0,
     "Nearest Corner and Best Matching Normal",
     "Copy from nearest corner which has the best matching normal"},
    {MREMAP_MODE_LOOP_NEAREST_POLYNOR,
     "NEAREST_POLYNOR",
     0,
     "Nearest Corner and Best Matching Face Normal",
     "Copy from nearest corner which has the face with the best matching normal to destination "
     "corner's face one"},
    {MREMAP_MODE_LOOP_POLY_NEAREST,
     "NEAREST_POLY",
     0,
     "Nearest Corner of Nearest Face",
     "Copy from nearest corner of nearest polygon"},
    {MREMAP_MODE_LOOP_POLYINTERP_NEAREST,
     "POLYINTERP_NEAREST",
     0,
     "Nearest Face Interpolated",
     "Copy from interpolated corners of the nearest source polygon"},
    {MREMAP_MODE_LOOP_POLYINTERP_LNORPROJ,
     "POLYINTERP_LNORPROJ",
     0,
     "Projected Face Interpolated",
     "Copy from interpolated corners of the source polygon hit by corner normal projection"},
    {0, NULL, 0, NULL, NULL},
};

const EnumPropertyItem rna_enum_dt_method_poly_items[] = {
    {MREMAP_MODE_TOPOLOGY, "TOPOLOGY", 0, "Topology", "Copy from identical topology meshes"},
    {MREMAP_MODE_POLY_NEAREST,
     "NEAREST",
     0,
     "Nearest Face",
     "Copy from nearest polygon (using center points)"},
    {MREMAP_MODE_POLY_NOR,
     "NORMAL",
     0,
     "Best Normal-Matching",
     "Copy from source polygon which normal is the closest to destination one"},
    {MREMAP_MODE_POLY_POLYINTERP_PNORPROJ,
     "POLYINTERP_PNORPROJ",
     0,
     "Projected Face Interpolated",
     "Interpolate all source polygons intersected by the projection of destination one along its "
     "own normal"},
    {0, NULL, 0, NULL, NULL},
};

const EnumPropertyItem rna_enum_dt_mix_mode_items[] = {
    {CDT_MIX_TRANSFER, "REPLACE", 0, "Replace", "Overwrite all elements' data"},
    {CDT_MIX_REPLACE_ABOVE_THRESHOLD,
     "ABOVE_THRESHOLD",
     0,
     "Above Threshold",
     "Only replace destination elements where data is above given threshold (exact behavior "
     "depends on data type)"},
    {CDT_MIX_REPLACE_BELOW_THRESHOLD,
     "BELOW_THRESHOLD",
     0,
     "Below Threshold",
     "Only replace destination elements where data is below given threshold (exact behavior "
     "depends on data type)"},
    {CDT_MIX_MIX,
     "MIX",
     0,
     "Mix",
     "Mix source value into destination one, using given threshold as factor"},
    {CDT_MIX_ADD,
     "ADD",
     0,
     "Add",
     "Add source value to destination one, using given threshold as factor"},
    {CDT_MIX_SUB,
     "SUB",
     0,
     "Subtract",
     "Subtract source value to destination one, using given threshold as factor"},
    {CDT_MIX_MUL,
     "MUL",
     0,
     "Multiply",
     "Multiply source value to destination one, using given threshold as factor"},
    /* Etc. */
    {0, NULL, 0, NULL, NULL},
};

const EnumPropertyItem rna_enum_dt_layers_select_src_items[] = {
    {DT_LAYERS_ACTIVE_SRC, "ACTIVE", 0, "Active Layer", "Only transfer active data layer"},
    {DT_LAYERS_ALL_SRC, "ALL", 0, "All Layers", "Transfer all data layers"},
    {DT_LAYERS_VGROUP_SRC_BONE_SELECT,
     "BONE_SELECT",
     0,
     "Selected Pose Bones",
     "Transfer all vertex groups used by selected pose bones"},
    {DT_LAYERS_VGROUP_SRC_BONE_DEFORM,
     "BONE_DEFORM",
     0,
     "Deform Pose Bones",
     "Transfer all vertex groups used by deform bones"},
    {0, NULL, 0, NULL, NULL},
};

const EnumPropertyItem rna_enum_dt_layers_select_dst_items[] = {
    {DT_LAYERS_ACTIVE_DST, "ACTIVE", 0, "Active Layer", "Affect active data layer of all targets"},
    {DT_LAYERS_NAME_DST, "NAME", 0, "By Name", "Match target data layers to affect by name"},
    {DT_LAYERS_INDEX_DST,
     "INDEX",
     0,
     "By Order",
     "Match target data layers to affect by order (indices)"},
    {0, NULL, 0, NULL, NULL},
};

const EnumPropertyItem rna_enum_axis_xy_items[] = {
    {0, "X", 0, "X", ""},
    {1, "Y", 0, "Y", ""},
    {0, NULL, 0, NULL, NULL},
};

const EnumPropertyItem rna_enum_axis_xyz_items[] = {
    {0, "X", 0, "X", ""},
    {1, "Y", 0, "Y", ""},
    {2, "Z", 0, "Z", ""},
    {0, NULL, 0, NULL, NULL},
};

const EnumPropertyItem rna_enum_axis_flag_xyz_items[] = {
    {(1 << 0), "X", 0, "X", ""},
    {(1 << 1), "Y", 0, "Y", ""},
    {(1 << 2), "Z", 0, "Z", ""},
    {0, NULL, 0, NULL, NULL},
};

const EnumPropertyItem rna_enum_subdivision_uv_smooth_items[] = {
    {SUBSURF_UV_SMOOTH_NONE, "NONE", 0, "None", "UVs are not smoothed, boundaries are kept sharp"},
    {SUBSURF_UV_SMOOTH_PRESERVE_CORNERS,
     "PRESERVE_CORNERS",
     0,
     "Keep Corners",
     "UVs are smoothed, corners on discontinuous boundary are kept sharp"},
    {SUBSURF_UV_SMOOTH_PRESERVE_CORNERS_AND_JUNCTIONS,
     "PRESERVE_CORNERS_AND_JUNCTIONS",
     0,
     "Keep Corners, Junctions",
     "UVs are smoothed, corners on discontinuous boundary and "
     "junctions of 3 or more regions are kept sharp"},
    {SUBSURF_UV_SMOOTH_PRESERVE_CORNERS_JUNCTIONS_AND_CONCAVE,
     "PRESERVE_CORNERS_JUNCTIONS_AND_CONCAVE",
     0,
     "Keep Corners, Junctions, Concave",
     "UVs are smoothed, corners on discontinuous boundary, "
     "junctions of 3 or more regions and darts and concave corners are kept sharp"},
    {SUBSURF_UV_SMOOTH_PRESERVE_BOUNDARIES,
     "PRESERVE_BOUNDARIES",
     0,
     "Keep Boundaries",
     "UVs are smoothed, boundaries are kept sharp"},
    {SUBSURF_UV_SMOOTH_ALL, "SMOOTH_ALL", 0, "All", "UVs and boundaries are smoothed"},
    {0, NULL, 0, NULL, NULL},
};

const EnumPropertyItem rna_enum_subdivision_boundary_smooth_items[] = {
    {SUBSURF_BOUNDARY_SMOOTH_PRESERVE_CORNERS,
     "PRESERVE_CORNERS",
     0,
     "Keep Corners",
     "Smooth boundaries, but corners are kept sharp"},
    {SUBSURF_BOUNDARY_SMOOTH_ALL, "ALL", 0, "All", "Smooth boundaries, including corners"},
    {0, NULL, 0, NULL, NULL},
};

#ifdef RNA_RUNTIME
#  include "DNA_curve_types.h"
#  include "DNA_fluid_types.h"
#  include "DNA_particle_types.h"

#  include "BKE_cachefile.h"
#  include "BKE_context.h"
#  include "BKE_deform.h"
#  include "BKE_mesh_runtime.h"
#  include "BKE_modifier.h"
#  include "BKE_object.h"
#  include "BKE_particle.h"

#  include "BLI_sort_utils.h"

#  include "DEG_depsgraph.h"
#  include "DEG_depsgraph_build.h"
#  include "DEG_depsgraph_query.h"

#  ifdef WITH_ALEMBIC
#    include "ABC_alembic.h"
#  endif

static void rna_UVProject_projectors_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  UVProjectModifierData *uvp = (UVProjectModifierData *)ptr->data;
  rna_iterator_array_begin(
      iter, (void *)uvp->projectors, sizeof(Object *), uvp->projectors_num, 0, NULL);
}

static StructRNA *rna_Modifier_refine(struct PointerRNA *ptr)
{
  ModifierData *md = (ModifierData *)ptr->data;
  const ModifierTypeInfo *modifier_type = BKE_modifier_get_info(md->type);
  if (modifier_type != NULL) {
    return modifier_type->srna;
  }
  return &RNA_Modifier;
}

static void rna_Modifier_name_set(PointerRNA *ptr, const char *value)
{
  ModifierData *md = ptr->data;
  char oldname[sizeof(md->name)];

  /* make a copy of the old name first */
  STRNCPY(oldname, md->name);

  /* copy the new name into the name slot */
  STRNCPY_UTF8(md->name, value);

  /* make sure the name is truly unique */
  if (ptr->owner_id) {
    Object *ob = (Object *)ptr->owner_id;
    BKE_modifier_unique_name(&ob->modifiers, md);
  }

  /* fix all the animation data which may link to this */
  BKE_animdata_fix_paths_rename_all(NULL, "modifiers", oldname, md->name);
}

static void rna_Modifier_name_update(Main *bmain, Scene *UNUSED(scene), PointerRNA *UNUSED(ptr))
{
  DEG_relations_tag_update(bmain);
}

static char *rna_Modifier_path(const PointerRNA *ptr)
{
  const ModifierData *md = ptr->data;
  char name_esc[sizeof(md->name) * 2];

  BLI_str_escape(name_esc, md->name, sizeof(name_esc));
  return BLI_sprintfN("modifiers[\"%s\"]", name_esc);
}

static void rna_Modifier_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
  DEG_id_tag_update(ptr->owner_id, ID_RECALC_GEOMETRY);
  WM_main_add_notifier(NC_OBJECT | ND_MODIFIER, ptr->owner_id);
}

static void rna_Modifier_dependency_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  rna_Modifier_update(bmain, scene, ptr);
  DEG_relations_tag_update(bmain);
}

static void rna_Modifier_is_active_set(PointerRNA *ptr, bool value)
{
  ModifierData *md = ptr->data;

  if (value) {
    /* Disable the active flag of all other modifiers. */
    for (ModifierData *prev_md = md->prev; prev_md != NULL; prev_md = prev_md->prev) {
      prev_md->flag &= ~eModifierFlag_Active;
    }
    for (ModifierData *next_md = md->next; next_md != NULL; next_md = next_md->next) {
      next_md->flag &= ~eModifierFlag_Active;
    }

    md->flag |= eModifierFlag_Active;
    WM_main_add_notifier(NC_OBJECT | ND_MODIFIER, ptr->owner_id);
  }
}

/* Vertex Groups */

#  define RNA_MOD_VGROUP_NAME_SET(_type, _prop) \
    static void rna_##_type##Modifier_##_prop##_set(PointerRNA *ptr, const char *value) \
    { \
      _type##ModifierData *tmd = (_type##ModifierData *)ptr->data; \
      rna_object_vgroup_name_set(ptr, value, tmd->_prop, sizeof(tmd->_prop)); \
    }

RNA_MOD_VGROUP_NAME_SET(Armature, defgrp_name);
RNA_MOD_VGROUP_NAME_SET(Bevel, defgrp_name);
RNA_MOD_VGROUP_NAME_SET(Cast, defgrp_name);
RNA_MOD_VGROUP_NAME_SET(Curve, name);
RNA_MOD_VGROUP_NAME_SET(DataTransfer, defgrp_name);
RNA_MOD_VGROUP_NAME_SET(Decimate, defgrp_name);
RNA_MOD_VGROUP_NAME_SET(CorrectiveSmooth, defgrp_name);
RNA_MOD_VGROUP_NAME_SET(Displace, defgrp_name);
RNA_MOD_VGROUP_NAME_SET(Hook, name);
RNA_MOD_VGROUP_NAME_SET(LaplacianDeform, anchor_grp_name);
RNA_MOD_VGROUP_NAME_SET(LaplacianSmooth, defgrp_name);
RNA_MOD_VGROUP_NAME_SET(Lattice, name);
RNA_MOD_VGROUP_NAME_SET(Mask, vgroup);
RNA_MOD_VGROUP_NAME_SET(MeshCache, defgrp_name);
RNA_MOD_VGROUP_NAME_SET(MeshDeform, defgrp_name);
RNA_MOD_VGROUP_NAME_SET(NormalEdit, defgrp_name);
RNA_MOD_VGROUP_NAME_SET(Shrinkwrap, vgroup_name);
RNA_MOD_VGROUP_NAME_SET(SimpleDeform, vgroup_name);
RNA_MOD_VGROUP_NAME_SET(Smooth, defgrp_name);
RNA_MOD_VGROUP_NAME_SET(Solidify, defgrp_name);
RNA_MOD_VGROUP_NAME_SET(Solidify, shell_defgrp_name);
RNA_MOD_VGROUP_NAME_SET(Solidify, rim_defgrp_name);
RNA_MOD_VGROUP_NAME_SET(SurfaceDeform, defgrp_name);
RNA_MOD_VGROUP_NAME_SET(UVWarp, vgroup_name);
RNA_MOD_VGROUP_NAME_SET(Warp, defgrp_name);
RNA_MOD_VGROUP_NAME_SET(Wave, defgrp_name);
RNA_MOD_VGROUP_NAME_SET(WeightVGEdit, defgrp_name);
RNA_MOD_VGROUP_NAME_SET(WeightVGEdit, mask_defgrp_name);
RNA_MOD_VGROUP_NAME_SET(WeightVGMix, defgrp_name_a);
RNA_MOD_VGROUP_NAME_SET(WeightVGMix, defgrp_name_b);
RNA_MOD_VGROUP_NAME_SET(WeightVGMix, mask_defgrp_name);
RNA_MOD_VGROUP_NAME_SET(WeightVGProximity, defgrp_name);
RNA_MOD_VGROUP_NAME_SET(WeightVGProximity, mask_defgrp_name);
RNA_MOD_VGROUP_NAME_SET(WeightedNormal, defgrp_name);
RNA_MOD_VGROUP_NAME_SET(Weld, defgrp_name);
RNA_MOD_VGROUP_NAME_SET(Wireframe, defgrp_name);

static void rna_ExplodeModifier_vgroup_get(PointerRNA *ptr, char *value)
{
  ExplodeModifierData *emd = (ExplodeModifierData *)ptr->data;
  rna_object_vgroup_name_index_get(ptr, value, emd->vgroup);
}

static int rna_ExplodeModifier_vgroup_length(PointerRNA *ptr)
{
  ExplodeModifierData *emd = (ExplodeModifierData *)ptr->data;
  return rna_object_vgroup_name_index_length(ptr, emd->vgroup);
}

static void rna_ExplodeModifier_vgroup_set(PointerRNA *ptr, const char *value)
{
  ExplodeModifierData *emd = (ExplodeModifierData *)ptr->data;
  rna_object_vgroup_name_index_set(ptr, value, &emd->vgroup);
}

#  undef RNA_MOD_VGROUP_NAME_SET

/* UV layers */

#  define RNA_MOD_UVLAYER_NAME_SET(_type, _prop) \
    static void rna_##_type##Modifier_##_prop##_set(PointerRNA *ptr, const char *value) \
    { \
      _type##ModifierData *tmd = (_type##ModifierData *)ptr->data; \
      rna_object_uvlayer_name_set(ptr, value, tmd->_prop, sizeof(tmd->_prop)); \
    }

RNA_MOD_UVLAYER_NAME_SET(MappingInfo, uvlayer_name);
RNA_MOD_UVLAYER_NAME_SET(UVProject, uvlayer_name);
RNA_MOD_UVLAYER_NAME_SET(UVWarp, uvlayer_name);
RNA_MOD_UVLAYER_NAME_SET(WeightVGEdit, mask_tex_uvlayer_name);
RNA_MOD_UVLAYER_NAME_SET(WeightVGMix, mask_tex_uvlayer_name);
RNA_MOD_UVLAYER_NAME_SET(WeightVGProximity, mask_tex_uvlayer_name);

#  undef RNA_MOD_UVLAYER_NAME_SET

/* Objects */

static void modifier_object_set(Object *self, Object **ob_p, int type, PointerRNA value)
{
  Object *ob = value.data;

  if (!self || ob != self) {
    if (!ob || type == OB_EMPTY || ob->type == type) {
      id_lib_extern((ID *)ob);
      *ob_p = ob;
    }
  }
}

#  define RNA_MOD_OBJECT_SET(_type, _prop, _obtype) \
    static void rna_##_type##Modifier_##_prop##_set( \
        PointerRNA *ptr, PointerRNA value, struct ReportList *UNUSED(reports)) \
    { \
      _type##ModifierData *tmd = (_type##ModifierData *)ptr->data; \
      modifier_object_set((Object *)ptr->owner_id, &tmd->_prop, _obtype, value); \
    }

RNA_MOD_OBJECT_SET(Armature, object, OB_ARMATURE);
RNA_MOD_OBJECT_SET(Array, start_cap, OB_MESH);
RNA_MOD_OBJECT_SET(Array, end_cap, OB_MESH);
RNA_MOD_OBJECT_SET(Array, curve_ob, OB_CURVES_LEGACY);
RNA_MOD_OBJECT_SET(Boolean, object, OB_MESH);
RNA_MOD_OBJECT_SET(Cast, object, OB_EMPTY);
RNA_MOD_OBJECT_SET(Curve, object, OB_CURVES_LEGACY);
RNA_MOD_OBJECT_SET(DataTransfer, ob_source, OB_MESH);
RNA_MOD_OBJECT_SET(Lattice, object, OB_LATTICE);
RNA_MOD_OBJECT_SET(Mask, ob_arm, OB_ARMATURE);
RNA_MOD_OBJECT_SET(MeshDeform, object, OB_MESH);
RNA_MOD_OBJECT_SET(NormalEdit, target, OB_EMPTY);
RNA_MOD_OBJECT_SET(Shrinkwrap, target, OB_MESH);
RNA_MOD_OBJECT_SET(Shrinkwrap, auxTarget, OB_MESH);
RNA_MOD_OBJECT_SET(SurfaceDeform, target, OB_MESH);

static void rna_HookModifier_object_set(PointerRNA *ptr,
                                        PointerRNA value,
                                        struct ReportList *UNUSED(reports))
{
  Object *owner = (Object *)ptr->owner_id;
  HookModifierData *hmd = ptr->data;
  Object *ob = (Object *)value.data;

  hmd->object = ob;
  id_lib_extern((ID *)ob);
  BKE_object_modifier_hook_reset(owner, hmd);
}

static bool rna_HookModifier_object_override_apply(Main *bmain,
                                                   PointerRNA *ptr_dst,
                                                   PointerRNA *ptr_src,
                                                   PointerRNA *ptr_storage,
                                                   PropertyRNA *prop_dst,
                                                   PropertyRNA *prop_src,
                                                   PropertyRNA *UNUSED(prop_storage),
                                                   const int len_dst,
                                                   const int len_src,
                                                   const int len_storage,
                                                   PointerRNA *UNUSED(ptr_item_dst),
                                                   PointerRNA *UNUSED(ptr_item_src),
                                                   PointerRNA *UNUSED(ptr_item_storage),
                                                   IDOverrideLibraryPropertyOperation *opop)
{
  BLI_assert(len_dst == len_src && (!ptr_storage || len_dst == len_storage) && len_dst == 0);
  BLI_assert(opop->operation == LIBOVERRIDE_OP_REPLACE &&
             "Unsupported RNA override operation on Hook modifier target object pointer");
  UNUSED_VARS_NDEBUG(ptr_storage, len_dst, len_src, len_storage, opop);

  /* We need a special handling here because setting hook target resets invert parent matrix,
   * which is evil in our case. */
  HookModifierData *hmd = ptr_dst->data;
  Object *owner = (Object *)ptr_dst->owner_id;
  Object *target_dst = RNA_property_pointer_get(ptr_dst, prop_dst).data;
  Object *target_src = RNA_property_pointer_get(ptr_src, prop_src).data;

  BLI_assert(target_dst == hmd->object);

  if (target_src == target_dst) {
    return false;
  }

  hmd->object = target_src;
  if (target_src == NULL) {
    /* The only case where we do want default behavior (with matrix reset). */
    BKE_object_modifier_hook_reset(owner, hmd);
  }
  RNA_property_update_main(bmain, NULL, ptr_dst, prop_dst);
  return true;
}

static void rna_HookModifier_subtarget_set(PointerRNA *ptr, const char *value)
{
  Object *owner = (Object *)ptr->owner_id;
  HookModifierData *hmd = ptr->data;

  STRNCPY(hmd->subtarget, value);
  BKE_object_modifier_hook_reset(owner, hmd);
}

static int rna_HookModifier_vertex_indices_get_length(const PointerRNA *ptr,
                                                      int length[RNA_MAX_ARRAY_DIMENSION])
{
  const HookModifierData *hmd = ptr->data;
  int indexar_num = hmd->indexar ? hmd->indexar_num : 0;
  return (length[0] = indexar_num);
}

static void rna_HookModifier_vertex_indices_get(PointerRNA *ptr, int *values)
{
  HookModifierData *hmd = ptr->data;
  if (hmd->indexar != NULL) {
    memcpy(values, hmd->indexar, sizeof(int) * hmd->indexar_num);
  }
}

static void rna_HookModifier_vertex_indices_set(HookModifierData *hmd,
                                                ReportList *reports,
                                                int indices_len,
                                                int *indices)
{
  if (indices_len == 0) {
    MEM_SAFE_FREE(hmd->indexar);
    hmd->indexar_num = 0;
  }
  else {
    /* Reject negative indices. */
    for (int i = 0; i < indices_len; i++) {
      if (indices[i] < 0) {
        BKE_reportf(reports, RPT_ERROR, "Negative vertex index in vertex_indices_set");
        return;
      }
    }

    /* Copy and sort the index array. */
    size_t size = sizeof(int) * indices_len;
    int *buffer = MEM_mallocN(size, "hook indexar");
    memcpy(buffer, indices, size);

    qsort(buffer, indices_len, sizeof(int), BLI_sortutil_cmp_int);

    /* Reject duplicate indices. */
    for (int i = 1; i < indices_len; i++) {
      if (buffer[i] == buffer[i - 1]) {
        BKE_reportf(reports, RPT_ERROR, "Duplicate index %d in vertex_indices_set", buffer[i]);
        MEM_freeN(buffer);
        return;
      }
    }

    /* Success - save the new array. */
    MEM_SAFE_FREE(hmd->indexar);
    hmd->indexar = buffer;
    hmd->indexar_num = indices_len;
  }
}

static PointerRNA rna_UVProjector_object_get(PointerRNA *ptr)
{
  Object **ob = (Object **)ptr->data;
  return rna_pointer_inherit_refine(ptr, &RNA_Object, *ob);
}

static void rna_UVProjector_object_set(PointerRNA *ptr,
                                       PointerRNA value,
                                       struct ReportList *UNUSED(reports))
{
  Object **ob_p = (Object **)ptr->data;
  Object *ob = (Object *)value.data;
  id_lib_extern((ID *)ob);
  *ob_p = ob;
}

#  undef RNA_MOD_OBJECT_SET

/* Other rna callbacks */

static void rna_fluid_set_type(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  FluidModifierData *fmd = (FluidModifierData *)ptr->data;
  Object *ob = (Object *)ptr->owner_id;

  /* nothing changed */
  if ((fmd->type & MOD_FLUID_TYPE_DOMAIN) && fmd->domain) {
    return;
  }

#  ifdef WITH_FLUID
  BKE_fluid_modifier_free(fmd);             /* XXX TODO: completely free all 3 pointers */
  BKE_fluid_modifier_create_type_data(fmd); /* create regarding of selected type */
#  endif

  switch (fmd->type) {
    case MOD_FLUID_TYPE_DOMAIN:
      ob->dt = OB_WIRE;
      break;
    case MOD_FLUID_TYPE_FLOW:
    case MOD_FLUID_TYPE_EFFEC:
    case 0:
    default:
      break;
  }

  /* update dependency since a domain - other type switch could have happened */
  rna_Modifier_dependency_update(bmain, scene, ptr);
}

static void rna_MultiresModifier_level_range(
    PointerRNA *ptr, int *min, int *max, int *UNUSED(softmin), int *UNUSED(softmax))
{
  MultiresModifierData *mmd = (MultiresModifierData *)ptr->data;

  *min = 0;
  *max = max_ii(0, mmd->totlvl); /* intentionally _not_ -1 */
}

static bool rna_MultiresModifier_external_get(PointerRNA *ptr)
{
  Object *ob = (Object *)ptr->owner_id;
  Mesh *me = ob->data;

  return CustomData_external_test(&me->ldata, CD_MDISPS);
}

static void rna_MultiresModifier_filepath_get(PointerRNA *ptr, char *value)
{
  Object *ob = (Object *)ptr->owner_id;
  CustomDataExternal *external = ((Mesh *)ob->data)->ldata.external;

  strcpy(value, (external) ? external->filepath : "");
}

static void rna_MultiresModifier_filepath_set(PointerRNA *ptr, const char *value)
{
  Object *ob = (Object *)ptr->owner_id;
  CustomDataExternal *external = ((Mesh *)ob->data)->ldata.external;

  if (external && !STREQ(external->filepath, value)) {
    STRNCPY(external->filepath, value);
    multires_force_external_reload(ob);
  }
}

static int rna_MultiresModifier_filepath_length(PointerRNA *ptr)
{
  Object *ob = (Object *)ptr->owner_id;
  CustomDataExternal *external = ((Mesh *)ob->data)->ldata.external;

  return strlen((external) ? external->filepath : "");
}

static int rna_ShrinkwrapModifier_face_cull_get(PointerRNA *ptr)
{
  ShrinkwrapModifierData *swm = (ShrinkwrapModifierData *)ptr->data;
  return swm->shrinkOpts & MOD_SHRINKWRAP_CULL_TARGET_MASK;
}

static void rna_ShrinkwrapModifier_face_cull_set(struct PointerRNA *ptr, int value)
{
  ShrinkwrapModifierData *swm = (ShrinkwrapModifierData *)ptr->data;
  swm->shrinkOpts = (swm->shrinkOpts & ~MOD_SHRINKWRAP_CULL_TARGET_MASK) | value;
}

static bool rna_MeshDeformModifier_is_bound_get(PointerRNA *ptr)
{
  return (((MeshDeformModifierData *)ptr->data)->bindcagecos != NULL);
}

static PointerRNA rna_SoftBodyModifier_settings_get(PointerRNA *ptr)
{
  Object *ob = (Object *)ptr->owner_id;
  return rna_pointer_inherit_refine(ptr, &RNA_SoftBodySettings, ob->soft);
}

static PointerRNA rna_SoftBodyModifier_point_cache_get(PointerRNA *ptr)
{
  Object *ob = (Object *)ptr->owner_id;
  return rna_pointer_inherit_refine(ptr, &RNA_PointCache, ob->soft->shared->pointcache);
}

static PointerRNA rna_CollisionModifier_settings_get(PointerRNA *ptr)
{
  Object *ob = (Object *)ptr->owner_id;
  return rna_pointer_inherit_refine(ptr, &RNA_CollisionSettings, ob->pd);
}

/* Special update function for setting the number of segments of the modifier that also resamples
 * the segments in the custom profile. */
static void rna_BevelModifier_update_segments(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  BevelModifierData *bmd = (BevelModifierData *)ptr->data;
  if (RNA_enum_get(ptr, "profile_type") == MOD_BEVEL_PROFILE_CUSTOM) {
    short segments = (short)RNA_int_get(ptr, "segments");
    BKE_curveprofile_init(bmd->custom_profile, segments);
  }
  rna_Modifier_update(bmain, scene, ptr);
}

static void rna_UVProjectModifier_num_projectors_set(PointerRNA *ptr, int value)
{
  UVProjectModifierData *md = (UVProjectModifierData *)ptr->data;
  int a;

  md->projectors_num = CLAMPIS(value, 1, MOD_UVPROJECT_MAXPROJECTORS);
  for (a = md->projectors_num; a < MOD_UVPROJECT_MAXPROJECTORS; a++) {
    md->projectors[a] = NULL;
  }
}

static void rna_OceanModifier_init_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  OceanModifierData *omd = (OceanModifierData *)ptr->data;

  BKE_ocean_free_modifier_cache(omd);
  rna_Modifier_update(bmain, scene, ptr);
}

static void rna_OceanModifier_ocean_chop_set(PointerRNA *ptr, float value)
{
  OceanModifierData *omd = (OceanModifierData *)ptr->data;
  float old_value = omd->chop_amount;

  omd->chop_amount = value;

  if ((old_value == 0.0f && value > 0.0f) || (old_value > 0.0f && value == 0.0f)) {
    BKE_ocean_free_modifier_cache(omd);
  }
}

static bool rna_LaplacianDeformModifier_is_bind_get(PointerRNA *ptr)
{
  LaplacianDeformModifierData *lmd = (LaplacianDeformModifierData *)ptr->data;
  return ((lmd->flag & MOD_LAPLACIANDEFORM_BIND) && (lmd->vertexco != NULL));
}

/* NOTE: Curve and array modifiers requires curve path to be evaluated,
 * dependency graph will make sure that curve eval would create such a path,
 * but if curve was already evaluated we might miss path.
 *
 * So what we do here is: if path was not calculated for target curve we
 * tag it for update.
 */

static void rna_CurveModifier_dependency_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  CurveModifierData *cmd = (CurveModifierData *)ptr->data;
  rna_Modifier_update(bmain, scene, ptr);
  DEG_relations_tag_update(bmain);
  if (cmd->object != NULL) {
    Curve *curve = cmd->object->data;
    if ((curve->flag & CU_PATH) == 0) {
      DEG_id_tag_update(&curve->id, ID_RECALC_GEOMETRY);
    }
  }
}

static void rna_ArrayModifier_dependency_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  ArrayModifierData *amd = (ArrayModifierData *)ptr->data;
  rna_Modifier_update(bmain, scene, ptr);
  DEG_relations_tag_update(bmain);
  if (amd->curve_ob != NULL) {
    Curve *curve = amd->curve_ob->data;
    if ((curve->flag & CU_PATH) == 0) {
      DEG_id_tag_update(&curve->id, ID_RECALC_GEOMETRY);
    }
  }
}

static void rna_DataTransferModifier_use_data_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  DataTransferModifierData *dtmd = (DataTransferModifierData *)ptr->data;

  if (!(dtmd->flags & MOD_DATATRANSFER_USE_VERT)) {
    dtmd->data_types &= ~DT_TYPE_VERT_ALL;
  }
  if (!(dtmd->flags & MOD_DATATRANSFER_USE_EDGE)) {
    dtmd->data_types &= ~DT_TYPE_EDGE_ALL;
  }
  if (!(dtmd->flags & MOD_DATATRANSFER_USE_LOOP)) {
    dtmd->data_types &= ~DT_TYPE_LOOP_ALL;
  }
  if (!(dtmd->flags & MOD_DATATRANSFER_USE_POLY)) {
    dtmd->data_types &= ~DT_TYPE_POLY_ALL;
  }

  rna_Modifier_dependency_update(bmain, scene, ptr);
}

static void rna_DataTransferModifier_data_types_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  DataTransferModifierData *dtmd = (DataTransferModifierData *)ptr->data;
  const int item_types = BKE_object_data_transfer_get_dttypes_item_types(dtmd->data_types);

  if (item_types & ME_VERT) {
    dtmd->flags |= MOD_DATATRANSFER_USE_VERT;
  }
  if (item_types & ME_EDGE) {
    dtmd->flags |= MOD_DATATRANSFER_USE_EDGE;
  }
  if (item_types & ME_LOOP) {
    dtmd->flags |= MOD_DATATRANSFER_USE_LOOP;
  }
  if (item_types & ME_POLY) {
    dtmd->flags |= MOD_DATATRANSFER_USE_POLY;
  }

  rna_Modifier_dependency_update(bmain, scene, ptr);
}

static void rna_DataTransferModifier_verts_data_types_set(struct PointerRNA *ptr, int value)
{
  DataTransferModifierData *dtmd = (DataTransferModifierData *)ptr->data;

  dtmd->data_types &= ~DT_TYPE_VERT_ALL;
  dtmd->data_types |= value;
}

static void rna_DataTransferModifier_edges_data_types_set(struct PointerRNA *ptr, int value)
{
  DataTransferModifierData *dtmd = (DataTransferModifierData *)ptr->data;

  dtmd->data_types &= ~DT_TYPE_EDGE_ALL;
  dtmd->data_types |= value;
}

static void rna_DataTransferModifier_loops_data_types_set(struct PointerRNA *ptr, int value)
{
  DataTransferModifierData *dtmd = (DataTransferModifierData *)ptr->data;

  dtmd->data_types &= ~DT_TYPE_LOOP_ALL;
  dtmd->data_types |= value;
}

static void rna_DataTransferModifier_polys_data_types_set(struct PointerRNA *ptr, int value)
{
  DataTransferModifierData *dtmd = (DataTransferModifierData *)ptr->data;

  dtmd->data_types &= ~DT_TYPE_POLY_ALL;
  dtmd->data_types |= value;
}

static const EnumPropertyItem *rna_DataTransferModifier_layers_select_src_itemf(bContext *C,
                                                                                PointerRNA *ptr,
                                                                                PropertyRNA *prop,
                                                                                bool *r_free)
{
  DataTransferModifierData *dtmd = (DataTransferModifierData *)ptr->data;
  EnumPropertyItem *item = NULL, tmp_item = {0};
  int totitem = 0;

  if (!C) { /* needed for docs and i18n tools */
    return rna_enum_dt_layers_select_src_items;
  }

  /* No active here! */
  RNA_enum_items_add_value(
      &item, &totitem, rna_enum_dt_layers_select_src_items, DT_LAYERS_ALL_SRC);

  if (STREQ(RNA_property_identifier(prop), "layers_vgroup_select_src")) {
    Object *ob_src = dtmd->ob_source;

#  if 0 /* XXX Don't think we want this in modifier version... */
    if (BKE_object_pose_armature_get(ob_src)) {
      RNA_enum_items_add_value(
          &item, &totitem, rna_enum_dt_layers_select_src_items, DT_LAYERS_VGROUP_SRC_BONE_SELECT);
      RNA_enum_items_add_value(
          &item, &totitem, rna_enum_dt_layers_select_src_items, DT_LAYERS_VGROUP_SRC_BONE_DEFORM);
    }
#  endif

    if (ob_src) {
      const bDeformGroup *dg;
      int i;

      RNA_enum_item_add_separator(&item, &totitem);

      const ListBase *defbase = BKE_object_defgroup_list(ob_src);
      for (i = 0, dg = defbase->first; dg; i++, dg = dg->next) {
        tmp_item.value = i;
        tmp_item.identifier = tmp_item.name = dg->name;
        RNA_enum_item_add(&item, &totitem, &tmp_item);
      }
    }
  }
  else if (STREQ(RNA_property_identifier(prop), "layers_shapekey_select_src")) {
    /* TODO */
  }
  else if (STREQ(RNA_property_identifier(prop), "layers_uv_select_src")) {
    Object *ob_src = dtmd->ob_source;

    if (ob_src) {
      int num_data, i;

      Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
      const Object *ob_eval = DEG_get_evaluated_object(depsgraph, ob_src);
      if (!ob_eval) {
        RNA_enum_item_end(&item, &totitem);
        *r_free = true;
        return item;
      }
      const Mesh *me_eval = BKE_object_get_evaluated_mesh(ob_eval);
      if (!me_eval) {
        RNA_enum_item_end(&item, &totitem);
        *r_free = true;
        return item;
      }

      num_data = CustomData_number_of_layers(&me_eval->ldata, CD_PROP_FLOAT2);

      RNA_enum_item_add_separator(&item, &totitem);

      for (i = 0; i < num_data; i++) {
        tmp_item.value = i;
        tmp_item.identifier = tmp_item.name = CustomData_get_layer_name(
            &me_eval->ldata, CD_PROP_FLOAT2, i);
        RNA_enum_item_add(&item, &totitem, &tmp_item);
      }
    }
  }
  else if (STREQ(RNA_property_identifier(prop), "layers_vcol_vert_select_src") ||
           STREQ(RNA_property_identifier(prop), "layers_vcol_loop_select_src"))
  {
    Object *ob_src = dtmd->ob_source;

    if (ob_src) {
      eAttrDomain domain = STREQ(RNA_property_identifier(prop), "layers_vcol_vert_select_src") ?
                               ATTR_DOMAIN_POINT :
                               ATTR_DOMAIN_CORNER;

      Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
      const Object *ob_eval = DEG_get_evaluated_object(depsgraph, ob_src);
      if (!ob_eval) {
        RNA_enum_item_end(&item, &totitem);
        *r_free = true;
        return item;
      }
      const Mesh *mesh_eval = BKE_object_get_evaluated_mesh(ob_eval);
      if (!mesh_eval) {
        RNA_enum_item_end(&item, &totitem);
        *r_free = true;
        return item;
      }

      const CustomData *cdata;
      if (domain == ATTR_DOMAIN_POINT) {
        cdata = &mesh_eval->vdata;
      }
      else {
        cdata = &mesh_eval->ldata;
      }

      eCustomDataType types[2] = {CD_PROP_COLOR, CD_PROP_BYTE_COLOR};

      int idx = 0;
      for (int i = 0; i < 2; i++) {
        int num_data = CustomData_number_of_layers(cdata, types[i]);

        RNA_enum_item_add_separator(&item, &totitem);

        for (int j = 0; j < num_data; j++) {
          tmp_item.value = idx++;
          tmp_item.identifier = tmp_item.name = CustomData_get_layer_name(cdata, types[i], j);
          RNA_enum_item_add(&item, &totitem, &tmp_item);
        }
      }
    }
  }

  RNA_enum_item_end(&item, &totitem);
  *r_free = true;

  return item;
}

static const EnumPropertyItem *rna_DataTransferModifier_layers_select_dst_itemf(bContext *C,
                                                                                PointerRNA *ptr,
                                                                                PropertyRNA *prop,
                                                                                bool *r_free)
{
  DataTransferModifierData *dtmd = (DataTransferModifierData *)ptr->data;
  EnumPropertyItem *item = NULL, tmp_item = {0};
  int totitem = 0;

  if (!C) { /* needed for docs and i18n tools */
    return rna_enum_dt_layers_select_dst_items;
  }

  /* No active here! */
  RNA_enum_items_add_value(
      &item, &totitem, rna_enum_dt_layers_select_dst_items, DT_LAYERS_NAME_DST);
  RNA_enum_items_add_value(
      &item, &totitem, rna_enum_dt_layers_select_dst_items, DT_LAYERS_INDEX_DST);

  if (STREQ(RNA_property_identifier(prop), "layers_vgroup_select_dst")) {
    /* Only list destination layers if we have a single source! */
    if (dtmd->layers_select_src[DT_MULTILAYER_INDEX_MDEFORMVERT] >= 0) {
      Object *ob_dst = CTX_data_active_object(C); /* XXX Is this OK? */

      if (ob_dst) {
        const bDeformGroup *dg;
        int i;

        RNA_enum_item_add_separator(&item, &totitem);

        const ListBase *defbase = BKE_object_defgroup_list(ob_dst);
        for (i = 0, dg = defbase->first; dg; i++, dg = dg->next) {
          tmp_item.value = i;
          tmp_item.identifier = tmp_item.name = dg->name;
          RNA_enum_item_add(&item, &totitem, &tmp_item);
        }
      }
    }
  }
  else if (STREQ(RNA_property_identifier(prop), "layers_shapekey_select_dst")) {
    /* TODO */
  }
  else if (STREQ(RNA_property_identifier(prop), "layers_uv_select_dst")) {
    /* Only list destination layers if we have a single source! */
    if (dtmd->layers_select_src[DT_MULTILAYER_INDEX_UV] >= 0) {
      Object *ob_dst = CTX_data_active_object(C); /* XXX Is this OK? */

      if (ob_dst && ob_dst->data) {
        Mesh *me_dst;
        CustomData *ldata;
        int num_data, i;

        me_dst = ob_dst->data;
        ldata = &me_dst->ldata;
        num_data = CustomData_number_of_layers(ldata, CD_PROP_FLOAT2);

        RNA_enum_item_add_separator(&item, &totitem);

        for (i = 0; i < num_data; i++) {
          tmp_item.value = i;
          tmp_item.identifier = tmp_item.name = CustomData_get_layer_name(
              ldata, CD_PROP_FLOAT2, i);
          RNA_enum_item_add(&item, &totitem, &tmp_item);
        }
      }
    }
  }
  else if (STREQ(RNA_property_identifier(prop), "layers_vcol_vert_select_dst") ||
           STREQ(RNA_property_identifier(prop), "layers_vcol_loop_select_dst"))
  {
    int multilayer_index = STREQ(RNA_property_identifier(prop), "layers_vcol_vert_select_dst") ?
                               DT_MULTILAYER_INDEX_VCOL_VERT :
                               DT_MULTILAYER_INDEX_VCOL_LOOP;

    /* Only list destination layers if we have a single source! */
    if (dtmd->layers_select_src[multilayer_index] >= 0) {
      Object *ob_dst = CTX_data_active_object(C); /* XXX Is this OK? */

      if (ob_dst && ob_dst->data) {
        eCustomDataType types[2] = {CD_PROP_COLOR, CD_PROP_BYTE_COLOR};

        Mesh *me_dst = ob_dst->data;
        CustomData *cdata = STREQ(RNA_property_identifier(prop), "layers_vcol_vert_select_dst") ?
                                &me_dst->vdata :
                                &me_dst->ldata;

        int idx = 0;
        for (int i = 0; i < 2; i++) {
          int num_data = CustomData_number_of_layers(cdata, types[i]);

          RNA_enum_item_add_separator(&item, &totitem);

          for (int j = 0; j < num_data; j++) {
            tmp_item.value = idx++;
            tmp_item.identifier = tmp_item.name = CustomData_get_layer_name(cdata, types[i], j);
            RNA_enum_item_add(&item, &totitem, &tmp_item);
          }
        }
      }
    }
  }

  RNA_enum_item_end(&item, &totitem);
  *r_free = true;

  return item;
}

static const EnumPropertyItem *rna_DataTransferModifier_mix_mode_itemf(bContext *C,
                                                                       PointerRNA *ptr,
                                                                       PropertyRNA *UNUSED(prop),
                                                                       bool *r_free)
{
  DataTransferModifierData *dtmd = (DataTransferModifierData *)ptr->data;
  EnumPropertyItem *item = NULL;
  int totitem = 0;

  bool support_advanced_mixing, support_threshold;

  if (!C) { /* needed for docs and i18n tools */
    return rna_enum_dt_mix_mode_items;
  }

  RNA_enum_items_add_value(&item, &totitem, rna_enum_dt_mix_mode_items, CDT_MIX_TRANSFER);

  BKE_object_data_transfer_get_dttypes_capacity(
      dtmd->data_types, &support_advanced_mixing, &support_threshold);

  if (support_threshold) {
    RNA_enum_items_add_value(
        &item, &totitem, rna_enum_dt_mix_mode_items, CDT_MIX_REPLACE_ABOVE_THRESHOLD);
    RNA_enum_items_add_value(
        &item, &totitem, rna_enum_dt_mix_mode_items, CDT_MIX_REPLACE_BELOW_THRESHOLD);
  }

  if (support_advanced_mixing) {
    RNA_enum_item_add_separator(&item, &totitem);
    RNA_enum_items_add_value(&item, &totitem, rna_enum_dt_mix_mode_items, CDT_MIX_MIX);
    RNA_enum_items_add_value(&item, &totitem, rna_enum_dt_mix_mode_items, CDT_MIX_ADD);
    RNA_enum_items_add_value(&item, &totitem, rna_enum_dt_mix_mode_items, CDT_MIX_SUB);
    RNA_enum_items_add_value(&item, &totitem, rna_enum_dt_mix_mode_items, CDT_MIX_MUL);
  }

  RNA_enum_item_end(&item, &totitem);
  *r_free = true;

  return item;
}

static void rna_CorrectiveSmoothModifier_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  CorrectiveSmoothModifierData *csmd = (CorrectiveSmoothModifierData *)ptr->data;

  MEM_SAFE_FREE(csmd->delta_cache.deltas);

  rna_Modifier_update(bmain, scene, ptr);
}

static void rna_CorrectiveSmoothModifier_rest_source_update(Main *bmain,
                                                            Scene *scene,
                                                            PointerRNA *ptr)
{
  CorrectiveSmoothModifierData *csmd = (CorrectiveSmoothModifierData *)ptr->data;

  if (csmd->rest_source != MOD_CORRECTIVESMOOTH_RESTSOURCE_BIND) {
    MEM_SAFE_FREE(csmd->bind_coords);
    csmd->bind_coords_num = 0;
  }

  rna_CorrectiveSmoothModifier_update(bmain, scene, ptr);
}

static bool rna_CorrectiveSmoothModifier_is_bind_get(PointerRNA *ptr)
{
  CorrectiveSmoothModifierData *csmd = (CorrectiveSmoothModifierData *)ptr->data;
  return (csmd->bind_coords != NULL);
}

static bool rna_SurfaceDeformModifier_is_bound_get(PointerRNA *ptr)
{
  return (((SurfaceDeformModifierData *)ptr->data)->verts != NULL);
}

static bool rna_ParticleInstanceModifier_particle_system_poll(PointerRNA *ptr,
                                                              const PointerRNA value)
{
  ParticleInstanceModifierData *psmd = ptr->data;
  ParticleSystem *psys = value.data;

  if (!psmd->ob) {
    return false;
  }

  /* make sure psys is in the object */
  return BLI_findindex(&psmd->ob->particlesystem, psys) != -1;
}

static PointerRNA rna_ParticleInstanceModifier_particle_system_get(PointerRNA *ptr)
{
  ParticleInstanceModifierData *psmd = ptr->data;
  ParticleSystem *psys;
  PointerRNA rptr;

  if (!psmd->ob) {
    return PointerRNA_NULL;
  }

  psys = BLI_findlink(&psmd->ob->particlesystem, psmd->psys - 1);
  RNA_pointer_create((ID *)psmd->ob, &RNA_ParticleSystem, psys, &rptr);
  return rptr;
}

static void rna_ParticleInstanceModifier_particle_system_set(PointerRNA *ptr,
                                                             const PointerRNA value,
                                                             struct ReportList *UNUSED(reports))
{
  ParticleInstanceModifierData *psmd = ptr->data;

  if (!psmd->ob) {
    return;
  }

  psmd->psys = BLI_findindex(&psmd->ob->particlesystem, value.data) + 1;
  CLAMP_MIN(psmd->psys, 1);
}

/**
 * Special set callback that just changes the first bit of the expansion flag.
 * This way the expansion state of all the sub-panels is not changed by RNA.
 */
static void rna_Modifier_show_expanded_set(PointerRNA *ptr, bool value)
{
  ModifierData *md = ptr->data;
  SET_FLAG_FROM_TEST(md->ui_expand_flag, value, UI_PANEL_DATA_EXPAND_ROOT);
}

/**
 * Only check the first bit of the expansion flag for the main panel's expansion,
 * maintaining compatibility with older versions where there was only one expansion
 * value.
 */
static bool rna_Modifier_show_expanded_get(PointerRNA *ptr)
{
  ModifierData *md = ptr->data;
  return md->ui_expand_flag & UI_PANEL_DATA_EXPAND_ROOT;
}

static bool rna_NodesModifier_node_group_poll(PointerRNA *UNUSED(ptr), PointerRNA value)
{
  bNodeTree *ntree = value.data;
  return ntree->type == NTREE_GEOMETRY;
}

static void rna_NodesModifier_node_group_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  Object *object = (Object *)ptr->owner_id;
  NodesModifierData *nmd = ptr->data;
  rna_Modifier_dependency_update(bmain, scene, ptr);
  MOD_nodes_update_interface(object, nmd);
}

static IDProperty **rna_NodesModifier_properties(PointerRNA *ptr)
{
  NodesModifierData *nmd = ptr->data;
  NodesModifierSettings *settings = &nmd->settings;
  return &settings->properties;
}
#else

static void rna_def_property_subdivision_common(StructRNA *srna)
{
  PropertyRNA *prop;
  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "uv_smooth", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "uv_smooth");
  RNA_def_property_enum_items(prop, rna_enum_subdivision_uv_smooth_items);
  RNA_def_property_ui_text(prop, "UV Smooth", "Controls how smoothing is applied to UVs");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "quality", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, NULL, "quality");
  RNA_def_property_range(prop, 1, 10);
  RNA_def_property_ui_range(prop, 1, 6, 1, -1);
  RNA_def_property_ui_text(
      prop, "Quality", "Accuracy of vertex positions, lower value is faster but less precise");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "boundary_smooth", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "boundary_smooth");
  RNA_def_property_enum_items(prop, rna_enum_subdivision_boundary_smooth_items);
  RNA_def_property_ui_text(prop, "Boundary Smooth", "Controls how open boundaries are smoothed");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_modifier_subsurf(BlenderRNA *brna)
{
  static const EnumPropertyItem prop_subdivision_type_items[] = {
      {SUBSURF_TYPE_CATMULL_CLARK, "CATMULL_CLARK", 0, "Catmull-Clark", ""},
      {SUBSURF_TYPE_SIMPLE, "SIMPLE", 0, "Simple", ""},
      {0, NULL, 0, NULL, NULL},
  };

  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "SubsurfModifier", "Modifier");
  RNA_def_struct_ui_text(srna, "Subdivision Surface Modifier", "Subdivision surface modifier");
  RNA_def_struct_sdna(srna, "SubsurfModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_SUBSURF);

  rna_def_property_subdivision_common(srna);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "subdivision_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "subdivType");
  RNA_def_property_enum_items(prop, prop_subdivision_type_items);
  RNA_def_property_ui_text(prop, "Subdivision Type", "Select type of subdivision algorithm");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  /* see CCGSUBSURF_LEVEL_MAX for max limit */
  prop = RNA_def_property(srna, "levels", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, NULL, "levels");
  RNA_def_property_range(prop, 0, 11);
  RNA_def_property_ui_range(prop, 0, 6, 1, -1);
  RNA_def_property_ui_text(prop, "Levels", "Number of subdivisions to perform");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "render_levels", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, NULL, "renderLevels");
  RNA_def_property_range(prop, 0, 11);
  RNA_def_property_ui_range(prop, 0, 6, 1, -1);
  RNA_def_property_ui_text(
      prop, "Render Levels", "Number of subdivisions to perform when rendering");

  prop = RNA_def_property(srna, "show_only_control_edges", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flags", eSubsurfModifierFlag_ControlEdges);
  RNA_def_property_ui_text(prop, "Optimal Display", "Skip displaying interior subdivided edges");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_creases", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flags", eSubsurfModifierFlag_UseCrease);
  RNA_def_property_ui_text(
      prop, "Use Creases", "Use mesh crease information to sharpen edges or corners");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_custom_normals", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flags", eSubsurfModifierFlag_UseCustomNormals);
  RNA_def_property_ui_text(
      prop, "Use Custom Normals", "Interpolates existing custom normals to resulting mesh");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_limit_surface", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(
      prop, NULL, "flags", eSubsurfModifierFlag_UseRecursiveSubdivision);
  RNA_def_property_ui_text(prop,
                           "Use Limit Surface",
                           "Place vertices at the surface that would be produced with infinite "
                           "levels of subdivision (smoothest possible shape)");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_modifier_generic_map_info(StructRNA *srna)
{
  static const EnumPropertyItem prop_texture_coordinates_items[] = {
      {MOD_DISP_MAP_LOCAL,
       "LOCAL",
       0,
       "Local",
       "Use the local coordinate system for the texture coordinates"},
      {MOD_DISP_MAP_GLOBAL,
       "GLOBAL",
       0,
       "Global",
       "Use the global coordinate system for the texture coordinates"},
      {MOD_DISP_MAP_OBJECT,
       "OBJECT",
       0,
       "Object",
       "Use the linked object's local coordinate system for the texture coordinates"},
      {MOD_DISP_MAP_UV, "UV", 0, "UV", "Use UV coordinates for the texture coordinates"},
      {0, NULL, 0, NULL, NULL},
  };

  PropertyRNA *prop;

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "texture", PROP_POINTER, PROP_NONE);
  RNA_def_property_ui_text(prop, "Texture", "");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

  prop = RNA_def_property(srna, "texture_coords", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "texmapping");
  RNA_def_property_enum_items(prop, prop_texture_coordinates_items);
  RNA_def_property_ui_text(prop, "Texture Coordinates", "");
  RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

  prop = RNA_def_property(srna, "uv_layer", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "uvlayer_name");
  RNA_def_property_ui_text(prop, "UV Map", "UV map name");
  RNA_def_property_string_funcs(prop, NULL, NULL, "rna_MappingInfoModifier_uvlayer_name_set");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "texture_coords_object", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "map_object");
  RNA_def_property_ui_text(
      prop, "Texture Coordinate Object", "Object to set the texture coordinates");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

  prop = RNA_def_property(srna, "texture_coords_bone", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "map_bone");
  RNA_def_property_ui_text(prop, "Texture Coordinate Bone", "Bone to set the texture coordinates");
  RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_modifier_warp(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "WarpModifier", "Modifier");
  RNA_def_struct_ui_text(srna, "Warp Modifier", "Warp modifier");
  RNA_def_struct_sdna(srna, "WarpModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_WARP);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "object_from", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "object_from");
  RNA_def_property_ui_text(prop, "Object From", "Object to transform from");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

  prop = RNA_def_property(srna, "bone_from", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "bone_from");
  RNA_def_property_ui_text(prop, "Bone From", "Bone to transform from");
  RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

  prop = RNA_def_property(srna, "object_to", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "object_to");
  RNA_def_property_ui_text(prop, "Object To", "Object to transform to");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

  prop = RNA_def_property(srna, "bone_to", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "bone_to");
  RNA_def_property_ui_text(prop, "Bone To", "Bone defining offset");
  RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

  prop = RNA_def_property(srna, "strength", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
  RNA_def_property_ui_range(prop, -100, 100, 10, 2);
  RNA_def_property_ui_text(prop, "Strength", "");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "falloff_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, modifier_warp_falloff_items);
  RNA_def_property_ui_text(prop, "Falloff Type", "");
  RNA_def_property_translation_context(prop,
                                       BLT_I18NCONTEXT_ID_CURVE_LEGACY); /* Abusing id_curve :/ */
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "falloff_radius", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_ui_text(prop, "Radius", "Radius to apply");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "falloff_curve", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "curfalloff");
  RNA_def_property_ui_text(prop, "Falloff Curve", "Custom falloff curve");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_volume_preserve", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_WARP_VOLUME_PRESERVE);
  RNA_def_property_ui_text(prop, "Preserve Volume", "Preserve volume when rotations are used");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "defgrp_name");
  RNA_def_property_ui_text(prop, "Vertex Group", "Vertex group name for modulating the deform");
  RNA_def_property_string_funcs(prop, NULL, NULL, "rna_WarpModifier_defgrp_name_set");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "invert_vertex_group", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_WARP_INVERT_VGROUP);
  RNA_def_property_ui_text(prop, "Invert", "Invert vertex group influence");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  RNA_define_lib_overridable(false);

  rna_def_modifier_generic_map_info(srna);
}

static void rna_def_modifier_multires(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "MultiresModifier", "Modifier");
  RNA_def_struct_ui_text(srna, "Multires Modifier", "Multiresolution mesh modifier");
  RNA_def_struct_sdna(srna, "MultiresModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_MULTIRES);

  RNA_define_lib_overridable(true);

  rna_def_property_subdivision_common(srna);

  prop = RNA_def_property(srna, "levels", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, NULL, "lvl");
  RNA_def_property_ui_text(prop, "Levels", "Number of subdivisions to use in the viewport");
  RNA_def_property_int_funcs(prop, NULL, NULL, "rna_MultiresModifier_level_range");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "sculpt_levels", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, NULL, "sculptlvl");
  RNA_def_property_ui_text(prop, "Sculpt Levels", "Number of subdivisions to use in sculpt mode");
  RNA_def_property_int_funcs(prop, NULL, NULL, "rna_MultiresModifier_level_range");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "render_levels", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, NULL, "renderlvl");
  RNA_def_property_ui_text(prop, "Render Levels", "The subdivision level visible at render time");
  RNA_def_property_int_funcs(prop, NULL, NULL, "rna_MultiresModifier_level_range");

  prop = RNA_def_property(srna, "total_levels", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, NULL, "totlvl");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop, "Total Levels", "Number of subdivisions for which displacements are stored");

  prop = RNA_def_property(srna, "is_external", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_boolean_funcs(prop, "rna_MultiresModifier_external_get", NULL);
  RNA_def_property_ui_text(
      prop, "External", "Store multires displacements outside the .blend file, to save memory");

  prop = RNA_def_property(srna, "filepath", PROP_STRING, PROP_FILEPATH);
  RNA_def_property_string_funcs(prop,
                                "rna_MultiresModifier_filepath_get",
                                "rna_MultiresModifier_filepath_length",
                                "rna_MultiresModifier_filepath_set");
  RNA_def_property_ui_text(prop, "File Path", "Path to external displacements file");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "show_only_control_edges", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flags", eMultiresModifierFlag_ControlEdges);
  RNA_def_property_ui_text(
      prop, "Optimal Display", "Skip drawing/rendering of interior subdivided edges");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_creases", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flags", eMultiresModifierFlag_UseCrease);
  RNA_def_property_ui_text(
      prop, "Use Creases", "Use mesh crease information to sharpen edges or corners");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_custom_normals", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flags", eMultiresModifierFlag_UseCustomNormals);
  RNA_def_property_ui_text(
      prop, "Use Custom Normals", "Interpolates existing custom normals to resulting mesh");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_sculpt_base_mesh", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flags", eMultiresModifierFlag_UseSculptBaseMesh);
  RNA_def_property_ui_text(prop,
                           "Sculpt Base Mesh",
                           "Make Sculpt Mode tools deform the base mesh while previewing the "
                           "displacement of higher subdivision levels");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_modifier_lattice(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "LatticeModifier", "Modifier");
  RNA_def_struct_ui_text(srna, "Lattice Modifier", "Lattice deformation modifier");
  RNA_def_struct_sdna(srna, "LatticeModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_LATTICE);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
  RNA_def_property_ui_text(prop, "Object", "Lattice object to deform with");
  RNA_def_property_pointer_funcs(
      prop, NULL, "rna_LatticeModifier_object_set", NULL, "rna_Lattice_object_poll");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

  prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "name");
  RNA_def_property_ui_text(
      prop,
      "Vertex Group",
      "Name of Vertex Group which determines influence of modifier per point");
  RNA_def_property_string_funcs(prop, NULL, NULL, "rna_LatticeModifier_name_set");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "invert_vertex_group", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_LATTICE_INVERT_VGROUP);
  RNA_def_property_ui_text(prop, "Invert", "Invert vertex group influence");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "strength", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
  RNA_def_property_ui_range(prop, 0, 1, 10, 2);
  RNA_def_property_ui_text(prop, "Strength", "Strength of modifier effect");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_modifier_curve(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem prop_deform_axis_items[] = {
      {MOD_CURVE_POSX, "POS_X", 0, "X", ""},
      {MOD_CURVE_POSY, "POS_Y", 0, "Y", ""},
      {MOD_CURVE_POSZ, "POS_Z", 0, "Z", ""},
      {MOD_CURVE_NEGX, "NEG_X", 0, "-X", ""},
      {MOD_CURVE_NEGY, "NEG_Y", 0, "-Y", ""},
      {MOD_CURVE_NEGZ, "NEG_Z", 0, "-Z", ""},
      {0, NULL, 0, NULL, NULL},
  };

  srna = RNA_def_struct(brna, "CurveModifier", "Modifier");
  RNA_def_struct_ui_text(srna, "Curve Modifier", "Curve deformation modifier");
  RNA_def_struct_sdna(srna, "CurveModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_CURVE);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
  RNA_def_property_ui_text(prop, "Object", "Curve object to deform with");
  RNA_def_property_pointer_funcs(
      prop, NULL, "rna_CurveModifier_object_set", NULL, "rna_Curve_object_poll");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_update(prop, 0, "rna_CurveModifier_dependency_update");

  prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "name");
  RNA_def_property_ui_text(
      prop,
      "Vertex Group",
      "Name of Vertex Group which determines influence of modifier per point");
  RNA_def_property_string_funcs(prop, NULL, NULL, "rna_CurveModifier_name_set");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "invert_vertex_group", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_CURVE_INVERT_VGROUP);
  RNA_def_property_ui_text(prop, "Invert", "Invert vertex group influence");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "deform_axis", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "defaxis");
  RNA_def_property_enum_items(prop, prop_deform_axis_items);
  RNA_def_property_ui_text(prop, "Deform Axis", "The axis that the curve deforms along");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_modifier_build(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "BuildModifier", "Modifier");
  RNA_def_struct_ui_text(srna, "Build Modifier", "Build effect modifier");
  RNA_def_struct_sdna(srna, "BuildModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_BUILD);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "frame_start", PROP_FLOAT, PROP_TIME);
  RNA_def_property_float_sdna(prop, NULL, "start");
  RNA_def_property_range(prop, MINAFRAMEF, MAXFRAMEF);
  RNA_def_property_ui_text(prop, "Start Frame", "Start frame of the effect");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "frame_duration", PROP_FLOAT, PROP_TIME);
  RNA_def_property_float_sdna(prop, NULL, "length");
  RNA_def_property_range(prop, 1, MAXFRAMEF);
  RNA_def_property_ui_text(prop, "Length", "Total time the build effect requires");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_reverse", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_BUILD_FLAG_REVERSE);
  RNA_def_property_ui_text(prop, "Reversed", "Deconstruct the mesh instead of building it");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_random_order", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_BUILD_FLAG_RANDOMIZE);
  RNA_def_property_ui_text(prop, "Randomize", "Randomize the faces or edges during build");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "seed", PROP_INT, PROP_NONE);
  RNA_def_property_range(prop, 1, MAXFRAMEF);
  RNA_def_property_ui_text(prop, "Seed", "Seed for random if used");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_modifier_mirror(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "MirrorModifier", "Modifier");
  RNA_def_struct_ui_text(srna, "Mirror Modifier", "Mirroring modifier");
  RNA_def_struct_sdna(srna, "MirrorModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_MIRROR);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "use_axis", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_MIR_AXIS_X);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Mirror Axis", "Enable axis mirror");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_bisect_axis", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_MIR_BISECT_AXIS_X);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Bisect Axis", "Cuts the mesh across the mirror plane");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_bisect_flip_axis", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_MIR_BISECT_FLIP_AXIS_X);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Bisect Flip Axis", "Flips the direction of the slice");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_clip", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_MIR_CLIPPING);
  RNA_def_property_ui_text(
      prop, "Clip", "Prevent vertices from going through the mirror during transform");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_mirror_vertex_groups", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_MIR_VGROUP);
  RNA_def_property_ui_text(prop, "Mirror Vertex Groups", "Mirror vertex groups (e.g. .R->.L)");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_mirror_merge", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", MOD_MIR_NO_MERGE);
  RNA_def_property_ui_text(prop, "Merge Vertices", "Merge vertices within the merge threshold");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_mirror_u", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_MIR_MIRROR_U);
  RNA_def_property_ui_text(
      prop, "Mirror U", "Mirror the U texture coordinate around the flip offset point");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_mirror_v", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_MIR_MIRROR_V);
  RNA_def_property_ui_text(
      prop, "Mirror V", "Mirror the V texture coordinate around the flip offset point");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_mirror_udim", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_MIR_MIRROR_UDIM);
  RNA_def_property_ui_text(
      prop, "Mirror UDIM", "Mirror the texture coordinate around each tile center");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "mirror_offset_u", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "uv_offset[0]");
  RNA_def_property_range(prop, -1, 1);
  RNA_def_property_ui_range(prop, -1, 1, 2, 4);
  RNA_def_property_ui_text(
      prop,
      "Flip U Offset",
      "Amount to offset mirrored UVs flipping point from the 0.5 on the U axis");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "mirror_offset_v", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "uv_offset[1]");
  RNA_def_property_range(prop, -1, 1);
  RNA_def_property_ui_range(prop, -1, 1, 2, 4);
  RNA_def_property_ui_text(
      prop,
      "Flip V Offset",
      "Amount to offset mirrored UVs flipping point from the 0.5 point on the V axis");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "offset_u", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "uv_offset_copy[0]");
  RNA_def_property_range(prop, -10000.0f, 10000.0f);
  RNA_def_property_ui_range(prop, -1, 1, 2, 4);
  RNA_def_property_ui_text(prop, "U Offset", "Mirrored UV offset on the U axis");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "offset_v", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "uv_offset_copy[1]");
  RNA_def_property_range(prop, -10000.0f, 10000.0f);
  RNA_def_property_ui_range(prop, -1, 1, 2, 4);
  RNA_def_property_ui_text(prop, "V Offset", "Mirrored UV offset on the V axis");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "merge_threshold", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, NULL, "tolerance");
  RNA_def_property_range(prop, 0, FLT_MAX);
  RNA_def_property_ui_range(prop, 0, 1, 0.01, 6);
  RNA_def_property_ui_text(
      prop, "Merge Distance", "Distance within which mirrored vertices are merged");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "bisect_threshold", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, NULL, "bisect_threshold");
  RNA_def_property_range(prop, 0, FLT_MAX);
  RNA_def_property_ui_range(prop, 0, 1, 0.01, 6);
  RNA_def_property_ui_text(
      prop, "Bisect Distance", "Distance from the bisect plane within which vertices are removed");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "mirror_object", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "mirror_ob");
  RNA_def_property_ui_text(prop, "Mirror Object", "Object to use as mirror");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_modifier_decimate(BlenderRNA *brna)
{
  static const EnumPropertyItem modifier_decim_mode_items[] = {
      {MOD_DECIM_MODE_COLLAPSE, "COLLAPSE", 0, "Collapse", "Use edge collapsing"},
      {MOD_DECIM_MODE_UNSUBDIV, "UNSUBDIV", 0, "Un-Subdivide", "Use un-subdivide face reduction"},
      {MOD_DECIM_MODE_DISSOLVE,
       "DISSOLVE",
       0,
       "Planar",
       "Dissolve geometry to form planar polygons"},
      {0, NULL, 0, NULL, NULL},
  };

  /* NOTE: keep in sync with operator 'MESH_OT_decimate'. */

  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "DecimateModifier", "Modifier");
  RNA_def_struct_ui_text(srna, "Decimate Modifier", "Decimation modifier");
  RNA_def_struct_sdna(srna, "DecimateModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_DECIM);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "decimate_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "mode");
  RNA_def_property_enum_items(prop, modifier_decim_mode_items);
  RNA_def_property_ui_text(prop, "Mode", "");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  /* (mode == MOD_DECIM_MODE_COLLAPSE) */
  prop = RNA_def_property(srna, "ratio", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "percent");
  RNA_def_property_range(prop, 0, 1);
  RNA_def_property_ui_range(prop, 0, 1, 1, 4);
  RNA_def_property_ui_text(prop, "Ratio", "Ratio of triangles to reduce to (collapse only)");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  /* (mode == MOD_DECIM_MODE_UNSUBDIV) */
  prop = RNA_def_property(srna, "iterations", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, NULL, "iter");
  RNA_def_property_range(prop, 0, SHRT_MAX);
  RNA_def_property_ui_range(prop, 0, 100, 1, -1);
  RNA_def_property_ui_text(
      prop, "Iterations", "Number of times reduce the geometry (unsubdivide only)");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  /* (mode == MOD_DECIM_MODE_DISSOLVE) */
  prop = RNA_def_property(srna, "angle_limit", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, NULL, "angle");
  RNA_def_property_range(prop, 0, DEG2RAD(180));
  RNA_def_property_ui_range(prop, 0, DEG2RAD(180), 10, 2);
  RNA_def_property_ui_text(prop, "Angle Limit", "Only dissolve angles below this (planar only)");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  /* (mode == MOD_DECIM_MODE_COLLAPSE) */
  prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "defgrp_name");
  RNA_def_property_ui_text(prop, "Vertex Group", "Vertex group name (collapse only)");
  RNA_def_property_string_funcs(prop, NULL, NULL, "rna_DecimateModifier_defgrp_name_set");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "invert_vertex_group", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_DECIM_FLAG_INVERT_VGROUP);
  RNA_def_property_ui_text(prop, "Invert", "Invert vertex group influence (collapse only)");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_collapse_triangulate", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_DECIM_FLAG_TRIANGULATE);
  RNA_def_property_ui_text(
      prop, "Triangulate", "Keep triangulated faces resulting from decimation (collapse only)");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_symmetry", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_DECIM_FLAG_SYMMETRY);
  RNA_def_property_ui_text(prop, "Symmetry", "Maintain symmetry on an axis");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "symmetry_axis", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "symmetry_axis");
  RNA_def_property_enum_items(prop, rna_enum_axis_xyz_items);
  RNA_def_property_ui_text(prop, "Axis", "Axis of symmetry");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "vertex_group_factor", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "defgrp_factor");
  RNA_def_property_range(prop, 0, 1000);
  RNA_def_property_ui_range(prop, 0, 10, 1, 4);
  RNA_def_property_ui_text(prop, "Factor", "Vertex group strength");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");
  /* end collapse-only option */

  /* (mode == MOD_DECIM_MODE_DISSOLVE) */
  prop = RNA_def_property(srna, "use_dissolve_boundaries", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_DECIM_FLAG_ALL_BOUNDARY_VERTS);
  RNA_def_property_ui_text(
      prop, "All Boundaries", "Dissolve all vertices in between face boundaries (planar only)");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "delimit", PROP_ENUM, PROP_NONE);
  RNA_def_property_flag(prop, PROP_ENUM_FLAG); /* important to run before default set */
  RNA_def_property_enum_items(prop, rna_enum_mesh_delimit_mode_items);
  RNA_def_property_ui_text(prop, "Delimit", "Limit merging geometry");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  /* end dissolve-only option */

  /* all modes use this */
  prop = RNA_def_property(srna, "face_count", PROP_INT, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop, "Face Count", "The current number of faces in the decimated mesh");

  RNA_define_lib_overridable(false);
}

static void rna_def_modifier_wave(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "WaveModifier", "Modifier");
  RNA_def_struct_ui_text(srna, "Wave Modifier", "Wave effect modifier");
  RNA_def_struct_sdna(srna, "WaveModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_WAVE);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "use_x", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_WAVE_X);
  RNA_def_property_ui_text(prop, "X", "X axis motion");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_y", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_WAVE_Y);
  RNA_def_property_ui_text(prop, "Y", "Y axis motion");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_cyclic", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_WAVE_CYCL);
  RNA_def_property_ui_text(prop, "Cyclic", "Cyclic wave effect");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_normal", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_WAVE_NORM);
  RNA_def_property_ui_text(prop, "Normals", "Displace along normals");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_normal_x", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_WAVE_NORM_X);
  RNA_def_property_ui_text(prop, "X Normal", "Enable displacement along the X normal");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_normal_y", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_WAVE_NORM_Y);
  RNA_def_property_ui_text(prop, "Y Normal", "Enable displacement along the Y normal");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_normal_z", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_WAVE_NORM_Z);
  RNA_def_property_ui_text(prop, "Z Normal", "Enable displacement along the Z normal");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "time_offset", PROP_FLOAT, PROP_TIME);
  RNA_def_property_float_sdna(prop, NULL, "timeoffs");
  RNA_def_property_range(prop, MINAFRAMEF, MAXFRAMEF);
  RNA_def_property_ui_text(
      prop,
      "Time Offset",
      "Either the starting frame (for positive speed) or ending frame (for negative speed)");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "lifetime", PROP_FLOAT, PROP_TIME);
  RNA_def_property_float_sdna(prop, NULL, "lifetime");
  RNA_def_property_range(prop, MINAFRAMEF, MAXFRAMEF);
  RNA_def_property_ui_text(
      prop, "Lifetime", "Lifetime of the wave in frames, zero means infinite");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "damping_time", PROP_FLOAT, PROP_TIME);
  RNA_def_property_float_sdna(prop, NULL, "damp");
  RNA_def_property_range(prop, MINAFRAMEF, MAXFRAMEF);
  RNA_def_property_ui_text(
      prop, "Damping Time", "Number of frames in which the wave damps out after it dies");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "falloff_radius", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, NULL, "falloff");
  RNA_def_property_range(prop, 0, FLT_MAX);
  RNA_def_property_ui_range(prop, 0, 100, 100, 2);
  RNA_def_property_ui_text(prop, "Falloff Radius", "Distance after which it fades out");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "start_position_x", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, NULL, "startx");
  RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
  RNA_def_property_ui_range(prop, -100, 100, 100, 2);
  RNA_def_property_ui_text(prop, "Start Position X", "X coordinate of the start position");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "start_position_y", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, NULL, "starty");
  RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
  RNA_def_property_ui_range(prop, -100, 100, 100, 2);
  RNA_def_property_ui_text(prop, "Start Position Y", "Y coordinate of the start position");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "start_position_object", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "objectcenter");
  RNA_def_property_ui_text(prop, "Start Position Object", "Object which defines the wave center");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

  prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "defgrp_name");
  RNA_def_property_ui_text(prop, "Vertex Group", "Vertex group name for modulating the wave");
  RNA_def_property_string_funcs(prop, NULL, NULL, "rna_WaveModifier_defgrp_name_set");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "invert_vertex_group", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_WAVE_INVERT_VGROUP);
  RNA_def_property_ui_text(prop, "Invert", "Invert vertex group influence");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "speed", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
  RNA_def_property_ui_range(prop, -1, 1, 10, 2);
  RNA_def_property_ui_text(
      prop, "Speed", "Speed of the wave, towards the starting point when negative");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "height", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
  RNA_def_property_ui_range(prop, -2, 2, 10, 2);
  RNA_def_property_ui_text(prop, "Height", "Height of the wave");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "width", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_range(prop, 0, FLT_MAX);
  RNA_def_property_ui_range(prop, 0, 5, 10, 2);
  RNA_def_property_ui_text(prop, "Width", "Distance between the waves");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "narrowness", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, NULL, "narrow");
  RNA_def_property_range(prop, 0, FLT_MAX);
  RNA_def_property_ui_range(prop, 0, 10, 10, 2);
  RNA_def_property_ui_text(
      prop,
      "Narrowness",
      "Distance between the top and the base of a wave, the higher the value, "
      "the more narrow the wave");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  RNA_define_lib_overridable(false);

  rna_def_modifier_generic_map_info(srna);
}

static void rna_def_modifier_armature(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "ArmatureModifier", "Modifier");
  RNA_def_struct_ui_text(srna, "Armature Modifier", "Armature deformation modifier");
  RNA_def_struct_sdna(srna, "ArmatureModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_ARMATURE);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
  RNA_def_property_ui_text(prop, "Object", "Armature object to deform with");
  RNA_def_property_pointer_funcs(
      prop, NULL, "rna_ArmatureModifier_object_set", NULL, "rna_Armature_object_poll");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

  prop = RNA_def_property(srna, "use_bone_envelopes", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "deformflag", ARM_DEF_ENVELOPE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Use Bone Envelopes", "Bind Bone envelopes to armature modifier");
  RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

  prop = RNA_def_property(srna, "use_vertex_groups", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "deformflag", ARM_DEF_VGROUP);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Use Vertex Groups", "Bind vertex groups to armature modifier");
  RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

  prop = RNA_def_property(srna, "use_deform_preserve_volume", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "deformflag", ARM_DEF_QUATERNION);
  RNA_def_property_ui_text(
      prop, "Preserve Volume", "Deform rotation interpolation with quaternions");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_multi_modifier", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "multi", 0);
  RNA_def_property_ui_text(
      prop,
      "Multi Modifier",
      "Use same input as previous modifier, and mix results using overall vgroup");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "defgrp_name");
  RNA_def_property_ui_text(
      prop,
      "Vertex Group",
      "Name of Vertex Group which determines influence of modifier per point");
  RNA_def_property_string_funcs(prop, NULL, NULL, "rna_ArmatureModifier_defgrp_name_set");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "invert_vertex_group", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "deformflag", ARM_DEF_INVERT_VGROUP);
  RNA_def_property_ui_text(prop, "Invert", "Invert vertex group influence");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_modifier_hook(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;
  FunctionRNA *func;
  PropertyRNA *parm;

  srna = RNA_def_struct(brna, "HookModifier", "Modifier");
  RNA_def_struct_ui_text(
      srna, "Hook Modifier", "Hook modifier to modify the location of vertices");
  RNA_def_struct_sdna(srna, "HookModifierData");
  RNA_def_struct_ui_icon(srna, ICON_HOOK);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "strength", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "force");
  RNA_def_property_range(prop, 0, 1);
  RNA_def_property_ui_text(prop, "Strength", "Relative force of the hook");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "falloff_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, modifier_warp_falloff_items); /* share the enum */
  RNA_def_property_ui_text(prop, "Falloff Type", "");
  RNA_def_property_translation_context(prop,
                                       BLT_I18NCONTEXT_ID_CURVE_LEGACY); /* Abusing id_curve :/ */
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "falloff_radius", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, NULL, "falloff");
  RNA_def_property_range(prop, 0, FLT_MAX);
  RNA_def_property_ui_range(prop, 0, 100, 100, 2);
  RNA_def_property_ui_text(
      prop, "Radius", "If not zero, the distance from the hook where influence ends");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "falloff_curve", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "curfalloff");
  RNA_def_property_ui_text(prop, "Falloff Curve", "Custom falloff curve");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "center", PROP_FLOAT, PROP_TRANSLATION);
  RNA_def_property_float_sdna(prop, NULL, "cent");
  RNA_def_property_ui_text(
      prop, "Hook Center", "Center of the hook, used for falloff and display");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "matrix_inverse", PROP_FLOAT, PROP_MATRIX);
  RNA_def_property_float_sdna(prop, NULL, "parentinv");
  RNA_def_property_multi_array(prop, 2, rna_matrix_dimsize_4x4);
  RNA_def_property_ui_text(
      prop, "Matrix", "Reverse the transformation between this object and its target");
  RNA_def_property_update(prop, NC_OBJECT | ND_TRANSFORM, "rna_Modifier_update");

  prop = RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
  RNA_def_property_ui_text(
      prop, "Object", "Parent Object for hook, also recalculates and clears offset");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_override_funcs(prop, NULL, NULL, "rna_HookModifier_object_override_apply");
  RNA_def_property_pointer_funcs(prop, NULL, "rna_HookModifier_object_set", NULL, NULL);
  RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

  prop = RNA_def_property(srna, "subtarget", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "subtarget");
  RNA_def_property_ui_text(
      prop,
      "Sub-Target",
      "Name of Parent Bone for hook (if applicable), also recalculates and clears offset");
  RNA_def_property_string_funcs(prop, NULL, NULL, "rna_HookModifier_subtarget_set");
  RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

  prop = RNA_def_property(srna, "use_falloff_uniform", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_HOOK_UNIFORM_SPACE);
  RNA_def_property_ui_text(prop, "Uniform Falloff", "Compensate for non-uniform object scale");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "name");
  RNA_def_property_ui_text(
      prop,
      "Vertex Group",
      "Name of Vertex Group which determines influence of modifier per point");
  RNA_def_property_string_funcs(prop, NULL, NULL, "rna_HookModifier_name_set");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "vertex_indices", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_array(prop, RNA_MAX_ARRAY_LENGTH);
  RNA_def_property_flag(prop, PROP_DYNAMIC);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_dynamic_array_funcs(prop, "rna_HookModifier_vertex_indices_get_length");
  RNA_def_property_int_funcs(prop, "rna_HookModifier_vertex_indices_get", NULL, NULL);
  RNA_def_property_ui_text(prop,
                           "Vertex Indices",
                           "Indices of vertices bound to the modifier. For bezier curves, "
                           "handles count as additional vertices");

  func = RNA_def_function(srna, "vertex_indices_set", "rna_HookModifier_vertex_indices_set");
  RNA_def_function_ui_description(
      func, "Validates and assigns the array of vertex indices bound to the modifier");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_int_array(
      func, "indices", 1, NULL, INT_MIN, INT_MAX, "", "Vertex Indices", 0, INT_MAX);
  RNA_def_property_array(parm, RNA_MAX_ARRAY_LENGTH);
  RNA_def_parameter_flags(parm, PROP_DYNAMIC, PARM_REQUIRED);

  prop = RNA_def_property(srna, "invert_vertex_group", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_HOOK_INVERT_VGROUP);
  RNA_def_property_ui_text(prop, "Invert", "Invert vertex group influence");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_modifier_softbody(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "SoftBodyModifier", "Modifier");
  RNA_def_struct_ui_text(srna, "Soft Body Modifier", "Soft body simulation modifier");
  RNA_def_struct_sdna(srna, "SoftbodyModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_SOFT);

  prop = RNA_def_property(srna, "settings", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_struct_type(prop, "SoftBodySettings");
  RNA_def_property_pointer_funcs(prop, "rna_SoftBodyModifier_settings_get", NULL, NULL, NULL);
  RNA_def_property_ui_text(prop, "Soft Body Settings", "");

  prop = RNA_def_property(srna, "point_cache", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_struct_type(prop, "PointCache");
  RNA_def_property_pointer_funcs(prop, "rna_SoftBodyModifier_point_cache_get", NULL, NULL, NULL);
  RNA_def_property_ui_text(prop, "Soft Body Point Cache", "");
}

static void rna_def_modifier_boolean(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem prop_operand_items[] = {
      {eBooleanModifierFlag_Object,
       "OBJECT",
       0,
       "Object",
       "Use a mesh object as the operand for the Boolean operation"},
      {eBooleanModifierFlag_Collection,
       "COLLECTION",
       0,
       "Collection",
       "Use a collection of mesh objects as the operand for the Boolean operation"},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem prop_operation_items[] = {
      {eBooleanModifierOp_Intersect,
       "INTERSECT",
       0,
       "Intersect",
       "Keep the part of the mesh that is common between all operands"},
      {eBooleanModifierOp_Union, "UNION", 0, "Union", "Combine meshes in an additive way"},
      {eBooleanModifierOp_Difference,
       "DIFFERENCE",
       0,
       "Difference",
       "Combine meshes in a subtractive way"},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem prop_solver_items[] = {
      {eBooleanModifierSolver_Fast,
       "FAST",
       0,
       "Fast",
       "Simple solver for the best performance, without support for overlapping geometry"},
      {eBooleanModifierSolver_Exact, "EXACT", 0, "Exact", "Advanced solver for the best result"},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem material_mode_items[] = {
      {eBooleanModifierMaterialMode_Index,
       "INDEX",
       0,
       "Index Based",
       "Set the material on new faces based on the order of the material slot lists. If a "
       "material doesn't exist on the modifier object, the face will use the same material slot "
       "or the first if the object doesn't have enough slots"},
      {eBooleanModifierMaterialMode_Transfer,
       "TRANSFER",
       0,
       "Transfer",
       "Transfer materials from non-empty slots to the result mesh, adding new materials as "
       "necessary. For empty slots, fall back to using the same material index as the operand "
       "mesh"},
      {0, NULL, 0, NULL, NULL},
  };

  srna = RNA_def_struct(brna, "BooleanModifier", "Modifier");
  RNA_def_struct_ui_text(srna, "Boolean Modifier", "Boolean operations modifier");
  RNA_def_struct_sdna(srna, "BooleanModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_BOOLEAN);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
  RNA_def_property_ui_text(prop, "Object", "Mesh object to use for Boolean operation");
  RNA_def_property_pointer_funcs(
      prop, NULL, "rna_BooleanModifier_object_set", NULL, "rna_Mesh_object_poll");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

  prop = RNA_def_property(srna, "collection", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "collection");
  RNA_def_property_struct_type(prop, "Collection");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_REFCOUNT);
  RNA_def_property_ui_text(
      prop, "Collection", "Use mesh objects in this collection for Boolean operation");
  RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

  prop = RNA_def_property(srna, "operation", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, prop_operation_items);
  RNA_def_property_enum_default(prop, eBooleanModifierOp_Difference);
  RNA_def_property_ui_text(prop, "Operation", "");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "operand_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, NULL, "flag");
  RNA_def_property_enum_items(prop, prop_operand_items);
  RNA_def_property_ui_text(prop, "Operand Type", "");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "double_threshold", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, NULL, "double_threshold");
  RNA_def_property_range(prop, 0, 1.0f);
  RNA_def_property_ui_range(prop, 0, 1, 1.0, 6);
  RNA_def_property_ui_scale_type(prop, PROP_SCALE_LOG);
  RNA_def_property_ui_text(
      prop, "Overlap Threshold", "Threshold for checking overlapping geometry");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "solver", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, prop_solver_items);
  RNA_def_property_enum_default(prop, eBooleanModifierSolver_Exact);
  RNA_def_property_ui_text(prop, "Solver", "Method for calculating booleans");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_self", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", eBooleanModifierFlag_Self);
  RNA_def_property_ui_text(prop, "Self Intersection", "Allow self-intersection in operands");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_hole_tolerant", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", eBooleanModifierFlag_HoleTolerant);
  RNA_def_property_ui_text(prop, "Hole Tolerant", "Better results when there are holes (slower)");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "material_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, material_mode_items);
  RNA_def_property_enum_default(prop, eBooleanModifierMaterialMode_Index);
  RNA_def_property_ui_text(prop, "Material Mode", "Method for setting materials on the new faces");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  /* BMesh debugging options, only used when G_DEBUG is set */

  /* BMesh intersection options */
  static const EnumPropertyItem debug_items[] = {
      {eBooleanModifierBMeshFlag_BMesh_Separate, "SEPARATE", 0, "Separate", ""},
      {eBooleanModifierBMeshFlag_BMesh_NoDissolve, "NO_DISSOLVE", 0, "No Dissolve", ""},
      {eBooleanModifierBMeshFlag_BMesh_NoConnectRegions,
       "NO_CONNECT_REGIONS",
       0,
       "No Connect Regions",
       ""},
      {0, NULL, 0, NULL, NULL},
  };

  prop = RNA_def_property(srna, "debug_options", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, debug_items);
  RNA_def_property_enum_sdna(prop, NULL, "bm_flag");
  RNA_def_property_flag(prop, PROP_ENUM_FLAG);
  RNA_def_property_ui_text(prop, "Debug", "Debugging options, only when started with '-d'");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_modifier_array(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem prop_fit_type_items[] = {
      {MOD_ARR_FIXEDCOUNT,
       "FIXED_COUNT",
       0,
       "Fixed Count",
       "Duplicate the object a certain number of times"},
      {MOD_ARR_FITLENGTH,
       "FIT_LENGTH",
       0,
       "Fit Length",
       "Duplicate the object as many times as fits in a certain length"},
      {MOD_ARR_FITCURVE, "FIT_CURVE", 0, "Fit Curve", "Fit the duplicated objects to a curve"},
      {0, NULL, 0, NULL, NULL},
  };

  srna = RNA_def_struct(brna, "ArrayModifier", "Modifier");
  RNA_def_struct_ui_text(srna, "Array Modifier", "Array duplication modifier");
  RNA_def_struct_sdna(srna, "ArrayModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_ARRAY);

  RNA_define_lib_overridable(true);

  /* Length parameters */
  prop = RNA_def_property(srna, "fit_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, prop_fit_type_items);
  RNA_def_property_ui_text(prop, "Fit Type", "Array length calculation method");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "count", PROP_INT, PROP_NONE);
  RNA_def_property_range(prop, 1, INT_MAX);
  RNA_def_property_ui_range(prop, 1, 1000, 1, -1);
  RNA_def_property_ui_text(prop, "Count", "Number of duplicates to make");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "fit_length", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, NULL, "length");
  RNA_def_property_range(prop, 0, INT_MAX);
  RNA_def_property_ui_range(prop, 0, 10000, 10, 2);
  RNA_def_property_ui_text(prop, "Length", "Length to fit array within");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "curve", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "curve_ob");
  RNA_def_property_ui_text(prop, "Curve", "Curve object to fit array length to");
  RNA_def_property_pointer_funcs(
      prop, NULL, "rna_ArrayModifier_curve_ob_set", NULL, "rna_Curve_object_poll");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_update(prop, 0, "rna_ArrayModifier_dependency_update");

  /* Offset parameters */
  prop = RNA_def_property(srna, "use_constant_offset", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "offset_type", MOD_ARR_OFF_CONST);
  RNA_def_property_ui_text(prop, "Constant Offset", "Add a constant offset");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "constant_offset_displace", PROP_FLOAT, PROP_TRANSLATION);
  RNA_def_property_float_sdna(prop, NULL, "offset");
  RNA_def_property_ui_text(
      prop, "Constant Offset Displacement", "Value for the distance between arrayed items");
  RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, RNA_TRANSLATION_PREC_DEFAULT);
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_relative_offset", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "offset_type", MOD_ARR_OFF_RELATIVE);
  RNA_def_property_ui_text(
      prop, "Relative Offset", "Add an offset relative to the object's bounding box");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  /* PROP_TRANSLATION causes units to be used which we don't want */
  prop = RNA_def_property(srna, "relative_offset_displace", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_float_sdna(prop, NULL, "scale");
  RNA_def_property_ui_text(
      prop,
      "Relative Offset Displacement",
      "The size of the geometry will determine the distance between arrayed items");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  /* Vertex merging parameters */
  prop = RNA_def_property(srna, "use_merge_vertices", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flags", MOD_ARR_MERGE);
  RNA_def_property_ui_text(prop, "Merge Vertices", "Merge vertices in adjacent duplicates");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_merge_vertices_cap", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flags", MOD_ARR_MERGEFINAL);
  RNA_def_property_ui_text(
      prop, "Merge End Vertices", "Merge vertices in first and last duplicates");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "merge_threshold", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, NULL, "merge_dist");
  RNA_def_property_range(prop, 0, FLT_MAX);
  RNA_def_property_ui_range(prop, 0, 1, 1, 4);
  RNA_def_property_ui_text(prop, "Merge Distance", "Limit below which to merge vertices");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  /* Offset object */
  prop = RNA_def_property(srna, "use_object_offset", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "offset_type", MOD_ARR_OFF_OBJ);
  RNA_def_property_ui_text(
      prop, "Object Offset", "Add another object's transformation to the total offset");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "offset_object", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "offset_ob");
  RNA_def_property_ui_text(
      prop,
      "Object Offset",
      "Use the location and rotation of another object to determine the distance and "
      "rotational change between arrayed items");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

  /* Caps */
  prop = RNA_def_property(srna, "start_cap", PROP_POINTER, PROP_NONE);
  RNA_def_property_ui_text(prop, "Start Cap", "Mesh object to use as a start cap");
  RNA_def_property_pointer_funcs(
      prop, NULL, "rna_ArrayModifier_start_cap_set", NULL, "rna_Mesh_object_poll");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

  prop = RNA_def_property(srna, "end_cap", PROP_POINTER, PROP_NONE);
  RNA_def_property_ui_text(prop, "End Cap", "Mesh object to use as an end cap");
  RNA_def_property_pointer_funcs(
      prop, NULL, "rna_ArrayModifier_end_cap_set", NULL, "rna_Mesh_object_poll");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

  prop = RNA_def_property(srna, "offset_u", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "uv_offset[0]");
  RNA_def_property_range(prop, -1, 1);
  RNA_def_property_ui_range(prop, -1, 1, 2, 4);
  RNA_def_property_ui_text(prop, "U Offset", "Amount to offset array UVs on the U axis");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "offset_v", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "uv_offset[1]");
  RNA_def_property_range(prop, -1, 1);
  RNA_def_property_ui_range(prop, -1, 1, 2, 4);
  RNA_def_property_ui_text(prop, "V Offset", "Amount to offset array UVs on the V axis");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_modifier_edgesplit(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "EdgeSplitModifier", "Modifier");
  RNA_def_struct_ui_text(
      srna, "EdgeSplit Modifier", "Edge splitting modifier to create sharp edges");
  RNA_def_struct_sdna(srna, "EdgeSplitModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_EDGESPLIT);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "split_angle", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_range(prop, 0.0f, DEG2RADF(180.0f));
  RNA_def_property_ui_range(prop, 0.0f, DEG2RADF(180.0f), 10, 2);
  RNA_def_property_ui_text(prop, "Split Angle", "Angle above which to split edges");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_edge_angle", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flags", MOD_EDGESPLIT_FROMANGLE);
  RNA_def_property_ui_text(prop, "Use Edge Angle", "Split edges with high angle between faces");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_edge_sharp", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flags", MOD_EDGESPLIT_FROMFLAG);
  RNA_def_property_ui_text(prop, "Use Sharp Edges", "Split edges that are marked as sharp");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_modifier_displace(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem prop_direction_items[] = {
      {MOD_DISP_DIR_X,
       "X",
       0,
       "X",
       "Use the texture's intensity value to displace in the X direction"},
      {MOD_DISP_DIR_Y,
       "Y",
       0,
       "Y",
       "Use the texture's intensity value to displace in the Y direction"},
      {MOD_DISP_DIR_Z,
       "Z",
       0,
       "Z",
       "Use the texture's intensity value to displace in the Z direction"},
      {MOD_DISP_DIR_NOR,
       "NORMAL",
       0,
       "Normal",
       "Use the texture's intensity value to displace along the vertex normal"},
      {MOD_DISP_DIR_CLNOR,
       "CUSTOM_NORMAL",
       0,
       "Custom Normal",
       "Use the texture's intensity value to displace along the (averaged) custom normal (falls "
       "back to vertex)"},
      {MOD_DISP_DIR_RGB_XYZ,
       "RGB_TO_XYZ",
       0,
       "RGB to XYZ",
       "Use the texture's RGB values to displace the mesh in the XYZ direction"},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem prop_space_items[] = {
      {MOD_DISP_SPACE_LOCAL, "LOCAL", 0, "Local", "Direction is defined in local coordinates"},
      {MOD_DISP_SPACE_GLOBAL, "GLOBAL", 0, "Global", "Direction is defined in global coordinates"},
      {0, NULL, 0, NULL, NULL},
  };

  srna = RNA_def_struct(brna, "DisplaceModifier", "Modifier");
  RNA_def_struct_ui_text(srna, "Displace Modifier", "Displacement modifier");
  RNA_def_struct_sdna(srna, "DisplaceModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_DISPLACE);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "defgrp_name");
  RNA_def_property_ui_text(
      prop,
      "Vertex Group",
      "Name of Vertex Group which determines influence of modifier per point");
  RNA_def_property_string_funcs(prop, NULL, NULL, "rna_DisplaceModifier_defgrp_name_set");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "mid_level", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "midlevel");
  RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
  RNA_def_property_ui_range(prop, 0, 1, 10, 3);
  RNA_def_property_ui_text(prop, "Midlevel", "Material value that gives no displacement");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "strength", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
  RNA_def_property_ui_range(prop, -100, 100, 10, 3);
  RNA_def_property_ui_text(prop, "Strength", "Amount to displace geometry");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "direction", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, prop_direction_items);
  RNA_def_property_ui_text(prop, "Direction", "");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "space", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, prop_space_items);
  RNA_def_property_ui_text(prop, "Space", "");
  RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

  prop = RNA_def_property(srna, "invert_vertex_group", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_DISP_INVERT_VGROUP);
  RNA_def_property_ui_text(prop, "Invert", "Invert vertex group influence");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  RNA_define_lib_overridable(false);

  rna_def_modifier_generic_map_info(srna);
}

static void rna_def_modifier_uvproject(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "UVProjectModifier", "Modifier");
  RNA_def_struct_ui_text(
      srna, "UV Project Modifier", "UV projection modifier to set UVs from a projector");
  RNA_def_struct_sdna(srna, "UVProjectModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_UVPROJECT);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "uv_layer", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "uvlayer_name");
  RNA_def_property_ui_text(prop, "UV Map", "UV map name");
  RNA_def_property_string_funcs(prop, NULL, NULL, "rna_UVProjectModifier_uvlayer_name_set");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "projector_count", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "projectors_num");
  RNA_def_property_ui_text(prop, "Number of Projectors", "Number of projectors to use");
  RNA_def_property_int_funcs(prop, NULL, "rna_UVProjectModifier_num_projectors_set", NULL);
  RNA_def_property_range(prop, 1, MOD_UVPROJECT_MAXPROJECTORS);
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "projectors", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "UVProjector");
  RNA_def_property_collection_funcs(prop,
                                    "rna_UVProject_projectors_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    NULL,
                                    NULL,
                                    NULL,
                                    NULL);
  RNA_def_property_ui_text(prop, "Projectors", "");

  prop = RNA_def_property(srna, "aspect_x", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "aspectx");
  RNA_def_property_flag(prop, PROP_PROPORTIONAL);
  RNA_def_property_range(prop, 1, FLT_MAX);
  RNA_def_property_ui_range(prop, 1, 1000, 1, 3);
  RNA_def_property_ui_text(
      prop, "Aspect X", "Horizontal aspect ratio (only used for camera projectors)");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "aspect_y", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "aspecty");
  RNA_def_property_flag(prop, PROP_PROPORTIONAL);
  RNA_def_property_range(prop, 1, FLT_MAX);
  RNA_def_property_ui_range(prop, 1, 1000, 1, 3);
  RNA_def_property_ui_text(
      prop, "Aspect Y", "Vertical aspect ratio (only used for camera projectors)");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "scale_x", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "scalex");
  RNA_def_property_flag(prop, PROP_PROPORTIONAL);
  RNA_def_property_range(prop, 0, FLT_MAX);
  RNA_def_property_ui_range(prop, 0, 1000, 1, 3);
  RNA_def_property_ui_text(prop, "Scale X", "Horizontal scale (only used for camera projectors)");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "scale_y", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "scaley");
  RNA_def_property_flag(prop, PROP_PROPORTIONAL);
  RNA_def_property_range(prop, 0, FLT_MAX);
  RNA_def_property_ui_range(prop, 0, 1000, 1, 3);
  RNA_def_property_ui_text(prop, "Scale Y", "Vertical scale (only used for camera projectors)");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  srna = RNA_def_struct(brna, "UVProjector", NULL);
  RNA_def_struct_ui_text(srna, "UVProjector", "UV projector used by the UV project modifier");

  prop = RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "Object");
  RNA_def_property_pointer_funcs(
      prop, "rna_UVProjector_object_get", "rna_UVProjector_object_set", NULL, NULL);
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_ui_text(prop, "Object", "Object to use as projector transform");
  RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_modifier_smooth(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "SmoothModifier", "Modifier");
  RNA_def_struct_ui_text(srna, "Smooth Modifier", "Smoothing effect modifier");
  RNA_def_struct_sdna(srna, "SmoothModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_SMOOTH);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "use_x", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_SMOOTH_X);
  RNA_def_property_ui_text(prop, "X", "Smooth object along X axis");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_y", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_SMOOTH_Y);
  RNA_def_property_ui_text(prop, "Y", "Smooth object along Y axis");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_z", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_SMOOTH_Z);
  RNA_def_property_ui_text(prop, "Z", "Smooth object along Z axis");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "factor", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "fac");
  RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
  RNA_def_property_ui_range(prop, -10, 10, 1, 3);
  RNA_def_property_ui_text(prop, "Factor", "Strength of modifier effect");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "iterations", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "repeat");
  RNA_def_property_ui_range(prop, 0, 30, 1, -1);
  RNA_def_property_ui_text(prop, "Repeat", "");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "defgrp_name");
  RNA_def_property_ui_text(
      prop,
      "Vertex Group",
      "Name of Vertex Group which determines influence of modifier per point");
  RNA_def_property_string_funcs(prop, NULL, NULL, "rna_SmoothModifier_defgrp_name_set");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "invert_vertex_group", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_SMOOTH_INVERT_VGROUP);
  RNA_def_property_ui_text(prop, "Invert", "Invert vertex group influence");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_modifier_correctivesmooth(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem modifier_smooth_type_items[] = {
      {MOD_CORRECTIVESMOOTH_SMOOTH_SIMPLE,
       "SIMPLE",
       0,
       "Simple",
       "Use the average of adjacent edge-vertices"},
      {MOD_CORRECTIVESMOOTH_SMOOTH_LENGTH_WEIGHT,
       "LENGTH_WEIGHTED",
       0,
       "Length Weight",
       "Use the average of adjacent edge-vertices weighted by their length"},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem modifier_rest_source_items[] = {
      {MOD_CORRECTIVESMOOTH_RESTSOURCE_ORCO,
       "ORCO",
       0,
       "Original Coords",
       "Use base mesh vertex coordinates as the rest position"},
      {MOD_CORRECTIVESMOOTH_RESTSOURCE_BIND,
       "BIND",
       0,
       "Bind Coords",
       "Use bind vertex coordinates for rest position"},
      {0, NULL, 0, NULL, NULL},
  };

  srna = RNA_def_struct(brna, "CorrectiveSmoothModifier", "Modifier");
  RNA_def_struct_ui_text(
      srna, "Corrective Smooth Modifier", "Correct distortion caused by deformation");
  RNA_def_struct_sdna(srna, "CorrectiveSmoothModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_SMOOTH);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "factor", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "lambda");
  RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0, 1.0, 5, 3);
  RNA_def_property_ui_text(prop, "Lambda Factor", "Smooth effect factor");
  RNA_def_property_update(prop, 0, "rna_CorrectiveSmoothModifier_update");

  prop = RNA_def_property(srna, "iterations", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "repeat");
  RNA_def_property_ui_range(prop, 0, 200, 1, -1);
  RNA_def_property_ui_text(prop, "Repeat", "");
  RNA_def_property_update(prop, 0, "rna_CorrectiveSmoothModifier_update");

  prop = RNA_def_property(srna, "scale", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "scale");
  RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0, 10.0, 5, 3);
  RNA_def_property_ui_text(prop, "Scale", "Compensate for scale applied by other modifiers");
  RNA_def_property_update(prop, 0, "rna_CorrectiveSmoothModifier_update");

  prop = RNA_def_property(srna, "rest_source", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "rest_source");
  RNA_def_property_enum_items(prop, modifier_rest_source_items);
  RNA_def_property_ui_text(prop, "Rest Source", "Select the source of rest positions");
  RNA_def_property_update(prop, 0, "rna_CorrectiveSmoothModifier_rest_source_update");

  prop = RNA_def_property(srna, "smooth_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "smooth_type");
  RNA_def_property_enum_items(prop, modifier_smooth_type_items);
  RNA_def_property_ui_text(prop, "Smooth Type", "Method used for smoothing");
  RNA_def_property_update(prop, 0, "rna_CorrectiveSmoothModifier_update");

  prop = RNA_def_property(srna, "invert_vertex_group", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_CORRECTIVESMOOTH_INVERT_VGROUP);
  RNA_def_property_ui_text(prop, "Invert", "Invert vertex group influence");
  RNA_def_property_update(prop, 0, "rna_CorrectiveSmoothModifier_update");

  prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "defgrp_name");
  RNA_def_property_ui_text(
      prop,
      "Vertex Group",
      "Name of Vertex Group which determines influence of modifier per point");
  RNA_def_property_string_funcs(prop, NULL, NULL, "rna_CorrectiveSmoothModifier_defgrp_name_set");
  RNA_def_property_update(prop, 0, "rna_CorrectiveSmoothModifier_update");

  prop = RNA_def_property(srna, "is_bind", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_ui_text(prop, "Bind current shape", "");
  RNA_def_property_boolean_funcs(prop, "rna_CorrectiveSmoothModifier_is_bind_get", NULL);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_only_smooth", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_CORRECTIVESMOOTH_ONLY_SMOOTH);
  RNA_def_property_ui_text(
      prop, "Only Smooth", "Apply smoothing without reconstructing the surface");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_pin_boundary", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_CORRECTIVESMOOTH_PIN_BOUNDARY);
  RNA_def_property_ui_text(
      prop, "Pin Boundaries", "Excludes boundary vertices from being smoothed");
  RNA_def_property_update(prop, 0, "rna_CorrectiveSmoothModifier_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_modifier_laplaciansmooth(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "LaplacianSmoothModifier", "Modifier");
  RNA_def_struct_ui_text(srna, "Laplacian Smooth Modifier", "Smoothing effect modifier");
  RNA_def_struct_sdna(srna, "LaplacianSmoothModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_SMOOTH);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "use_x", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_LAPLACIANSMOOTH_X);
  RNA_def_property_ui_text(prop, "X", "Smooth object along X axis");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_y", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_LAPLACIANSMOOTH_Y);
  RNA_def_property_ui_text(prop, "Y", "Smooth object along Y axis");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_z", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_LAPLACIANSMOOTH_Z);
  RNA_def_property_ui_text(prop, "Z", "Smooth object along Z axis");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_volume_preserve", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_LAPLACIANSMOOTH_PRESERVE_VOLUME);
  RNA_def_property_ui_text(prop, "Preserve Volume", "Apply volume preservation after smooth");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_normalized", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_LAPLACIANSMOOTH_NORMALIZED);
  RNA_def_property_ui_text(prop, "Normalized", "Improve and stabilize the enhanced shape");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "lambda_factor", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "lambda");
  RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
  RNA_def_property_ui_range(prop, -1000.0, 1000.0, 5, 3);
  RNA_def_property_ui_text(prop, "Lambda Factor", "Smooth effect factor");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "lambda_border", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "lambda_border");
  RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
  RNA_def_property_ui_range(prop, -1000.0, 1000.0, 5, 3);
  RNA_def_property_ui_text(prop, "Lambda Border", "Lambda factor in border");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "iterations", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "repeat");
  RNA_def_property_ui_range(prop, 0, 200, 1, -1);
  RNA_def_property_ui_text(prop, "Repeat", "");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "defgrp_name");
  RNA_def_property_ui_text(
      prop,
      "Vertex Group",
      "Name of Vertex Group which determines influence of modifier per point");
  RNA_def_property_string_funcs(prop, NULL, NULL, "rna_LaplacianSmoothModifier_defgrp_name_set");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "invert_vertex_group", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_LAPLACIANSMOOTH_INVERT_VGROUP);
  RNA_def_property_ui_text(prop, "Invert", "Invert vertex group influence");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_modifier_cast(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem prop_cast_type_items[] = {
      {MOD_CAST_TYPE_SPHERE, "SPHERE", 0, "Sphere", ""},
      {MOD_CAST_TYPE_CYLINDER, "CYLINDER", 0, "Cylinder", ""},
      {MOD_CAST_TYPE_CUBOID, "CUBOID", 0, "Cuboid", ""},
      {0, NULL, 0, NULL, NULL},
  };

  srna = RNA_def_struct(brna, "CastModifier", "Modifier");
  RNA_def_struct_ui_text(srna, "Cast Modifier", "Modifier to cast to other shapes");
  RNA_def_struct_sdna(srna, "CastModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_CAST);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "cast_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "type");
  RNA_def_property_enum_items(prop, prop_cast_type_items);
  RNA_def_property_ui_text(prop, "Shape", "Target object shape");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
  RNA_def_property_ui_text(
      prop,
      "Object",
      "Control object: if available, its location determines the center of the effect");
  RNA_def_property_pointer_funcs(prop, NULL, "rna_CastModifier_object_set", NULL, NULL);
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

  prop = RNA_def_property(srna, "invert_vertex_group", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_CAST_INVERT_VGROUP);
  RNA_def_property_ui_text(prop, "Invert", "Invert vertex group influence");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_x", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_CAST_X);
  RNA_def_property_ui_text(prop, "X", "");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_y", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_CAST_Y);
  RNA_def_property_ui_text(prop, "Y", "");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_z", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_CAST_Z);
  RNA_def_property_ui_text(prop, "Z", "");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_radius_as_size", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_CAST_SIZE_FROM_RADIUS);
  RNA_def_property_ui_text(
      prop, "Size from Radius", "Use radius as size of projection shape (0 = auto)");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_transform", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_CAST_USE_OB_TRANSFORM);
  RNA_def_property_ui_text(
      prop, "Use Transform", "Use object transform to control projection shape");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "factor", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "fac");
  RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
  RNA_def_property_ui_range(prop, -10, 10, 5, 2);
  RNA_def_property_ui_text(prop, "Factor", "");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "radius", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_range(prop, 0, FLT_MAX);
  RNA_def_property_ui_range(prop, 0, 100, 5, 2);
  RNA_def_property_ui_text(
      prop,
      "Radius",
      "Only deform vertices within this distance from the center of the effect "
      "(leave as 0 for infinite.)");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "size", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 0, FLT_MAX);
  RNA_def_property_ui_range(prop, 0, 100, 5, 2);
  RNA_def_property_ui_text(prop, "Size", "Size of projection shape (leave as 0 for auto)");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "defgrp_name");
  RNA_def_property_ui_text(prop, "Vertex Group", "Vertex group name");
  RNA_def_property_string_funcs(prop, NULL, NULL, "rna_CastModifier_defgrp_name_set");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_modifier_meshdeform(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;
#  if 0
  static const EnumPropertyItem prop_mode_items[] = {
      {0, "VOLUME", 0, "Volume", "Bind to volume inside cage mesh"},
      {1, "SURFACE", 0, "Surface", "Bind to surface of cage mesh"},
      {0, NULL, 0, NULL, NULL},
  };
#  endif

  srna = RNA_def_struct(brna, "MeshDeformModifier", "Modifier");
  RNA_def_struct_ui_text(
      srna, "MeshDeform Modifier", "Mesh deformation modifier to deform with other meshes");
  RNA_def_struct_sdna(srna, "MeshDeformModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_MESHDEFORM);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
  RNA_def_property_ui_text(prop, "Object", "Mesh object to deform with");
  RNA_def_property_pointer_funcs(
      prop, NULL, "rna_MeshDeformModifier_object_set", NULL, "rna_Mesh_object_poll");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

  prop = RNA_def_property(srna, "is_bound", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop, "rna_MeshDeformModifier_is_bound_get", NULL);
  RNA_def_property_ui_text(prop, "Bound", "Whether geometry has been bound to control cage");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_property(srna, "invert_vertex_group", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_MDEF_INVERT_VGROUP);
  RNA_def_property_ui_text(prop, "Invert", "Invert vertex group influence");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "defgrp_name");
  RNA_def_property_ui_text(prop, "Vertex Group", "Vertex group name");
  RNA_def_property_string_funcs(prop, NULL, NULL, "rna_MeshDeformModifier_defgrp_name_set");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "precision", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "gridsize");
  RNA_def_property_range(prop, 2, 10);
  RNA_def_property_ui_text(prop, "Precision", "The grid size for binding");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_dynamic_bind", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_MDEF_DYNAMIC_BIND);
  RNA_def_property_ui_text(prop,
                           "Dynamic",
                           "Recompute binding dynamically on top of other deformers "
                           "(slower and more memory consuming)");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

#  if 0
  prop = RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, prop_mode_items);
  RNA_def_property_ui_text(prop, "Mode", "Method of binding vertices are bound to cage mesh");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");
#  endif

  RNA_define_lib_overridable(false);
}

static void rna_def_modifier_particlesystem(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "ParticleSystemModifier", "Modifier");
  RNA_def_struct_ui_text(srna, "ParticleSystem Modifier", "Particle system simulation modifier");
  RNA_def_struct_sdna(srna, "ParticleSystemModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_PARTICLES);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "particle_system", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_pointer_sdna(prop, NULL, "psys");
  RNA_def_property_ui_text(prop, "Particle System", "Particle System that this modifier controls");

  RNA_define_lib_overridable(false);
}

static void rna_def_modifier_particleinstance(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static EnumPropertyItem particleinstance_space[] = {
      {eParticleInstanceSpace_Local,
       "LOCAL",
       0,
       "Local",
       "Use offset from the particle object in the instance object"},
      {eParticleInstanceSpace_World,
       "WORLD",
       0,
       "World",
       "Use world space offset in the instance object"},
      {0, NULL, 0, NULL, NULL},
  };

  srna = RNA_def_struct(brna, "ParticleInstanceModifier", "Modifier");
  RNA_def_struct_ui_text(srna, "ParticleInstance Modifier", "Particle system instancing modifier");
  RNA_def_struct_sdna(srna, "ParticleInstanceModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_PARTICLES);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "ob");
  RNA_def_property_pointer_funcs(prop, NULL, NULL, NULL, "rna_Mesh_object_poll");
  RNA_def_property_ui_text(prop, "Object", "Object that has the particle system");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

  prop = RNA_def_property(srna, "particle_system_index", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "psys");
  RNA_def_property_range(prop, 1, SHRT_MAX);
  RNA_def_property_ui_text(prop, "Particle System Number", "");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "particle_system", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "ParticleSystem");
  RNA_def_property_pointer_funcs(prop,
                                 "rna_ParticleInstanceModifier_particle_system_get",
                                 "rna_ParticleInstanceModifier_particle_system_set",
                                 NULL,
                                 "rna_ParticleInstanceModifier_particle_system_poll");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Particle System", "");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "axis", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "axis");
  RNA_def_property_enum_items(prop, rna_enum_axis_xyz_items);
  RNA_def_property_ui_text(prop, "Axis", "Pole axis for rotation");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "space", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "space");
  RNA_def_property_enum_items(prop, particleinstance_space);
  RNA_def_property_ui_text(prop, "Space", "Space to use for copying mesh data");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_normal", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", eParticleInstanceFlag_Parents);
  RNA_def_property_ui_text(prop, "Regular", "Create instances from normal particles");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_children", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", eParticleInstanceFlag_Children);
  RNA_def_property_ui_text(prop, "Children", "Create instances from child particles");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_PARTICLESETTINGS);
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_path", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", eParticleInstanceFlag_Path);
  RNA_def_property_ui_text(prop, "Path", "Create instances along particle paths");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "show_unborn", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", eParticleInstanceFlag_Unborn);
  RNA_def_property_ui_text(prop, "Unborn", "Show instances when particles are unborn");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "show_alive", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", eParticleInstanceFlag_Alive);
  RNA_def_property_ui_text(prop, "Alive", "Show instances when particles are alive");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "show_dead", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", eParticleInstanceFlag_Dead);
  RNA_def_property_ui_text(prop, "Dead", "Show instances when particles are dead");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_preserve_shape", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", eParticleInstanceFlag_KeepShape);
  RNA_def_property_ui_text(prop, "Keep Shape", "Don't stretch the object");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_size", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", eParticleInstanceFlag_UseSize);
  RNA_def_property_ui_text(prop, "Size", "Use particle size to scale the instances");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "position", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "position");
  RNA_def_property_range(prop, 0.0, 1.0);
  RNA_def_property_ui_text(prop, "Position", "Position along path");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "random_position", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "random_position");
  RNA_def_property_range(prop, 0.0, 1.0);
  RNA_def_property_ui_text(prop, "Random Position", "Randomize position along path");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "rotation", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "rotation");
  RNA_def_property_range(prop, 0.0, 1.0);
  RNA_def_property_ui_text(prop, "Rotation", "Rotation around path");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "random_rotation", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "random_rotation");
  RNA_def_property_range(prop, 0.0, 1.0);
  RNA_def_property_ui_text(prop, "Random Rotation", "Randomize rotation around path");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "particle_amount", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_range(prop, 0.0, 1.0);
  RNA_def_property_ui_text(prop, "Particle Amount", "Amount of particles to use for instancing");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "particle_offset", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_range(prop, 0.0, 1.0);
  RNA_def_property_ui_text(prop,
                           "Particle Offset",
                           "Relative offset of particles to use for instancing, to avoid overlap "
                           "of multiple instances");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "index_layer_name", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "index_layer_name");
  RNA_def_property_ui_text(prop, "Index Layer Name", "Custom data layer name for the index");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "value_layer_name", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "value_layer_name");
  RNA_def_property_ui_text(
      prop, "Value Layer Name", "Custom data layer name for the randomized value");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_modifier_explode(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "ExplodeModifier", "Modifier");
  RNA_def_struct_ui_text(
      srna, "Explode Modifier", "Explosion effect modifier based on a particle system");
  RNA_def_struct_sdna(srna, "ExplodeModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_EXPLODE);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(prop,
                                "rna_ExplodeModifier_vgroup_get",
                                "rna_ExplodeModifier_vgroup_length",
                                "rna_ExplodeModifier_vgroup_set");
  RNA_def_property_ui_text(prop, "Vertex Group", "");

  prop = RNA_def_property(srna, "protect", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 0, 1);
  RNA_def_property_ui_text(prop, "Protect", "Clean vertex group edges");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_edge_cut", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", eExplodeFlag_EdgeCut);
  RNA_def_property_ui_text(prop, "Cut Edges", "Cut face edges for nicer shrapnel");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "show_unborn", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", eExplodeFlag_Unborn);
  RNA_def_property_ui_text(prop, "Unborn", "Show mesh when particles are unborn");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "show_alive", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", eExplodeFlag_Alive);
  RNA_def_property_ui_text(prop, "Alive", "Show mesh when particles are alive");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "show_dead", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", eExplodeFlag_Dead);
  RNA_def_property_ui_text(prop, "Dead", "Show mesh when particles are dead");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_size", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", eExplodeFlag_PaSize);
  RNA_def_property_ui_text(prop, "Size", "Use particle size for the shrapnel");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "particle_uv", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "uvname");
  RNA_def_property_string_maxlength(prop, MAX_CUSTOMDATA_LAYER_NAME_NO_PREFIX);
  RNA_def_property_ui_text(prop, "Particle UV", "UV map to change with particle age");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "invert_vertex_group", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", eExplodeFlag_INVERT_VGROUP);
  RNA_def_property_ui_text(prop, "Invert", "Invert vertex group influence");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_modifier_cloth(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "ClothModifier", "Modifier");
  RNA_def_struct_ui_text(srna, "Cloth Modifier", "Cloth simulation modifier");
  RNA_def_struct_sdna(srna, "ClothModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_CLOTH);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "settings", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_pointer_sdna(prop, NULL, "sim_parms");
  RNA_def_property_ui_text(prop, "Cloth Settings", "");

  prop = RNA_def_property(srna, "collision_settings", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_pointer_sdna(prop, NULL, "coll_parms");
  RNA_def_property_ui_text(prop, "Cloth Collision Settings", "");

  prop = RNA_def_property(srna, "solver_result", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "ClothSolverResult");
  RNA_def_property_pointer_sdna(prop, NULL, "solver_result");
  RNA_def_property_ui_text(prop, "Solver Result", "");

  prop = RNA_def_property(srna, "point_cache", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_ui_text(prop, "Point Cache", "");

  prop = RNA_def_property(srna, "hair_grid_min", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "hair_grid_min");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Hair Grid Minimum", "");

  prop = RNA_def_property(srna, "hair_grid_max", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "hair_grid_max");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Hair Grid Maximum", "");

  prop = RNA_def_property(srna, "hair_grid_resolution", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "hair_grid_res");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Hair Grid Resolution", "");

  RNA_define_lib_overridable(false);
}

static void rna_def_modifier_fluid(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem prop_fluid_type_items[] = {
      {0, "NONE", 0, "None", ""},
      {MOD_FLUID_TYPE_DOMAIN, "DOMAIN", 0, "Domain", ""},
      {MOD_FLUID_TYPE_FLOW, "FLOW", 0, "Flow", "Inflow/Outflow"},
      {MOD_FLUID_TYPE_EFFEC, "EFFECTOR", 0, "Effector", ""},
      {0, NULL, 0, NULL, NULL},
  };

  srna = RNA_def_struct(brna, "FluidModifier", "Modifier");
  RNA_def_struct_ui_text(srna, "Fluid Modifier", "Fluid simulation modifier");
  RNA_def_struct_sdna(srna, "FluidModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_FLUIDSIM);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "domain_settings", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "domain");
  RNA_def_property_ui_text(prop, "Domain Settings", "");

  prop = RNA_def_property(srna, "flow_settings", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "flow");
  RNA_def_property_ui_text(prop, "Flow Settings", "");

  prop = RNA_def_property(srna, "effector_settings", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "effector");
  RNA_def_property_ui_text(prop, "Effector Settings", "");

  prop = RNA_def_property(srna, "fluid_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "type");
  RNA_def_property_enum_items(prop, prop_fluid_type_items);
  RNA_def_property_ui_text(prop, "Type", "");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, 0, "rna_fluid_set_type");

  RNA_define_lib_overridable(false);
}

static void rna_def_modifier_dynamic_paint(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "DynamicPaintModifier", "Modifier");
  RNA_def_struct_ui_text(srna, "Dynamic Paint Modifier", "Dynamic Paint modifier");
  RNA_def_struct_sdna(srna, "DynamicPaintModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_DYNAMICPAINT);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "canvas_settings", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "canvas");
  RNA_def_property_ui_text(prop, "Canvas Settings", "");

  prop = RNA_def_property(srna, "brush_settings", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "brush");
  RNA_def_property_ui_text(prop, "Brush Settings", "");

  prop = RNA_def_property(srna, "ui_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_enum_sdna(prop, NULL, "type");
  RNA_def_property_enum_items(prop, rna_enum_prop_dynamicpaint_type_items);
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_SIMULATION);
  RNA_def_property_ui_text(prop, "Type", "");

  RNA_define_lib_overridable(false);
}

static void rna_def_modifier_collision(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "CollisionModifier", "Modifier");
  RNA_def_struct_ui_text(srna,
                         "Collision Modifier",
                         "Collision modifier defining modifier stack position used for collision");
  RNA_def_struct_sdna(srna, "CollisionModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_PHYSICS);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "settings", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_struct_type(prop, "CollisionSettings");
  RNA_def_property_pointer_funcs(prop, "rna_CollisionModifier_settings_get", NULL, NULL, NULL);
  RNA_def_property_ui_text(prop, "Settings", "");

  RNA_define_lib_overridable(false);
}

static void rna_def_modifier_bevel(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem prop_limit_method_items[] = {
      {0, "NONE", 0, "None", "Bevel the entire mesh by a constant amount"},
      {MOD_BEVEL_ANGLE,
       "ANGLE",
       0,
       "Angle",
       "Only bevel edges with sharp enough angles between faces"},
      {MOD_BEVEL_WEIGHT,
       "WEIGHT",
       0,
       "Weight",
       "Use bevel weights to determine how much bevel is applied in edge mode"},
      {MOD_BEVEL_VGROUP,
       "VGROUP",
       0,
       "Vertex Group",
       "Use vertex group weights to select whether vertex or edge is beveled"},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem prop_val_type_items[] = {
      {MOD_BEVEL_AMT_OFFSET, "OFFSET", 0, "Offset", "Amount is offset of new edges from original"},
      {MOD_BEVEL_AMT_WIDTH, "WIDTH", 0, "Width", "Amount is width of new face"},
      {MOD_BEVEL_AMT_DEPTH,
       "DEPTH",
       0,
       "Depth",
       "Amount is perpendicular distance from original edge to bevel face"},
      {MOD_BEVEL_AMT_PERCENT,
       "PERCENT",
       0,
       "Percent",
       "Amount is percent of adjacent edge length"},
      {MOD_BEVEL_AMT_ABSOLUTE,
       "ABSOLUTE",
       0,
       "Absolute",
       "Amount is absolute distance along adjacent edge"},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem prop_profile_type_items[] = {
      {MOD_BEVEL_PROFILE_SUPERELLIPSE,
       "SUPERELLIPSE",
       0,
       "Superellipse",
       "The profile can be a concave or convex curve"},
      {MOD_BEVEL_PROFILE_CUSTOM,
       "CUSTOM",
       0,
       "Custom",
       "The profile can be any arbitrary path between its endpoints"},
      {0, NULL, 0, NULL, NULL},
  };

  static EnumPropertyItem prop_harden_normals_items[] = {
      {MOD_BEVEL_FACE_STRENGTH_NONE, "FSTR_NONE", 0, "None", "Do not set face strength"},
      {MOD_BEVEL_FACE_STRENGTH_NEW, "FSTR_NEW", 0, "New", "Set face strength on new faces only"},
      {MOD_BEVEL_FACE_STRENGTH_AFFECTED,
       "FSTR_AFFECTED",
       0,
       "Affected",
       "Set face strength on new and affected faces only"},
      {MOD_BEVEL_FACE_STRENGTH_ALL, "FSTR_ALL", 0, "All", "Set face strength on all faces"},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem prop_miter_outer_items[] = {
      {MOD_BEVEL_MITER_SHARP, "MITER_SHARP", 0, "Sharp", "Outside of miter is sharp"},
      {MOD_BEVEL_MITER_PATCH, "MITER_PATCH", 0, "Patch", "Outside of miter is squared-off patch"},
      {MOD_BEVEL_MITER_ARC, "MITER_ARC", 0, "Arc", "Outside of miter is arc"},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem prop_miter_inner_items[] = {
      {MOD_BEVEL_MITER_SHARP, "MITER_SHARP", 0, "Sharp", "Inside of miter is sharp"},
      {MOD_BEVEL_MITER_ARC, "MITER_ARC", 0, "Arc", "Inside of miter is arc"},
      {0, NULL, 0, NULL, NULL},
  };

  static EnumPropertyItem prop_vmesh_method_items[] = {
      {MOD_BEVEL_VMESH_ADJ, "ADJ", 0, "Grid Fill", "Default patterned fill"},
      {MOD_BEVEL_VMESH_CUTOFF,
       "CUTOFF",
       0,
       "Cutoff",
       "A cut-off at the end of each profile before the intersection"},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem prop_affect_items[] = {
      {MOD_BEVEL_AFFECT_VERTICES, "VERTICES", 0, "Vertices", "Affect only vertices"},
      {MOD_BEVEL_AFFECT_EDGES, "EDGES", 0, "Edges", "Affect only edges"},
      {0, NULL, 0, NULL, NULL},
  };

  srna = RNA_def_struct(brna, "BevelModifier", "Modifier");
  RNA_def_struct_ui_text(
      srna, "Bevel Modifier", "Bevel modifier to make edges and vertices more rounded");
  RNA_def_struct_sdna(srna, "BevelModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_BEVEL);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "width", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, NULL, "value");
  RNA_def_property_range(prop, 0, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0f, 100.0f, 0.1, 4);
  RNA_def_property_ui_text(prop, "Width", "Bevel amount");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "width_pct", PROP_FLOAT, PROP_PERCENTAGE);
  RNA_def_property_float_sdna(prop, NULL, "value");
  RNA_def_property_range(prop, 0, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0f, 100.0f, 5.0, 2);
  RNA_def_property_ui_text(prop, "Width Percent", "Bevel amount for percentage method");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "segments", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "res");
  RNA_def_property_range(prop, 1, 1000);
  RNA_def_property_ui_range(prop, 1, 100, 1, -1);
  RNA_def_property_ui_text(prop, "Segments", "Number of segments for round edges/verts");
  RNA_def_property_update(prop, 0, "rna_BevelModifier_update_segments");

  prop = RNA_def_property(srna, "affect", PROP_ENUM, PROP_NONE); /* as an enum */
  RNA_def_property_enum_sdna(prop, NULL, "affect_type");
  RNA_def_property_enum_items(prop, prop_affect_items);
  RNA_def_property_ui_text(prop, "Affect", "Affect edges or vertices");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "limit_method", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "lim_flags");
  RNA_def_property_enum_items(prop, prop_limit_method_items);
  RNA_def_property_ui_text(prop, "Limit Method", "");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "angle_limit", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, NULL, "bevel_angle");
  RNA_def_property_range(prop, 0.0f, DEG2RADF(180.0f));
  RNA_def_property_ui_range(prop, 0.0f, DEG2RADF(180.0f), 10, 2);
  RNA_def_property_ui_text(prop, "Angle", "Angle above which to bevel edges");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "defgrp_name");
  RNA_def_property_ui_text(prop, "Vertex Group", "Vertex group name");
  RNA_def_property_string_funcs(prop, NULL, NULL, "rna_BevelModifier_defgrp_name_set");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "invert_vertex_group", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flags", MOD_BEVEL_INVERT_VGROUP);
  RNA_def_property_ui_text(prop, "Invert", "Invert vertex group influence");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_clamp_overlap", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, NULL, "flags", MOD_BEVEL_OVERLAP_OK);
  RNA_def_property_ui_text(prop, "Clamp Overlap", "Clamp the width to avoid overlap");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "offset_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "val_flags");
  RNA_def_property_enum_items(prop, prop_val_type_items);
  RNA_def_property_ui_text(prop, "Width Type", "What distance Width measures");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "profile_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "profile_type");
  RNA_def_property_enum_items(prop, prop_profile_type_items);
  RNA_def_property_ui_text(
      prop, "Profile Type", "The type of shape used to rebuild a beveled section");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "profile", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.05, 2);
  RNA_def_property_ui_text(prop, "Profile", "The profile shape (0.5 = round)");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "material", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "mat");
  RNA_def_property_range(prop, -1, SHRT_MAX);
  RNA_def_property_ui_text(
      prop, "Material Index", "Material index of generated faces, -1 for automatic");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "loop_slide", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, NULL, "flags", MOD_BEVEL_EVEN_WIDTHS);
  RNA_def_property_ui_text(prop, "Loop Slide", "Prefer sliding along edges to having even widths");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "mark_seam", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "edge_flags", MOD_BEVEL_MARK_SEAM);
  RNA_def_property_ui_text(prop, "Mark Seams", "Mark Seams along beveled edges");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "mark_sharp", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "edge_flags", MOD_BEVEL_MARK_SHARP);
  RNA_def_property_ui_text(prop, "Mark Sharp", "Mark beveled edges as sharp");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "harden_normals", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flags", MOD_BEVEL_HARDEN_NORMALS);
  RNA_def_property_ui_text(prop, "Harden Normals", "Match normals of new faces to adjacent faces");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "face_strength_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "face_str_mode");
  RNA_def_property_enum_items(prop, prop_harden_normals_items);
  RNA_def_property_ui_text(
      prop, "Face Strength", "Whether to set face strength, and which faces to set it on");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "miter_outer", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "miter_outer");
  RNA_def_property_enum_items(prop, prop_miter_outer_items);
  RNA_def_property_ui_text(prop, "Outer Miter", "Pattern to use for outside of miters");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "miter_inner", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "miter_inner");
  RNA_def_property_enum_items(prop, prop_miter_inner_items);
  RNA_def_property_ui_text(prop, "Inner Miter", "Pattern to use for inside of miters");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "spread", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, NULL, "spread");
  RNA_def_property_range(prop, 0, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0f, 100.0f, 0.1, 4);
  RNA_def_property_ui_text(prop, "Spread", "Spread distance for inner miter arcs");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "custom_profile", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "CurveProfile");
  RNA_def_property_pointer_sdna(prop, NULL, "custom_profile");
  RNA_def_property_ui_text(prop, "Custom Profile Path", "The path for the custom profile");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "vmesh_method", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "vmesh_method");
  RNA_def_property_enum_items(prop, prop_vmesh_method_items);
  RNA_def_property_ui_text(
      prop, "Vertex Mesh Method", "The method to use to create the mesh at intersections");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_modifier_shrinkwrap(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "ShrinkwrapModifier", "Modifier");
  RNA_def_struct_ui_text(srna,
                         "Shrinkwrap Modifier",
                         "Shrink wrapping modifier to shrink wrap and object to a target");
  RNA_def_struct_sdna(srna, "ShrinkwrapModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_SHRINKWRAP);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "wrap_method", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "shrinkType");
  RNA_def_property_enum_items(prop, rna_enum_shrinkwrap_type_items);
  RNA_def_property_ui_text(prop, "Wrap Method", "");
  RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

  prop = RNA_def_property(srna, "wrap_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "shrinkMode");
  RNA_def_property_enum_items(prop, rna_enum_modifier_shrinkwrap_mode_items);
  RNA_def_property_ui_text(
      prop, "Snap Mode", "Select how vertices are constrained to the target surface");
  RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

  prop = RNA_def_property(srna, "cull_face", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "shrinkOpts");
  RNA_def_property_enum_items(prop, rna_enum_shrinkwrap_face_cull_items);
  RNA_def_property_enum_funcs(
      prop, "rna_ShrinkwrapModifier_face_cull_get", "rna_ShrinkwrapModifier_face_cull_set", NULL);
  RNA_def_property_ui_text(
      prop,
      "Face Cull",
      "Stop vertices from projecting to a face on the target when facing towards/away");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "target", PROP_POINTER, PROP_NONE);
  RNA_def_property_ui_text(prop, "Target", "Mesh target to shrink to");
  RNA_def_property_pointer_funcs(
      prop, NULL, "rna_ShrinkwrapModifier_target_set", NULL, "rna_Mesh_object_poll");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

  prop = RNA_def_property(srna, "auxiliary_target", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "auxTarget");
  RNA_def_property_ui_text(prop, "Auxiliary Target", "Additional mesh target to shrink to");
  RNA_def_property_pointer_funcs(
      prop, NULL, "rna_ShrinkwrapModifier_auxTarget_set", NULL, "rna_Mesh_object_poll");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

  prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "vgroup_name");
  RNA_def_property_ui_text(prop, "Vertex Group", "Vertex group name");
  RNA_def_property_string_funcs(prop, NULL, NULL, "rna_ShrinkwrapModifier_vgroup_name_set");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "offset", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, NULL, "keepDist");
  RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
  RNA_def_property_ui_range(prop, -100, 100, 1, 2);
  RNA_def_property_ui_text(prop, "Offset", "Distance to keep from the target");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "project_limit", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, NULL, "projLimit");
  RNA_def_property_range(prop, 0.0, FLT_MAX);
  RNA_def_property_ui_range(prop, 0, 100, 1, 2);
  RNA_def_property_ui_text(
      prop, "Project Limit", "Limit the distance used for projection (zero disables)");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_project_x", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "projAxis", MOD_SHRINKWRAP_PROJECT_OVER_X_AXIS);
  RNA_def_property_ui_text(prop, "X", "");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_project_y", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "projAxis", MOD_SHRINKWRAP_PROJECT_OVER_Y_AXIS);
  RNA_def_property_ui_text(prop, "Y", "");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_project_z", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "projAxis", MOD_SHRINKWRAP_PROJECT_OVER_Z_AXIS);
  RNA_def_property_ui_text(prop, "Z", "");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "subsurf_levels", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "subsurfLevels");
  RNA_def_property_range(prop, 0, 6);
  RNA_def_property_ui_range(prop, 0, 6, 1, -1);
  RNA_def_property_ui_text(
      prop,
      "Subdivision Levels",
      "Number of subdivisions that must be performed before extracting vertices' "
      "positions and normals");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_negative_direction", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "shrinkOpts", MOD_SHRINKWRAP_PROJECT_ALLOW_NEG_DIR);
  RNA_def_property_ui_text(
      prop, "Negative", "Allow vertices to move in the negative direction of axis");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_positive_direction", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "shrinkOpts", MOD_SHRINKWRAP_PROJECT_ALLOW_POS_DIR);
  RNA_def_property_ui_text(
      prop, "Positive", "Allow vertices to move in the positive direction of axis");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_invert_cull", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "shrinkOpts", MOD_SHRINKWRAP_INVERT_CULL_TARGET);
  RNA_def_property_ui_text(
      prop, "Invert Cull", "When projecting in the negative direction invert the face cull mode");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "invert_vertex_group", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "shrinkOpts", MOD_SHRINKWRAP_INVERT_VGROUP);
  RNA_def_property_ui_text(prop, "Invert", "Invert vertex group influence");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_modifier_mask(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem modifier_mask_mode_items[] = {
      {MOD_MASK_MODE_VGROUP, "VERTEX_GROUP", 0, "Vertex Group", ""},
      {MOD_MASK_MODE_ARM, "ARMATURE", 0, "Armature", ""},
      {0, NULL, 0, NULL, NULL},
  };

  srna = RNA_def_struct(brna, "MaskModifier", "Modifier");
  RNA_def_struct_ui_text(srna, "Mask Modifier", "Mask modifier to hide parts of the mesh");
  RNA_def_struct_sdna(srna, "MaskModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_MASK);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, modifier_mask_mode_items);
  RNA_def_property_ui_text(prop, "Mode", "");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "armature", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "ob_arm");
  RNA_def_property_ui_text(prop, "Armature", "Armature to use as source of bones to mask");
  RNA_def_property_pointer_funcs(
      prop, NULL, "rna_MaskModifier_ob_arm_set", NULL, "rna_Armature_object_poll");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

  prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "vgroup");
  RNA_def_property_ui_text(prop, "Vertex Group", "Vertex group name");
  RNA_def_property_string_funcs(prop, NULL, NULL, "rna_MaskModifier_vgroup_set");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "invert_vertex_group", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_MASK_INV);
  RNA_def_property_ui_text(prop, "Invert", "Use vertices that are not part of region defined");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_smooth", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_MASK_SMOOTH);
  RNA_def_property_ui_text(
      prop, "Smooth", "Use vertex group weights to cut faces at the weight contour");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "threshold", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "threshold");
  RNA_def_property_range(prop, 0.0, 1.0);
  RNA_def_property_ui_range(prop, 0, 1, 0.1, 3);
  RNA_def_property_ui_text(prop, "Threshold", "Weights over this threshold remain");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_modifier_simpledeform(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem simple_deform_mode_items[] = {
      {MOD_SIMPLEDEFORM_MODE_TWIST,
       "TWIST",
       0,
       "Twist",
       "Rotate around the Z axis of the modifier space"},
      {MOD_SIMPLEDEFORM_MODE_BEND,
       "BEND",
       0,
       "Bend",
       "Bend the mesh over the Z axis of the modifier space"},
      {MOD_SIMPLEDEFORM_MODE_TAPER,
       "TAPER",
       0,
       "Taper",
       "Linearly scale along Z axis of the modifier space"},
      {MOD_SIMPLEDEFORM_MODE_STRETCH,
       "STRETCH",
       0,
       "Stretch",
       "Stretch the object along the Z axis of the modifier space"},
      {0, NULL, 0, NULL, NULL},
  };

  srna = RNA_def_struct(brna, "SimpleDeformModifier", "Modifier");
  RNA_def_struct_ui_text(
      srna,
      "SimpleDeform Modifier",
      "Simple deformation modifier to apply effects such as twisting and bending");
  RNA_def_struct_sdna(srna, "SimpleDeformModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_SIMPLEDEFORM);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "deform_method", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "mode");
  RNA_def_property_enum_items(prop, simple_deform_mode_items);
  RNA_def_property_ui_text(prop, "Mode", "");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_OPERATOR_DEFAULT);
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "vgroup_name");
  RNA_def_property_ui_text(prop, "Vertex Group", "Vertex group name");
  RNA_def_property_string_funcs(prop, NULL, NULL, "rna_SimpleDeformModifier_vgroup_name_set");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "deform_axis", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_axis_xyz_items);
  RNA_def_property_ui_text(prop, "Axis", "Deform around local axis");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "origin", PROP_POINTER, PROP_NONE);
  RNA_def_property_ui_text(prop, "Origin", "Offset the origin and orientation of the deformation");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

  prop = RNA_def_property(srna, "factor", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
  RNA_def_property_ui_range(prop, -10.0, 10.0, 1.0, 3);
  RNA_def_property_ui_text(prop, "Factor", "Amount to deform object");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "angle", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, NULL, "factor");
  RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
  RNA_def_property_ui_range(prop, DEG2RAD(-360.0), DEG2RAD(360.0), 10.0, 3);
  RNA_def_property_ui_text(prop, "Angle", "Angle of deformation");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "limits", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "limit");
  RNA_def_property_array(prop, 2);
  RNA_def_property_range(prop, 0, 1);
  RNA_def_property_ui_range(prop, 0, 1, 5, 2);
  RNA_def_property_ui_text(prop, "Limits", "Lower/Upper limits for deform");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "lock_x", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "axis", MOD_SIMPLEDEFORM_LOCK_AXIS_X);
  RNA_def_property_ui_text(prop, "X", "Do not allow deformation along the X axis");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "lock_y", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "axis", MOD_SIMPLEDEFORM_LOCK_AXIS_Y);
  RNA_def_property_ui_text(prop, "Y", "Do not allow deformation along the Y axis");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "lock_z", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "axis", MOD_SIMPLEDEFORM_LOCK_AXIS_Z);
  RNA_def_property_ui_text(prop, "Z", "Do not allow deformation along the Z axis");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "invert_vertex_group", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_SIMPLEDEFORM_FLAG_INVERT_VGROUP);
  RNA_def_property_ui_text(prop, "Invert", "Invert vertex group influence");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_modifier_surface(BlenderRNA *brna)
{
  StructRNA *srna;

  srna = RNA_def_struct(brna, "SurfaceModifier", "Modifier");
  RNA_def_struct_ui_text(
      srna,
      "Surface Modifier",
      "Surface modifier defining modifier stack position used for surface fields");
  RNA_def_struct_sdna(srna, "SurfaceModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_PHYSICS);
}

static void rna_def_modifier_solidify(BlenderRNA *brna)
{
  static const EnumPropertyItem mode_items[] = {
      {MOD_SOLIDIFY_MODE_EXTRUDE,
       "EXTRUDE",
       0,
       "Simple",
       "Output a solidified version of a mesh by simple extrusion"},
      {MOD_SOLIDIFY_MODE_NONMANIFOLD,
       "NON_MANIFOLD",
       0,
       "Complex",
       "Output a manifold mesh even if the base mesh is non-manifold, "
       "where edges have 3 or more connecting faces. "
       "This method is slower"},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem nonmanifold_thickness_mode_items[] = {
      {MOD_SOLIDIFY_NONMANIFOLD_OFFSET_MODE_FIXED,
       "FIXED",
       0,
       "Fixed",
       "Most basic thickness calculation"},
      {MOD_SOLIDIFY_NONMANIFOLD_OFFSET_MODE_EVEN,
       "EVEN",
       0,
       "Even",
       "Even thickness calculation which takes the angle between faces into account"},
      {MOD_SOLIDIFY_NONMANIFOLD_OFFSET_MODE_CONSTRAINTS,
       "CONSTRAINTS",
       0,
       "Constraints",
       "Thickness calculation using constraints, most advanced"},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem nonmanifold_boundary_mode_items[] = {
      {MOD_SOLIDIFY_NONMANIFOLD_BOUNDARY_MODE_NONE, "NONE", 0, "None", "No shape correction"},
      {MOD_SOLIDIFY_NONMANIFOLD_BOUNDARY_MODE_ROUND,
       "ROUND",
       0,
       "Round",
       "Round open perimeter shape"},
      {MOD_SOLIDIFY_NONMANIFOLD_BOUNDARY_MODE_FLAT,
       "FLAT",
       0,
       "Flat",
       "Flat open perimeter shape"},
      {0, NULL, 0, NULL, NULL},
  };

  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "SolidifyModifier", "Modifier");
  RNA_def_struct_ui_text(
      srna, "Solidify Modifier", "Create a solid skin, compensating for sharp angles");
  RNA_def_struct_sdna(srna, "SolidifyModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_SOLIDIFY);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "solidify_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "mode");
  RNA_def_property_enum_items(prop, mode_items);
  RNA_def_property_ui_text(prop, "Mode", "Selects the used algorithm");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "thickness", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, NULL, "offset");
  RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
  RNA_def_property_ui_range(prop, -10, 10, 0.1, 4);
  RNA_def_property_ui_text(prop, "Thickness", "Thickness of the shell");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "thickness_clamp", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "offset_clamp");
  RNA_def_property_range(prop, 0, 100.0);
  RNA_def_property_ui_range(prop, 0, 2.0, 0.1, 4);
  RNA_def_property_ui_text(prop, "Clamp", "Offset clamp based on geometry scale");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_thickness_angle_clamp", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_SOLIDIFY_OFFSET_ANGLE_CLAMP);
  RNA_def_property_ui_text(prop, "Angle Clamp", "Clamp thickness based on angles");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "thickness_vertex_group", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "offset_fac_vg");
  RNA_def_property_range(prop, 0.0, 1.0);
  RNA_def_property_ui_range(prop, 0, 1, 0.1, 3);
  RNA_def_property_ui_text(
      prop, "Vertex Group Factor", "Thickness factor to use for zero vertex group influence");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "offset", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "offset_fac");
  RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
  RNA_def_property_ui_range(prop, -1, 1, 0.1, 4);
  RNA_def_property_ui_text(prop, "Offset", "Offset the thickness from the center");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "edge_crease_inner", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "crease_inner");
  RNA_def_property_range(prop, 0, 1);
  RNA_def_property_ui_range(prop, 0, 1, 0.1, 3);
  RNA_def_property_ui_text(prop, "Inner Crease", "Assign a crease to inner edges");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "edge_crease_outer", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "crease_outer");
  RNA_def_property_range(prop, 0, 1);
  RNA_def_property_ui_range(prop, 0, 1, 0.1, 3);
  RNA_def_property_ui_text(prop, "Outer Crease", "Assign a crease to outer edges");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "edge_crease_rim", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "crease_rim");
  RNA_def_property_range(prop, 0, 1);
  RNA_def_property_ui_range(prop, 0, 1, 0.1, 3);
  RNA_def_property_ui_text(prop, "Rim Crease", "Assign a crease to the edges making up the rim");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "material_offset", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "mat_ofs");
  RNA_def_property_range(prop, SHRT_MIN, SHRT_MAX);
  RNA_def_property_ui_text(prop, "Material Offset", "Offset material index of generated faces");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "material_offset_rim", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "mat_ofs_rim");
  RNA_def_property_range(prop, SHRT_MIN, SHRT_MAX);
  RNA_def_property_ui_text(
      prop, "Rim Material Offset", "Offset material index of generated rim faces");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "defgrp_name");
  RNA_def_property_ui_text(prop, "Vertex Group", "Vertex group name");
  RNA_def_property_string_funcs(prop, NULL, NULL, "rna_SolidifyModifier_defgrp_name_set");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "shell_vertex_group", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "shell_defgrp_name");
  RNA_def_property_ui_text(prop,
                           "Shell Vertex Group",
                           "Vertex group that the generated shell geometry will be weighted to");
  RNA_def_property_string_funcs(prop, NULL, NULL, "rna_SolidifyModifier_shell_defgrp_name_set");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "rim_vertex_group", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "rim_defgrp_name");
  RNA_def_property_ui_text(prop,
                           "Rim Vertex Group",
                           "Vertex group that the generated rim geometry will be weighted to");
  RNA_def_property_string_funcs(prop, NULL, NULL, "rna_SolidifyModifier_rim_defgrp_name_set");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_rim", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_SOLIDIFY_RIM);
  RNA_def_property_ui_text(prop,
                           "Fill Rim",
                           "Create edge loops between the inner and outer surfaces on face edges "
                           "(slow, disable when not needed)");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_even_offset", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_SOLIDIFY_EVEN);
  RNA_def_property_ui_text(
      prop,
      "Even Thickness",
      "Maintain thickness by adjusting for sharp corners (slow, disable when not needed)");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_quality_normals", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_SOLIDIFY_NORMAL_CALC);
  RNA_def_property_ui_text(
      prop,
      "High Quality Normals",
      "Calculate normals which result in more even thickness (slow, disable when not needed)");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "invert_vertex_group", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_SOLIDIFY_VGROUP_INV);
  RNA_def_property_ui_text(prop, "Vertex Group Invert", "Invert the vertex group influence");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_flat_faces", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_SOLIDIFY_NONMANIFOLD_FLAT_FACES);
  RNA_def_property_ui_text(prop,
                           "Flat Faces",
                           "Make faces use the minimal vertex weight assigned to their vertices "
                           "(ensures new faces remain parallel to their original ones, slow, "
                           "disable when not needed)");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_flip_normals", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_SOLIDIFY_FLIP);
  RNA_def_property_ui_text(prop, "Flip Normals", "Invert the face direction");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_rim_only", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_SOLIDIFY_NOSHELL);
  RNA_def_property_ui_text(prop, "Only Rim", "Only add the rim to the original data");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  /* Settings for #MOD_SOLIDIFY_MODE_NONMANIFOLD */
  prop = RNA_def_property(srna, "nonmanifold_thickness_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "nonmanifold_offset_mode");
  RNA_def_property_enum_items(prop, nonmanifold_thickness_mode_items);
  RNA_def_property_ui_text(prop, "Thickness Mode", "Selects the used thickness algorithm");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "nonmanifold_boundary_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, nonmanifold_boundary_mode_items);
  RNA_def_property_ui_text(prop, "Boundary Shape", "Selects the boundary adjustment algorithm");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "nonmanifold_merge_threshold", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, NULL, "merge_tolerance");
  RNA_def_property_range(prop, 0.0, 1.0);
  RNA_def_property_ui_range(prop, 0.0, 1.0, 0.01, 4);
  RNA_def_property_ui_text(
      prop, "Merge Threshold", "Distance within which degenerated geometry is merged");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "bevel_convex", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "bevel_convex");
  RNA_def_property_range(prop, -1.0, 1.0);
  RNA_def_property_ui_range(prop, -1.0, 1.0, 0.1, 3);
  RNA_def_property_ui_text(prop, "Bevel Convex", "Edge bevel weight to be added to outside edges");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_modifier_screw(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "ScrewModifier", "Modifier");
  RNA_def_struct_ui_text(srna, "Screw Modifier", "Revolve edges");
  RNA_def_struct_sdna(srna, "ScrewModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_SCREW);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "ob_axis");
  RNA_def_property_ui_text(prop, "Object", "Object to define the screw axis");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

  prop = RNA_def_property(srna, "steps", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_range(prop, 1, 10000);
  RNA_def_property_ui_range(prop, 1, 512, 1, -1);
  RNA_def_property_ui_text(prop, "Steps", "Number of steps in the revolution");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "render_steps", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_range(prop, 1, 10000);
  RNA_def_property_ui_range(prop, 1, 512, 1, -1);
  RNA_def_property_ui_text(prop, "Render Steps", "Number of steps in the revolution");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "iterations", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, NULL, "iter");
  RNA_def_property_range(prop, 1, 10000);
  RNA_def_property_ui_range(prop, 1, 100, 1, -1);
  RNA_def_property_ui_text(prop, "Iterations", "Number of times to apply the screw operation");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "axis", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_axis_xyz_items);
  RNA_def_property_ui_text(prop, "Axis", "Screw axis");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "angle", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_ui_range(prop, -M_PI * 2, M_PI * 2, 10, -1);
  RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
  RNA_def_property_ui_text(prop, "Angle", "Angle of revolution");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "screw_offset", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, NULL, "screw_ofs");
  RNA_def_property_ui_text(prop, "Screw", "Offset the revolution along its axis");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "merge_threshold", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, NULL, "merge_dist");
  RNA_def_property_range(prop, 0, FLT_MAX);
  RNA_def_property_ui_range(prop, 0, 1, 1, 4);
  RNA_def_property_ui_text(prop, "Merge Distance", "Limit below which to merge vertices");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_normal_flip", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_SCREW_NORMAL_FLIP);
  RNA_def_property_ui_text(prop, "Flip", "Flip normals of lathed faces");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_normal_calculate", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_SCREW_NORMAL_CALC);
  RNA_def_property_ui_text(
      prop, "Calculate Order", "Calculate the order of edges (needed for meshes, but not curves)");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_object_screw_offset", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_SCREW_OBJECT_OFFSET);
  RNA_def_property_ui_text(
      prop, "Object Screw", "Use the distance between the objects to make a screw");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  /* Vertex merging parameters */
  prop = RNA_def_property(srna, "use_merge_vertices", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_SCREW_MERGE);
  RNA_def_property_ui_text(
      prop, "Merge Vertices", "Merge adjacent vertices (screw offset must be zero)");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_smooth_shade", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_SCREW_SMOOTH_SHADING);
  RNA_def_property_ui_text(
      prop, "Smooth Shading", "Output faces with smooth shading rather than flat shaded");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_stretch_u", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_SCREW_UV_STRETCH_U);
  RNA_def_property_ui_text(
      prop, "Stretch U", "Stretch the U coordinates between 0 and 1 when UVs are present");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_stretch_v", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_SCREW_UV_STRETCH_V);
  RNA_def_property_ui_text(
      prop, "Stretch V", "Stretch the V coordinates between 0 and 1 when UVs are present");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

#  if 0
  prop = RNA_def_property(srna, "use_angle_object", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_SCREW_OBJECT_ANGLE);
  RNA_def_property_ui_text(
      prop, "Object Angle", "Use the angle between the objects rather than the fixed angle");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");
#  endif

  RNA_define_lib_overridable(false);
}

static void rna_def_modifier_uvwarp(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "UVWarpModifier", "Modifier");
  RNA_def_struct_ui_text(srna, "UVWarp Modifier", "Add target position to UV coordinates");
  RNA_def_struct_sdna(srna, "UVWarpModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_UVPROJECT);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "axis_u", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "axis_u");
  RNA_def_property_enum_items(prop, rna_enum_axis_xyz_items);
  RNA_def_property_ui_text(prop, "U-Axis", "Pole axis for rotation");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "axis_v", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "axis_v");
  RNA_def_property_enum_items(prop, rna_enum_axis_xyz_items);
  RNA_def_property_ui_text(prop, "V-Axis", "Pole axis for rotation");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "center", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "center");
  RNA_def_property_ui_text(prop, "UV Center", "Center point for rotate/scale");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "offset", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "offset");
  RNA_def_property_ui_text(prop, "Offset", "2D Offset for the warp");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "scale", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "scale");
  RNA_def_property_ui_text(prop, "Scale", "2D Scale for the warp");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "rotation", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, NULL, "rotation");
  RNA_def_property_ui_text(prop, "Rotation", "2D Rotation for the warp");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "object_from", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "object_src");
  RNA_def_property_ui_text(prop, "Object From", "Object defining offset");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

  prop = RNA_def_property(srna, "bone_from", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "bone_src");
  RNA_def_property_ui_text(prop, "Bone From", "Bone defining offset");
  RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

  prop = RNA_def_property(srna, "object_to", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "object_dst");
  RNA_def_property_ui_text(prop, "Object To", "Object defining offset");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

  prop = RNA_def_property(srna, "bone_to", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "bone_dst");
  RNA_def_property_ui_text(prop, "Bone To", "Bone defining offset");
  RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

  prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "vgroup_name");
  RNA_def_property_ui_text(prop, "Vertex Group", "Vertex group name");
  RNA_def_property_string_funcs(prop, NULL, NULL, "rna_UVWarpModifier_vgroup_name_set");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "invert_vertex_group", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_UVWARP_INVERT_VGROUP);
  RNA_def_property_ui_text(prop, "Invert", "Invert vertex group influence");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "uv_layer", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "uvlayer_name");
  RNA_def_property_ui_text(prop, "UV Layer", "UV Layer name");
  RNA_def_property_string_funcs(prop, NULL, NULL, "rna_UVWarpModifier_uvlayer_name_set");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_modifier_weightvg_mask(BlenderRNA *UNUSED(brna),
                                           StructRNA *srna,
                                           const char *mask_flags,
                                           const int invert_vgroup_mask_flag,
                                           const char *mask_vgroup_setter,
                                           const char *mask_uvlayer_setter)
{
  static const EnumPropertyItem weightvg_mask_tex_map_items[] = {
      {MOD_DISP_MAP_LOCAL, "LOCAL", 0, "Local", "Use local generated coordinates"},
      {MOD_DISP_MAP_GLOBAL, "GLOBAL", 0, "Global", "Use global coordinates"},
      {MOD_DISP_MAP_OBJECT,
       "OBJECT",
       0,
       "Object",
       "Use local generated coordinates of another object"},
      {MOD_DISP_MAP_UV, "UV", 0, "UV", "Use coordinates from a UV layer"},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem weightvg_mask_tex_used_items[] = {
      {MOD_WVG_MASK_TEX_USE_INT, "INT", 0, "Intensity", ""},
      {MOD_WVG_MASK_TEX_USE_RED, "RED", 0, "Red", ""},
      {MOD_WVG_MASK_TEX_USE_GREEN, "GREEN", 0, "Green", ""},
      {MOD_WVG_MASK_TEX_USE_BLUE, "BLUE", 0, "Blue", ""},
      {MOD_WVG_MASK_TEX_USE_HUE, "HUE", 0, "Hue", ""},
      {MOD_WVG_MASK_TEX_USE_SAT, "SAT", 0, "Saturation", ""},
      {MOD_WVG_MASK_TEX_USE_VAL, "VAL", 0, "Value", ""},
      {MOD_WVG_MASK_TEX_USE_ALPHA, "ALPHA", 0, "Alpha", ""},
      {0, NULL, 0, NULL, NULL},
  };

  PropertyRNA *prop;

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "mask_constant", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0, 1.0, 1, -1);
  RNA_def_property_ui_text(
      prop, "Influence", "Global influence of current modifications on vgroup");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "mask_vertex_group", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "mask_defgrp_name");
  RNA_def_property_ui_text(prop, "Mask Vertex Group", "Masking vertex group name");
  RNA_def_property_string_funcs(prop, NULL, NULL, mask_vgroup_setter);
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "invert_mask_vertex_group", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, mask_flags, invert_vgroup_mask_flag);
  RNA_def_property_ui_text(prop, "Invert", "Invert vertex group mask influence");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "mask_texture", PROP_POINTER, PROP_NONE);
  RNA_def_property_ui_text(prop, "Masking Tex", "Masking texture");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

  prop = RNA_def_property(srna, "mask_tex_use_channel", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, weightvg_mask_tex_used_items);
  RNA_def_property_ui_text(prop, "Use Channel", "Which texture channel to use for masking");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "mask_tex_mapping", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, weightvg_mask_tex_map_items);
  RNA_def_property_ui_text(prop,
                           "Texture Coordinates",
                           "Which texture coordinates "
                           "to use for mapping");
  RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

  prop = RNA_def_property(srna, "mask_tex_uv_layer", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "mask_tex_uvlayer_name");
  RNA_def_property_ui_text(prop, "UV Map", "UV map name");
  RNA_def_property_string_funcs(prop, NULL, NULL, mask_uvlayer_setter);
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "mask_tex_map_object", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "mask_tex_map_obj");
  RNA_def_property_ui_text(prop,
                           "Texture Coordinate Object",
                           "Which object to take texture "
                           "coordinates from");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

  prop = RNA_def_property(srna, "mask_tex_map_bone", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "mask_tex_map_bone");
  RNA_def_property_ui_text(
      prop, "Texture Coordinate Bone", "Which bone to take texture coordinates from");
  RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_modifier_weightvgedit(BlenderRNA *brna)
{
  static const EnumPropertyItem weightvg_edit_falloff_type_items[] = {
      {MOD_WVG_MAPPING_NONE, "LINEAR", ICON_LINCURVE, "Linear", "Null action"},
      {MOD_WVG_MAPPING_CURVE, "CURVE", ICON_RNDCURVE, "Custom Curve", ""},
      {MOD_WVG_MAPPING_SHARP, "SHARP", ICON_SHARPCURVE, "Sharp", ""},
      {MOD_WVG_MAPPING_SMOOTH, "SMOOTH", ICON_SMOOTHCURVE, "Smooth", ""},
      {MOD_WVG_MAPPING_ROOT, "ROOT", ICON_ROOTCURVE, "Root", ""},
      {MOD_WVG_MAPPING_SPHERE, "ICON_SPHERECURVE", ICON_SPHERECURVE, "Sphere", ""},
      {MOD_WVG_MAPPING_RANDOM, "RANDOM", ICON_RNDCURVE, "Random", ""},
      {MOD_WVG_MAPPING_STEP,
       "STEP",
       ICON_IPO_CONSTANT,
       "Median Step",
       "Map all values below 0.5 to 0.0, and all others to 1.0"},
      {0, NULL, 0, NULL, NULL},
  };

  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "VertexWeightEditModifier", "Modifier");
  RNA_def_struct_ui_text(
      srna, "WeightVG Edit Modifier", "Edit the weights of vertices in a group");
  RNA_def_struct_sdna(srna, "WeightVGEditModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_VERTEX_WEIGHT);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "defgrp_name");
  RNA_def_property_ui_text(prop, "Vertex Group", "Vertex group name");
  RNA_def_property_string_funcs(prop, NULL, NULL, "rna_WeightVGEditModifier_defgrp_name_set");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "falloff_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, weightvg_edit_falloff_type_items);
  RNA_def_property_ui_text(prop, "Falloff Type", "How weights are mapped to their new values");
  RNA_def_property_translation_context(prop,
                                       BLT_I18NCONTEXT_ID_CURVE_LEGACY); /* Abusing id_curve :/ */
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "invert_falloff", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "edit_flags", MOD_WVG_INVERT_FALLOFF);
  RNA_def_property_ui_text(prop, "Invert Falloff", "Invert the resulting falloff weight");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "normalize", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "edit_flags", MOD_WVG_EDIT_WEIGHTS_NORMALIZE);
  RNA_def_property_ui_text(
      prop,
      "Normalize Weights",
      "Normalize the resulting weights (otherwise they are only clamped within 0.0 to 1.0 range)");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "map_curve", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "cmap_curve");
  RNA_def_property_ui_text(prop, "Mapping Curve", "Custom mapping curve");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_add", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "edit_flags", MOD_WVG_EDIT_ADD2VG);
  RNA_def_property_ui_text(prop,
                           "Group Add",
                           "Add vertices with weight over threshold "
                           "to vgroup");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_remove", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "edit_flags", MOD_WVG_EDIT_REMFVG);
  RNA_def_property_ui_text(prop,
                           "Group Remove",
                           "Remove vertices with weight below threshold "
                           "from vgroup");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "default_weight", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_range(prop, 0.0, 1.0f);
  RNA_def_property_ui_range(prop, 0.0, 1.0, 1, -1);
  RNA_def_property_ui_text(prop,
                           "Default Weight",
                           "Default weight a vertex will have if "
                           "it is not in the vgroup");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "add_threshold", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "add_threshold");
  RNA_def_property_range(prop, 0.0, 1.0);
  RNA_def_property_ui_range(prop, 0.0, 1.0, 1, -1);
  RNA_def_property_ui_text(prop,
                           "Add Threshold",
                           "Lower bound for a vertex's weight "
                           "to be added to the vgroup");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "remove_threshold", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "rem_threshold");
  RNA_def_property_range(prop, 0.0, 1.0);
  RNA_def_property_ui_range(prop, 0.0, 1.0, 1, -1);
  RNA_def_property_ui_text(prop,
                           "Remove Threshold",
                           "Upper bound for a vertex's weight "
                           "to be removed from the vgroup");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  RNA_define_lib_overridable(false);

  /* Common masking properties. */
  rna_def_modifier_weightvg_mask(brna,
                                 srna,
                                 "edit_flags",
                                 MOD_WVG_EDIT_INVERT_VGROUP_MASK,
                                 "rna_WeightVGEditModifier_mask_defgrp_name_set",
                                 "rna_WeightVGEditModifier_mask_tex_uvlayer_name_set");
}

static void rna_def_modifier_weightvgmix(BlenderRNA *brna)
{
  static const EnumPropertyItem weightvg_mix_modes_items[] = {
      {MOD_WVG_MIX_SET, "SET", 0, "Replace", "Replace VGroup A's weights by VGroup B's ones"},
      {MOD_WVG_MIX_ADD, "ADD", 0, "Add", "Add VGroup B's weights to VGroup A's ones"},
      {MOD_WVG_MIX_SUB, "SUB", 0, "Subtract", "Subtract VGroup B's weights from VGroup A's ones"},
      {MOD_WVG_MIX_MUL, "MUL", 0, "Multiply", "Multiply VGroup A's weights by VGroup B's ones"},
      {MOD_WVG_MIX_DIV, "DIV", 0, "Divide", "Divide VGroup A's weights by VGroup B's ones"},
      {MOD_WVG_MIX_DIF,
       "DIF",
       0,
       "Difference",
       "Difference between VGroup A's and VGroup B's weights"},
      {MOD_WVG_MIX_AVG, "AVG", 0, "Average", "Average value of VGroup A's and VGroup B's weights"},
      {MOD_WVG_MIX_MIN, "MIN", 0, "Minimum", "Minimum of VGroup A's and VGroup B's weights"},
      {MOD_WVG_MIX_MAX, "MAX", 0, "Maximum", "Maximum of VGroup A's and VGroup B's weights"},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem weightvg_mix_set_items[] = {
      {MOD_WVG_SET_ALL, "ALL", 0, "All", "Affect all vertices (might add some to VGroup A)"},
      {MOD_WVG_SET_A, "A", 0, "VGroup A", "Affect vertices in VGroup A"},
      {MOD_WVG_SET_B,
       "B",
       0,
       "VGroup B",
       "Affect vertices in VGroup B (might add some to VGroup A)"},
      {MOD_WVG_SET_OR,
       "OR",
       0,
       "VGroup A or B",
       "Affect vertices in at least one of both VGroups (might add some to VGroup A)"},
      {MOD_WVG_SET_AND, "AND", 0, "VGroup A and B", "Affect vertices in both groups"},
      {0, NULL, 0, NULL, NULL},
  };

  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "VertexWeightMixModifier", "Modifier");
  RNA_def_struct_ui_text(srna, "WeightVG Mix Modifier", "Mix the weights of two vertex groups");
  RNA_def_struct_sdna(srna, "WeightVGMixModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_VERTEX_WEIGHT);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "vertex_group_a", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "defgrp_name_a");
  RNA_def_property_ui_text(prop, "Vertex Group A", "First vertex group name");
  RNA_def_property_string_funcs(prop, NULL, NULL, "rna_WeightVGMixModifier_defgrp_name_a_set");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "vertex_group_b", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "defgrp_name_b");
  RNA_def_property_ui_text(prop, "Vertex Group B", "Second vertex group name");
  RNA_def_property_string_funcs(prop, NULL, NULL, "rna_WeightVGMixModifier_defgrp_name_b_set");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "invert_vertex_group_a", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_WVG_MIX_INVERT_VGROUP_A);
  RNA_def_property_ui_text(prop, "Invert Weights A", "Invert the influence of vertex group A");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "invert_vertex_group_b", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_WVG_MIX_INVERT_VGROUP_B);
  RNA_def_property_ui_text(prop, "Invert Weights B", "Invert the influence of vertex group B");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "default_weight_a", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_range(prop, 0.0, 1.0f);
  RNA_def_property_ui_range(prop, 0.0, 1.0, 1, -1);
  RNA_def_property_ui_text(prop,
                           "Default Weight A",
                           "Default weight a vertex will have if "
                           "it is not in the first A vgroup");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "default_weight_b", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_range(prop, 0.0, 1.0f);
  RNA_def_property_ui_range(prop, 0.0, 1.0, 1, -1);
  RNA_def_property_ui_text(prop,
                           "Default Weight B",
                           "Default weight a vertex will have if "
                           "it is not in the second B vgroup");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "mix_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, weightvg_mix_modes_items);
  RNA_def_property_ui_text(prop,
                           "Mix Mode",
                           "How weights from vgroup B affect weights "
                           "of vgroup A");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "mix_set", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, weightvg_mix_set_items);
  RNA_def_property_ui_text(prop, "Vertex Set", "Which vertices should be affected");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "normalize", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_WVG_MIX_WEIGHTS_NORMALIZE);
  RNA_def_property_ui_text(
      prop,
      "Normalize Weights",
      "Normalize the resulting weights (otherwise they are only clamped within 0.0 to 1.0 range)");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  RNA_define_lib_overridable(false);

  /* Common masking properties. */
  rna_def_modifier_weightvg_mask(brna,
                                 srna,
                                 "flag",
                                 MOD_WVG_MIX_INVERT_VGROUP_MASK,
                                 "rna_WeightVGMixModifier_mask_defgrp_name_set",
                                 "rna_WeightVGMixModifier_mask_tex_uvlayer_name_set");
}

static void rna_def_modifier_weightvgproximity(BlenderRNA *brna)
{
  static const EnumPropertyItem weightvg_proximity_modes_items[] = {
      {MOD_WVG_PROXIMITY_OBJECT,
       "OBJECT",
       0,
       "Object",
       "Use distance between affected and target objects"},
      {MOD_WVG_PROXIMITY_GEOMETRY,
       "GEOMETRY",
       0,
       "Geometry",
       "Use distance between affected object's vertices and target "
       "object, or target object's geometry"},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem proximity_geometry_items[] = {
      {MOD_WVG_PROXIMITY_GEOM_VERTS, "VERTEX", 0, "Vertex", "Compute distance to nearest vertex"},
      {MOD_WVG_PROXIMITY_GEOM_EDGES, "EDGE", 0, "Edge", "Compute distance to nearest edge"},
      {MOD_WVG_PROXIMITY_GEOM_FACES, "FACE", 0, "Face", "Compute distance to nearest face"},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem weightvg_proximity_falloff_type_items[] = {
      {MOD_WVG_MAPPING_NONE, "LINEAR", ICON_LINCURVE, "Linear", "Null action"},
      {MOD_WVG_MAPPING_CURVE, "CURVE", ICON_RNDCURVE, "Custom Curve", ""},
      {MOD_WVG_MAPPING_SHARP, "SHARP", ICON_SHARPCURVE, "Sharp", ""},
      {MOD_WVG_MAPPING_SMOOTH, "SMOOTH", ICON_SMOOTHCURVE, "Smooth", ""},
      {MOD_WVG_MAPPING_ROOT, "ROOT", ICON_ROOTCURVE, "Root", ""},
      {MOD_WVG_MAPPING_SPHERE, "ICON_SPHERECURVE", ICON_SPHERECURVE, "Sphere", ""},
      {MOD_WVG_MAPPING_RANDOM, "RANDOM", ICON_RNDCURVE, "Random", ""},
      {MOD_WVG_MAPPING_STEP,
       "STEP",
       ICON_IPO_CONSTANT,
       "Median Step",
       "Map all values below 0.5 to 0.0, and all others to 1.0"},
      {0, NULL, 0, NULL, NULL},
  };

  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "VertexWeightProximityModifier", "Modifier");
  RNA_def_struct_ui_text(srna,
                         "WeightVG Proximity Modifier",
                         "Set the weights of vertices in a group from a target object's "
                         "distance");
  RNA_def_struct_sdna(srna, "WeightVGProximityModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_VERTEX_WEIGHT);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "defgrp_name");
  RNA_def_property_ui_text(prop, "Vertex Group", "Vertex group name");
  RNA_def_property_string_funcs(prop, NULL, NULL, "rna_WeightVGProximityModifier_defgrp_name_set");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "proximity_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, weightvg_proximity_modes_items);
  RNA_def_property_enum_default(prop, MOD_WVG_PROXIMITY_GEOMETRY);
  RNA_def_property_ui_text(prop, "Proximity Mode", "Which distances to target object to use");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "proximity_geometry", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "proximity_flags");
  RNA_def_property_enum_items(prop, proximity_geometry_items);
  RNA_def_property_flag(prop, PROP_ENUM_FLAG); /* important to run before default set */
  RNA_def_property_enum_default(prop, MOD_WVG_PROXIMITY_GEOM_FACES);
  RNA_def_property_ui_text(prop,
                           "Proximity Geometry",
                           "Use the shortest computed distance to target object's geometry "
                           "as weight");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "target", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "proximity_ob_target");
  RNA_def_property_ui_text(prop, "Target Object", "Object to calculate vertices distances from");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

  prop = RNA_def_property(srna, "min_dist", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_range(prop, 0.0, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0, 1000.0, 10, -1);
  RNA_def_property_ui_text(prop, "Lowest", "Distance mapping to weight 0.0");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "max_dist", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_range(prop, 0.0, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0, 1000.0, 10, -1);
  RNA_def_property_ui_text(prop, "Highest", "Distance mapping to weight 1.0");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "falloff_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, weightvg_proximity_falloff_type_items);
  RNA_def_property_ui_text(prop, "Falloff Type", "How weights are mapped to their new values");
  RNA_def_property_translation_context(prop,
                                       BLT_I18NCONTEXT_ID_CURVE_LEGACY); /* Abusing id_curve :/ */
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "invert_falloff", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "proximity_flags", MOD_WVG_PROXIMITY_INVERT_FALLOFF);
  RNA_def_property_ui_text(prop, "Invert Falloff", "Invert the resulting falloff weight");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "normalize", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, NULL, "proximity_flags", MOD_WVG_PROXIMITY_WEIGHTS_NORMALIZE);
  RNA_def_property_ui_text(
      prop,
      "Normalize Weights",
      "Normalize the resulting weights (otherwise they are only clamped within 0.0 to 1.0 range)");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "map_curve", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "cmap_curve");
  RNA_def_property_ui_text(prop, "Mapping Curve", "Custom mapping curve");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  RNA_define_lib_overridable(false);

  /* Common masking properties. */
  rna_def_modifier_weightvg_mask(brna,
                                 srna,
                                 "proximity_flags",
                                 MOD_WVG_PROXIMITY_INVERT_VGROUP_MASK,
                                 "rna_WeightVGProximityModifier_mask_defgrp_name_set",
                                 "rna_WeightVGProximityModifier_mask_tex_uvlayer_name_set");
}

static void rna_def_modifier_remesh(BlenderRNA *brna)
{
  static const EnumPropertyItem mode_items[] = {
      {MOD_REMESH_CENTROID, "BLOCKS", 0, "Blocks", "Output a blocky surface with no smoothing"},
      {MOD_REMESH_MASS_POINT,
       "SMOOTH",
       0,
       "Smooth",
       "Output a smooth surface with no sharp-features detection"},
      {MOD_REMESH_SHARP_FEATURES,
       "SHARP",
       0,
       "Sharp",
       "Output a surface that reproduces sharp edges and corners from the input mesh"},
      {MOD_REMESH_VOXEL,
       "VOXEL",
       0,
       "Voxel",
       "Output a mesh corresponding to the volume of the original mesh"},
      {0, NULL, 0, NULL, NULL},
  };

  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "RemeshModifier", "Modifier");
  RNA_def_struct_ui_text(
      srna,
      "Remesh Modifier",
      "Generate a new surface with regular topology that follows the shape of the input mesh");
  RNA_def_struct_sdna(srna, "RemeshModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_REMESH);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, mode_items);
  RNA_def_property_ui_text(prop, "Mode", "");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "scale", PROP_FLOAT, PROP_NONE);
  RNA_def_property_ui_range(prop, 0, 0.99, 0.01, 3);
  RNA_def_property_range(prop, 0, 0.99);
  RNA_def_property_ui_text(
      prop, "Scale", "The ratio of the largest dimension of the model over the size of the grid");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "threshold", PROP_FLOAT, PROP_NONE);
  RNA_def_property_ui_range(prop, 0, 1, 0.1, 3);
  RNA_def_property_range(prop, 0, 1);
  RNA_def_property_ui_text(
      prop,
      "Threshold",
      "If removing disconnected pieces, minimum size of components to preserve as a ratio "
      "of the number of polygons in the largest component");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "octree_depth", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "depth");
  RNA_def_property_range(prop, 1, 24);
  RNA_def_property_ui_range(prop, 1, 12, 1, 3);
  RNA_def_property_ui_text(
      prop, "Octree Depth", "Resolution of the octree; higher values give finer details");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "sharpness", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "hermite_num");
  RNA_def_property_ui_range(prop, 0, 2, 0.1, 3);
  RNA_def_property_ui_text(
      prop,
      "Sharpness",
      "Tolerance for outliers; lower values filter noise while higher values will reproduce "
      "edges closer to the input");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "voxel_size", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, NULL, "voxel_size");
  RNA_def_property_range(prop, 0.0001f, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0001, 2, 0.1, 3);
  RNA_def_property_ui_scale_type(prop, PROP_SCALE_LOG);
  RNA_def_property_ui_text(prop,
                           "Voxel Size",
                           "Size of the voxel in object space used for volume evaluation. Lower "
                           "values preserve finer details");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "adaptivity", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, NULL, "adaptivity");
  RNA_def_property_ui_range(prop, 0, 1, 0.1, 3);
  RNA_def_property_ui_text(
      prop,
      "Adaptivity",
      "Reduces the final face count by simplifying geometry where detail is not needed, "
      "generating triangles. A value greater than 0 disables Fix Poles");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_remove_disconnected", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_REMESH_FLOOD_FILL);
  RNA_def_property_ui_text(prop, "Remove Disconnected", "");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_smooth_shade", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_REMESH_SMOOTH_SHADING);
  RNA_def_property_ui_text(
      prop, "Smooth Shading", "Output faces with smooth shading rather than flat shaded");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_modifier_ocean(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem geometry_items[] = {
    {MOD_OCEAN_GEOM_GENERATE,
     "GENERATE",
     0,
     "Generate",
     "Generate ocean surface geometry at the specified resolution"},
    {MOD_OCEAN_GEOM_DISPLACE,
     "DISPLACE",
     0,
     "Displace",
     "Displace existing geometry according to simulation"},
#  if 0
    {MOD_OCEAN_GEOM_SIM_ONLY,
     "SIM_ONLY",
     0,
     "Sim Only",
     "Leaves geometry unchanged, but still runs simulation (to be used from texture)"},
#  endif
    {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem spectrum_items[] = {
      {MOD_OCEAN_SPECTRUM_PHILLIPS,
       "PHILLIPS",
       0,
       "Turbulent Ocean",
       "Use for turbulent seas with foam"},
      {MOD_OCEAN_SPECTRUM_PIERSON_MOSKOWITZ,
       "PIERSON_MOSKOWITZ",
       0,
       "Established Ocean",
       "Use for a large area, established ocean (Pierson-Moskowitz method)"},
      {MOD_OCEAN_SPECTRUM_JONSWAP,
       "JONSWAP",
       0,
       "Established Ocean (Sharp Peaks)",
       "Use for established oceans ('JONSWAP', Pierson-Moskowitz method) with peak sharpening"},
      {MOD_OCEAN_SPECTRUM_TEXEL_MARSEN_ARSLOE,
       "TEXEL_MARSEN_ARSLOE",
       0,
       "Shallow Water",
       "Use for shallow water ('JONSWAP', 'TMA' - Texel-Marsen-Arsloe method)"},
      {0, NULL, 0, NULL, NULL},
  };

  srna = RNA_def_struct(brna, "OceanModifier", "Modifier");
  RNA_def_struct_ui_text(srna, "Ocean Modifier", "Simulate an ocean surface");
  RNA_def_struct_sdna(srna, "OceanModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_OCEAN);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "geometry_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "geometry_mode");
  RNA_def_property_enum_items(prop, geometry_items);
  RNA_def_property_ui_text(prop, "Geometry", "Method of modifying geometry");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "size", PROP_FLOAT, PROP_UNSIGNED);
  RNA_def_property_float_sdna(prop, NULL, "size");
  RNA_def_property_ui_text(
      prop, "Size", "Surface scale factor (does not affect the height of the waves)");
  RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, -1);
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "repeat_x", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, NULL, "repeat_x");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_range(prop, 1, 1024);
  RNA_def_property_ui_range(prop, 1, 100, 1, -1);
  RNA_def_property_ui_text(prop, "Repeat X", "Repetitions of the generated surface in X");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "repeat_y", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, NULL, "repeat_y");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_range(prop, 1, 1024);
  RNA_def_property_ui_range(prop, 1, 100, 1, -1);
  RNA_def_property_ui_text(prop, "Repeat Y", "Repetitions of the generated surface in Y");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_normals", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_OCEAN_GENERATE_NORMALS);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop,
      "Generate Normals",
      "Output normals for bump mapping - disabling can speed up performance if it's not needed");
  RNA_def_property_update(prop, 0, "rna_OceanModifier_init_update");

  prop = RNA_def_property(srna, "use_foam", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_OCEAN_GENERATE_FOAM);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Generate Foam", "Generate foam mask as a vertex color channel");
  RNA_def_property_update(prop, 0, "rna_OceanModifier_init_update");

  prop = RNA_def_property(srna, "use_spray", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_OCEAN_GENERATE_SPRAY);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop, "Generate Spray Map", "Generate map of spray direction as a vertex color channel");
  RNA_def_property_update(prop, 0, "rna_OceanModifier_init_update");

  prop = RNA_def_property(srna, "invert_spray", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_OCEAN_INVERT_SPRAY);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Invert Spray", "Invert the spray direction map");
  RNA_def_property_update(prop, 0, "rna_OceanModifier_init_update");

  prop = RNA_def_property(srna, "spray_layer_name", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "spraylayername");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop, "Spray Map", "Name of the vertex color layer used for the spray direction map");
  RNA_def_property_update(prop, 0, "rna_OceanModifier_init_update");

  prop = RNA_def_property(srna, "resolution", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, NULL, "resolution");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_range(prop, 1, 1024);
  RNA_def_property_ui_range(prop, 1, 32, 1, -1);
  RNA_def_property_ui_text(
      prop, "Render Resolution", "Resolution of the generated surface for rendering and baking");
  RNA_def_property_update(prop, 0, "rna_OceanModifier_init_update");

  prop = RNA_def_property(srna, "viewport_resolution", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, NULL, "viewport_resolution");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_range(prop, 1, 1024);
  RNA_def_property_ui_range(prop, 1, 32, 1, -1);
  RNA_def_property_ui_text(
      prop, "Viewport Resolution", "Viewport resolution of the generated surface");
  RNA_def_property_update(prop, 0, "rna_OceanModifier_init_update");

  prop = RNA_def_property(srna, "spatial_size", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "spatial_size");
  RNA_def_property_ui_range(prop, 1, 512, 2, -1);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop,
      "Spatial Size",
      "Size of the simulation domain (in meters), and of the generated geometry (in BU)");
  RNA_def_property_update(prop, 0, "rna_OceanModifier_init_update");

  prop = RNA_def_property(srna, "wind_velocity", PROP_FLOAT, PROP_VELOCITY);
  RNA_def_property_float_sdna(prop, NULL, "wind_velocity");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Wind Velocity", "Wind speed");
  RNA_def_property_update(prop, 0, "rna_OceanModifier_init_update");

  prop = RNA_def_property(srna, "damping", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "damp");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop, "Damping", "Damp reflected waves going in opposite direction to the wind");
  RNA_def_property_update(prop, 0, "rna_OceanModifier_init_update");

  prop = RNA_def_property(srna, "wave_scale_min", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, NULL, "smallest_wave");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_range(prop, 0.0, FLT_MAX);
  RNA_def_property_ui_text(prop, "Smallest Wave", "Shortest allowed wavelength");
  RNA_def_property_update(prop, 0, "rna_OceanModifier_init_update");

  prop = RNA_def_property(srna, "wave_alignment", PROP_FLOAT, PROP_UNSIGNED);
  RNA_def_property_float_sdna(prop, NULL, "wave_alignment");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_range(prop, 0.0, 1.0);
  RNA_def_property_ui_text(prop, "Wave Alignment", "How much the waves are aligned to each other");
  RNA_def_property_update(prop, 0, "rna_OceanModifier_init_update");

  prop = RNA_def_property(srna, "wave_direction", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, NULL, "wave_direction");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop, "Wave Direction", "Main direction of the waves when they are (partially) aligned");
  RNA_def_property_update(prop, 0, "rna_OceanModifier_init_update");

  prop = RNA_def_property(srna, "wave_scale", PROP_FLOAT, PROP_UNSIGNED);
  RNA_def_property_float_sdna(prop, NULL, "wave_scale");
  RNA_def_property_ui_text(prop, "Wave Scale", "Scale of the displacement effect");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "depth", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, NULL, "depth");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Depth", "Depth of the solid ground below the water surface");
  RNA_def_property_ui_range(prop, 0, 250, 1, -1);
  RNA_def_property_update(prop, 0, "rna_OceanModifier_init_update");

  prop = RNA_def_property(srna, "foam_coverage", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "foam_coverage");
  RNA_def_property_ui_text(prop, "Foam Coverage", "Amount of generated foam");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "bake_foam_fade", PROP_FLOAT, PROP_UNSIGNED);
  RNA_def_property_float_sdna(prop, NULL, "foam_fade");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop, "Foam Fade", "How much foam accumulates over time (baked ocean only)");
  RNA_def_property_ui_range(prop, 0.0, 10.0, 1, -1);
  RNA_def_property_update(prop, 0, NULL);

  prop = RNA_def_property(srna, "foam_layer_name", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "foamlayername");
  RNA_def_property_ui_text(
      prop, "Foam Layer Name", "Name of the vertex color layer used for foam");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "choppiness", PROP_FLOAT, PROP_UNSIGNED);
  RNA_def_property_float_sdna(prop, NULL, "chop_amount");
  RNA_def_property_ui_text(
      prop,
      "Choppiness",
      "Choppiness of the wave's crest (adds some horizontal component to the displacement)");
  RNA_def_property_ui_range(prop, 0.0, 4.0, 3, -1);
  RNA_def_property_float_funcs(prop, NULL, "rna_OceanModifier_ocean_chop_set", NULL);
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "time", PROP_FLOAT, PROP_UNSIGNED);
  RNA_def_property_float_sdna(prop, NULL, "time");
  RNA_def_property_ui_text(prop, "Time", "Current time of the simulation");
  RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, -1);
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "spectrum", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "spectrum");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_enum_items(prop, spectrum_items);
  RNA_def_property_ui_text(prop, "Spectrum", "Spectrum to use");
  RNA_def_property_update(prop, 0, "rna_OceanModifier_init_update");

  prop = RNA_def_property(srna, "fetch_jonswap", PROP_FLOAT, PROP_UNSIGNED);
  RNA_def_property_float_sdna(prop, NULL, "fetch_jonswap");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_range(prop, 0.0, FLT_MAX);
  RNA_def_property_ui_text(
      prop,
      "Fetch",
      "This is the distance from a lee shore, "
      "called the fetch, or the distance over which the wind blows with constant velocity. "
      "Used by 'JONSWAP' and 'TMA' models");
  RNA_def_property_update(prop, 0, "rna_OceanModifier_init_update");

  prop = RNA_def_property(srna, "sharpen_peak_jonswap", PROP_FLOAT, PROP_UNSIGNED);
  RNA_def_property_float_sdna(prop, NULL, "sharpen_peak_jonswap");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_range(prop, 0.0, 1.0);
  RNA_def_property_ui_text(prop, "Sharpen peak", "Peak sharpening for 'JONSWAP' and 'TMA' models");
  RNA_def_property_update(prop, 0, "rna_OceanModifier_init_update");

  prop = RNA_def_property(srna, "random_seed", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, NULL, "seed");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Random Seed", "Seed of the random generator");
  RNA_def_property_update(prop, 0, "rna_OceanModifier_init_update");

  prop = RNA_def_property(srna, "frame_start", PROP_INT, PROP_TIME);
  RNA_def_property_int_sdna(prop, NULL, "bakestart");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Bake Start", "Start frame of the ocean baking");
  RNA_def_property_update(prop, 0, "rna_OceanModifier_init_update");

  prop = RNA_def_property(srna, "frame_end", PROP_INT, PROP_TIME);
  RNA_def_property_int_sdna(prop, NULL, "bakeend");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Bake End", "End frame of the ocean baking");
  RNA_def_property_update(prop, 0, "rna_OceanModifier_init_update");

  prop = RNA_def_property(srna, "is_cached", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "cached", 1);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop, "Ocean is Cached", "Whether the ocean is using cached data or simulating");

  prop = RNA_def_property(srna, "filepath", PROP_STRING, PROP_DIRPATH);
  RNA_def_property_string_sdna(prop, NULL, "cachepath");
  RNA_def_property_ui_text(prop, "Cache Path", "Path to a folder to store external baked images");
  // RNA_def_property_update(prop, 0, "rna_Modifier_update");
  /* XXX how to update? */

  RNA_define_lib_overridable(false);
}

static void rna_def_modifier_skin(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "SkinModifier", "Modifier");
  RNA_def_struct_ui_text(srna, "Skin Modifier", "Generate Skin");
  RNA_def_struct_sdna(srna, "SkinModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_SKIN);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "branch_smoothing", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_ui_text(prop, "Branch Smoothing", "Smooth complex geometry around branches");
  RNA_def_property_ui_range(prop, 0, 1, 1, -1);
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_smooth_shade", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_SKIN_SMOOTH_SHADING);
  RNA_def_property_ui_text(
      prop, "Smooth Shading", "Output faces with smooth shading rather than flat shaded");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_x_symmetry", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "symmetry_axes", MOD_SKIN_SYMM_X);
  RNA_def_property_ui_text(prop, "X", "Avoid making unsymmetrical quads across the X axis");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_y_symmetry", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "symmetry_axes", MOD_SKIN_SYMM_Y);
  RNA_def_property_ui_text(prop, "Y", "Avoid making unsymmetrical quads across the Y axis");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_z_symmetry", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "symmetry_axes", MOD_SKIN_SYMM_Z);
  RNA_def_property_ui_text(prop, "Z", "Avoid making unsymmetrical quads across the Z axis");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_modifier_triangulate(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "TriangulateModifier", "Modifier");
  RNA_def_struct_ui_text(srna, "Triangulate Modifier", "Triangulate Mesh");
  RNA_def_struct_sdna(srna, "TriangulateModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_TRIANGULATE);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "quad_method", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "quad_method");
  RNA_def_property_enum_items(prop, rna_enum_modifier_triangulate_quad_method_items);
  RNA_def_property_ui_text(prop, "Quad Method", "Method for splitting the quads into triangles");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "ngon_method", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "ngon_method");
  RNA_def_property_enum_items(prop, rna_enum_modifier_triangulate_ngon_method_items);
  RNA_def_property_ui_text(prop, "N-gon Method", "Method for splitting the n-gons into triangles");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "min_vertices", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, NULL, "min_vertices");
  RNA_def_property_range(prop, 4, INT_MAX);
  RNA_def_property_ui_text(
      prop,
      "Minimum Vertices",
      "Triangulate only polygons with vertex count greater than or equal to this number");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "keep_custom_normals", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_TRIANGULATE_KEEP_CUSTOMLOOP_NORMALS);
  RNA_def_property_ui_text(
      prop,
      "Keep Normals",
      "Try to preserve custom normals.\n"
      "Warning: Depending on chosen triangulation method, "
      "shading may not be fully preserved, \"Fixed\" method usually gives the best result here");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_modifier_meshcache(BlenderRNA *brna)
{
  static const EnumPropertyItem prop_format_type_items[] = {
      {MOD_MESHCACHE_TYPE_MDD, "MDD", 0, "MDD", ""},
      {MOD_MESHCACHE_TYPE_PC2, "PC2", 0, "PC2", ""},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem prop_deform_mode_items[] = {
      {MOD_MESHCACHE_DEFORM_OVERWRITE,
       "OVERWRITE",
       0,
       "Overwrite",
       "Replace vertex coordinates with cached values"},
      {MOD_MESHCACHE_DEFORM_INTEGRATE,
       "INTEGRATE",
       0,
       "Integrate",
       "Integrate deformation from this modifier's input with the mesh-cache coordinates "
       "(useful for shape keys)"},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem prop_interpolation_type_items[] = {
      {MOD_MESHCACHE_INTERP_NONE, "NONE", 0, "None", ""},
      {MOD_MESHCACHE_INTERP_LINEAR, "LINEAR", 0, "Linear", ""},
      /* for cardinal we'd need to read 4x cache's */
      // {MOD_MESHCACHE_INTERP_CARDINAL, "CARDINAL", 0, "Cardinal", ""},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem prop_time_type_items[] = {
      /* use 'eval_frame' */
      {MOD_MESHCACHE_TIME_FRAME,
       "FRAME",
       0,
       "Frame",
       "Control playback using a frame-number "
       "(ignoring time FPS and start frame from the file)"},
      /* use 'eval_time' */
      {MOD_MESHCACHE_TIME_SECONDS, "TIME", 0, "Time", "Control playback using time in seconds"},
      /* use 'eval_factor' */
      {MOD_MESHCACHE_TIME_FACTOR,
       "FACTOR",
       0,
       "Factor",
       "Control playback using a value between 0 and 1"},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem prop_time_play_items[] = {
      {MOD_MESHCACHE_PLAY_CFEA, "SCENE", 0, "Scene", "Use the time from the scene"},
      {MOD_MESHCACHE_PLAY_EVAL, "CUSTOM", 0, "Custom", "Use the modifier's own time evaluation"},
      {0, NULL, 0, NULL, NULL},
  };

  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "MeshCacheModifier", "Modifier");
  RNA_def_struct_ui_text(srna, "Cache Modifier", "Cache Mesh");
  RNA_def_struct_sdna(srna, "MeshCacheModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_MESHDEFORM); /* XXX, needs own icon */

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "cache_format", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "type");
  RNA_def_property_enum_items(prop, prop_format_type_items);
  RNA_def_property_ui_text(prop, "Format", "");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "interpolation", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "interp");
  RNA_def_property_enum_items(prop, prop_interpolation_type_items);
  RNA_def_property_ui_text(prop, "Interpolation", "");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "time_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "time_mode");
  RNA_def_property_enum_items(prop, prop_time_type_items);
  RNA_def_property_ui_text(prop, "Time Mode", "Method to control playback time");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "play_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "play_mode");
  RNA_def_property_enum_items(prop, prop_time_play_items);
  RNA_def_property_ui_text(prop, "Play Mode", "");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "deform_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "deform_mode");
  RNA_def_property_enum_items(prop, prop_deform_mode_items);
  RNA_def_property_ui_text(prop, "Deform Mode", "");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "filepath", PROP_STRING, PROP_FILEPATH);
  RNA_def_property_ui_text(prop, "File Path", "Path to external displacements file");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "factor", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "factor");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Influence", "Influence of the deformation");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "defgrp_name");
  RNA_def_property_ui_text(
      prop,
      "Vertex Group",
      "Name of the Vertex Group which determines the influence of the modifier per point");
  RNA_def_property_string_funcs(prop, NULL, NULL, "rna_MeshCacheModifier_defgrp_name_set");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "invert_vertex_group", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_MESHCACHE_INVERT_VERTEX_GROUP);
  RNA_def_property_ui_text(prop, "Invert", "Invert vertex group influence");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  /* -------------------------------------------------------------------- */
  /* Axis Conversion */
  prop = RNA_def_property(srna, "forward_axis", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "forward_axis");
  RNA_def_property_enum_items(prop, rna_enum_object_axis_items);
  RNA_def_property_ui_text(prop, "Forward", "");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "up_axis", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "up_axis");
  RNA_def_property_enum_items(prop, rna_enum_object_axis_items);
  RNA_def_property_ui_text(prop, "Up", "");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "flip_axis", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "flip_axis");
  RNA_def_property_enum_items(prop, rna_enum_axis_flag_xyz_items);
  RNA_def_property_flag(prop, PROP_ENUM_FLAG);
  RNA_def_property_ui_text(prop, "Flip Axis", "");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  /* -------------------------------------------------------------------- */
  /* For Scene time */
  prop = RNA_def_property(srna, "frame_start", PROP_FLOAT, PROP_TIME);
  RNA_def_property_float_sdna(prop, NULL, "frame_start");
  RNA_def_property_range(prop, -MAXFRAME, MAXFRAME);
  RNA_def_property_ui_text(prop, "Frame Start", "Add this to the start frame");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "frame_scale", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "frame_scale");
  RNA_def_property_range(prop, 0.0f, 100.0f);
  RNA_def_property_ui_text(prop, "Frame Scale", "Evaluation time in seconds");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  /* -------------------------------------------------------------------- */
  /* eval values depend on 'time_mode' */
  prop = RNA_def_property(srna, "eval_frame", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "eval_frame");
  RNA_def_property_range(prop, MINFRAME, MAXFRAME);
  RNA_def_property_ui_text(prop, "Evaluation Frame", "The frame to evaluate (starting at 0)");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "eval_time", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "eval_time");
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_ui_text(prop, "Evaluation Time", "Evaluation time in seconds");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "eval_factor", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "eval_factor");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Evaluation Factor", "Evaluation time in seconds");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_modifier_meshseqcache(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "MeshSequenceCacheModifier", "Modifier");
  RNA_def_struct_ui_text(srna, "Cache Modifier", "Cache Mesh");
  RNA_def_struct_sdna(srna, "MeshSeqCacheModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_MESHDEFORM); /* XXX, needs own icon */

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "cache_file", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "cache_file");
  RNA_def_property_struct_type(prop, "CacheFile");
  RNA_def_property_ui_text(prop, "Cache File", "");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

  prop = RNA_def_property(srna, "object_path", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(
      prop,
      "Object Path",
      "Path to the object in the Alembic archive used to lookup geometric data");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  static const EnumPropertyItem read_flag_items[] = {
      {MOD_MESHSEQ_READ_VERT, "VERT", 0, "Vertex", ""},
      {MOD_MESHSEQ_READ_POLY, "POLY", 0, "Faces", ""},
      {MOD_MESHSEQ_READ_UV, "UV", 0, "UV", ""},
      {MOD_MESHSEQ_READ_COLOR, "COLOR", 0, "Color", ""},
      {0, NULL, 0, NULL, NULL},
  };

  prop = RNA_def_property(srna, "read_data", PROP_ENUM, PROP_NONE);
  RNA_def_property_flag(prop, PROP_ENUM_FLAG);
  RNA_def_property_enum_sdna(prop, NULL, "read_flag");
  RNA_def_property_enum_items(prop, read_flag_items);
  RNA_def_property_ui_text(prop, "Read Data", "Data to read from the cache");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_vertex_interpolation", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "read_flag", MOD_MESHSEQ_INTERPOLATE_VERTICES);
  RNA_def_property_ui_text(
      prop, "Vertex Interpolation", "Allow interpolation of vertex positions");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "velocity_scale", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "velocity_scale");
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_ui_text(
      prop,
      "Velocity Scale",
      "Multiplier used to control the magnitude of the velocity vectors for time effects");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_modifier_laplaciandeform(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "LaplacianDeformModifier", "Modifier");
  RNA_def_struct_ui_text(srna, "Laplacian Deform Modifier", "Mesh deform modifier");
  RNA_def_struct_sdna(srna, "LaplacianDeformModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_MESHDEFORM);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "anchor_grp_name");
  RNA_def_property_ui_text(
      prop, "Anchor Weights", "Name of Vertex Group which determines Anchors");
  RNA_def_property_string_funcs(
      prop, NULL, NULL, "rna_LaplacianDeformModifier_anchor_grp_name_set");

  prop = RNA_def_property(srna, "iterations", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "repeat");
  RNA_def_property_ui_range(prop, 1, 50, 1, -1);
  RNA_def_property_ui_text(prop, "Repeat", "");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "is_bind", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop, "rna_LaplacianDeformModifier_is_bind_get", NULL);
  RNA_def_property_ui_text(prop, "Bound", "Whether geometry has been bound to anchors");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_property(srna, "invert_vertex_group", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_LAPLACIANDEFORM_INVERT_VGROUP);
  RNA_def_property_ui_text(prop, "Invert", "Invert vertex group influence");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  RNA_define_lib_overridable(false);

  RNA_def_property_update(prop, 0, "rna_Modifier_update");
}

static void rna_def_modifier_weld(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem mode_items[] = {
      {MOD_WELD_MODE_ALL, "ALL", 0, "All", "Full merge by distance"},
      {MOD_WELD_MODE_CONNECTED, "CONNECTED", 0, "Connected", "Only merge along the edges"},
      {0, NULL, 0, NULL, NULL},
  };

  srna = RNA_def_struct(brna, "WeldModifier", "Modifier");
  RNA_def_struct_ui_text(srna, "Weld Modifier", "Weld modifier");
  RNA_def_struct_sdna(srna, "WeldModifierData");
  RNA_def_struct_ui_icon(srna, ICON_AUTOMERGE_OFF);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, mode_items);
  RNA_def_property_ui_text(prop, "Mode", "Mode defines the merge rule");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "merge_threshold", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, NULL, "merge_dist");
  RNA_def_property_range(prop, 0, FLT_MAX);
  RNA_def_property_ui_range(prop, 0, 1, 0.001, 6);
  RNA_def_property_ui_text(prop, "Merge Distance", "Limit below which to merge vertices");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "defgrp_name");
  RNA_def_property_ui_text(
      prop, "Vertex Group", "Vertex group name for selecting the affected areas");
  RNA_def_property_string_funcs(prop, NULL, NULL, "rna_WeldModifier_defgrp_name_set");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "invert_vertex_group", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_WELD_INVERT_VGROUP);
  RNA_def_property_ui_text(prop, "Invert", "Invert vertex group influence");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "loose_edges", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_WELD_LOOSE_EDGES);
  RNA_def_property_ui_text(
      prop, "Only Loose Edges", "Collapse edges without faces, cloth sewing edges");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_modifier_wireframe(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "WireframeModifier", "Modifier");
  RNA_def_struct_ui_text(srna, "Wireframe Modifier", "Wireframe effect modifier");
  RNA_def_struct_sdna(srna, "WireframeModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_WIREFRAME);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "thickness", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, NULL, "offset");
  RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.01, 4);
  RNA_def_property_ui_text(prop, "Thickness", "Thickness factor");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "thickness_vertex_group", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "offset_fac_vg");
  RNA_def_property_range(prop, 0.0, 1.0);
  RNA_def_property_ui_range(prop, 0, 1, 0.1, 3);
  RNA_def_property_ui_text(
      prop, "Vertex Group Factor", "Thickness factor to use for zero vertex group influence");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "offset", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "offset_fac");
  RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
  RNA_def_property_ui_range(prop, -1, 1, 0.1, 4);
  RNA_def_property_ui_text(prop, "Offset", "Offset the thickness from the center");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_replace", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_WIREFRAME_REPLACE);
  RNA_def_property_ui_text(prop, "Replace", "Remove original geometry");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_boundary", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_WIREFRAME_BOUNDARY);
  RNA_def_property_ui_text(prop, "Boundary", "Support face boundaries");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_even_offset", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_WIREFRAME_OFS_EVEN);
  RNA_def_property_ui_text(prop, "Offset Even", "Scale the offset to give more even thickness");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_relative_offset", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_WIREFRAME_OFS_RELATIVE);
  RNA_def_property_ui_text(prop, "Offset Relative", "Scale the offset by surrounding geometry");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_crease", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_WIREFRAME_CREASE);
  RNA_def_property_ui_text(
      prop, "Offset Relative", "Crease hub edges for improved subdivision surface");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "crease_weight", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "crease_weight");
  RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.1, 1);
  RNA_def_property_ui_text(prop, "Weight", "Crease weight (if active)");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "material_offset", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "mat_ofs");
  RNA_def_property_range(prop, SHRT_MIN, SHRT_MAX);
  RNA_def_property_ui_text(prop, "Material Offset", "Offset material index of generated faces");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "defgrp_name");
  RNA_def_property_ui_text(
      prop, "Vertex Group", "Vertex group name for selecting the affected areas");
  RNA_def_property_string_funcs(prop, NULL, NULL, "rna_WireframeModifier_defgrp_name_set");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "invert_vertex_group", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_WIREFRAME_INVERT_VGROUP);
  RNA_def_property_ui_text(prop, "Invert", "Invert vertex group influence");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_modifier_datatransfer(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem DT_layer_vert_items[] = {
    {DT_TYPE_MDEFORMVERT,
     "VGROUP_WEIGHTS",
     0,
     "Vertex Groups",
     "Transfer active or all vertex groups"},
#  if 0 /* TODO */
    {DT_TYPE_SHAPEKEY, "SHAPEKEYS", 0, "Shapekey(s)", "Transfer active or all shape keys"},
#  endif
  /* XXX When SkinModifier is enabled,
   * it seems to erase its own CD_MVERT_SKIN layer from final DM :( */
#  if 0
    {DT_TYPE_SKIN, "SKIN", 0, "Skin Weight", "Transfer skin weights"},
#  endif
    {DT_TYPE_BWEIGHT_VERT, "BEVEL_WEIGHT_VERT", 0, "Bevel Weight", "Transfer bevel weights"},
    {DT_TYPE_MPROPCOL_VERT | DT_TYPE_MLOOPCOL_VERT,
     "COLOR_VERTEX",
     0,
     "Colors",
     "Transfer color attributes"},
    {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem DT_layer_edge_items[] = {
      {DT_TYPE_SHARP_EDGE, "SHARP_EDGE", 0, "Sharp", "Transfer sharp mark"},
      {DT_TYPE_SEAM, "SEAM", 0, "UV Seam", "Transfer UV seam mark"},
      {DT_TYPE_CREASE, "CREASE", 0, "Crease", "Transfer subdivision crease values"},
      {DT_TYPE_BWEIGHT_EDGE, "BEVEL_WEIGHT_EDGE", 0, "Bevel Weight", "Transfer bevel weights"},
      {DT_TYPE_FREESTYLE_EDGE, "FREESTYLE_EDGE", 0, "Freestyle", "Transfer Freestyle edge mark"},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem DT_layer_loop_items[] = {
      {DT_TYPE_LNOR, "CUSTOM_NORMAL", 0, "Custom Normals", "Transfer custom normals"},
      {DT_TYPE_MPROPCOL_LOOP | DT_TYPE_MLOOPCOL_LOOP,
       "COLOR_CORNER",
       0,
       "Colors",
       "Transfer color attributes"},
      {DT_TYPE_UV, "UV", 0, "UVs", "Transfer UV layers"},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem DT_layer_poly_items[] = {
      {DT_TYPE_SHARP_FACE, "SMOOTH", 0, "Smooth", "Transfer flat/smooth mark"},
      {DT_TYPE_FREESTYLE_FACE,
       "FREESTYLE_FACE",
       0,
       "Freestyle Mark",
       "Transfer Freestyle face mark"},
      {0, NULL, 0, NULL, NULL},
  };

  srna = RNA_def_struct(brna, "DataTransferModifier", "Modifier");
  RNA_def_struct_ui_text(
      srna, "Data Transfer Modifier", "Modifier transferring some data from a source mesh");
  RNA_def_struct_sdna(srna, "DataTransferModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_DATA_TRANSFER);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "ob_source");
  RNA_def_property_ui_text(prop, "Source Object", "Object to transfer data from");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_pointer_funcs(
      prop, NULL, "rna_DataTransferModifier_ob_source_set", NULL, "rna_Mesh_object_poll");
  RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

  prop = RNA_def_boolean(srna,
                         "use_object_transform",
                         true,
                         "Object Transform",
                         "Evaluate source and destination meshes in global space");
  RNA_def_property_boolean_sdna(prop, NULL, "flags", MOD_DATATRANSFER_OBSRC_TRANSFORM);
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  /* Generic, UI-only data types toggles. */
  prop = RNA_def_boolean(
      srna, "use_vert_data", false, "Vertex Data", "Enable vertex data transfer");
  RNA_def_property_boolean_sdna(prop, NULL, "flags", MOD_DATATRANSFER_USE_VERT);
  RNA_def_property_update(prop, 0, "rna_DataTransferModifier_use_data_update");

  prop = RNA_def_boolean(srna, "use_edge_data", false, "Edge Data", "Enable edge data transfer");
  RNA_def_property_boolean_sdna(prop, NULL, "flags", MOD_DATATRANSFER_USE_EDGE);
  RNA_def_property_update(prop, 0, "rna_DataTransferModifier_use_data_update");

  prop = RNA_def_boolean(
      srna, "use_loop_data", false, "Face Corner Data", "Enable face corner data transfer");
  RNA_def_property_boolean_sdna(prop, NULL, "flags", MOD_DATATRANSFER_USE_LOOP);
  RNA_def_property_update(prop, 0, "rna_DataTransferModifier_use_data_update");

  prop = RNA_def_boolean(srna, "use_poly_data", false, "Face Data", "Enable face data transfer");
  RNA_def_property_boolean_sdna(prop, NULL, "flags", MOD_DATATRANSFER_USE_POLY);
  RNA_def_property_update(prop, 0, "rna_DataTransferModifier_use_data_update");

  /* Actual data types selection. */
  prop = RNA_def_enum(srna,
                      "data_types_verts",
                      DT_layer_vert_items,
                      0,
                      "Vertex Data Types",
                      "Which vertex data layers to transfer");
  RNA_def_property_flag(prop, PROP_ENUM_FLAG);
  RNA_def_property_enum_sdna(prop, NULL, "data_types");
  RNA_def_property_enum_funcs(prop, NULL, "rna_DataTransferModifier_verts_data_types_set", NULL);
  RNA_def_property_update(prop, 0, "rna_DataTransferModifier_data_types_update");

  prop = RNA_def_enum(srna,
                      "data_types_edges",
                      DT_layer_edge_items,
                      0,
                      "Edge Data Types",
                      "Which edge data layers to transfer");
  RNA_def_property_flag(prop, PROP_ENUM_FLAG);
  RNA_def_property_enum_sdna(prop, NULL, "data_types");
  RNA_def_property_enum_funcs(prop, NULL, "rna_DataTransferModifier_edges_data_types_set", NULL);
  RNA_def_property_update(prop, 0, "rna_DataTransferModifier_data_types_update");

  prop = RNA_def_enum(srna,
                      "data_types_loops",
                      DT_layer_loop_items,
                      0,
                      "Face Corner Data Types",
                      "Which face corner data layers to transfer");
  RNA_def_property_flag(prop, PROP_ENUM_FLAG);
  RNA_def_property_enum_sdna(prop, NULL, "data_types");
  RNA_def_property_enum_funcs(prop, NULL, "rna_DataTransferModifier_loops_data_types_set", NULL);
  RNA_def_property_update(prop, 0, "rna_DataTransferModifier_data_types_update");

  prop = RNA_def_enum(srna,
                      "data_types_polys",
                      DT_layer_poly_items,
                      0,
                      "Poly Data Types",
                      "Which poly data layers to transfer");
  RNA_def_property_flag(prop, PROP_ENUM_FLAG);
  RNA_def_property_enum_sdna(prop, NULL, "data_types");
  RNA_def_property_enum_funcs(prop, NULL, "rna_DataTransferModifier_polys_data_types_set", NULL);
  RNA_def_property_update(prop, 0, "rna_DataTransferModifier_data_types_update");

  /* Mapping methods. */
  prop = RNA_def_enum(srna,
                      "vert_mapping",
                      rna_enum_dt_method_vertex_items,
                      MREMAP_MODE_VERT_NEAREST,
                      "Vertex Mapping",
                      "Method used to map source vertices to destination ones");
  RNA_def_property_enum_sdna(prop, NULL, "vmap_mode");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_enum(srna,
                      "edge_mapping",
                      rna_enum_dt_method_edge_items,
                      MREMAP_MODE_EDGE_NEAREST,
                      "Edge Mapping",
                      "Method used to map source edges to destination ones");
  RNA_def_property_enum_sdna(prop, NULL, "emap_mode");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_enum(srna,
                      "loop_mapping",
                      rna_enum_dt_method_loop_items,
                      MREMAP_MODE_LOOP_NEAREST_POLYNOR,
                      "Face Corner Mapping",
                      "Method used to map source faces' corners to destination ones");
  RNA_def_property_enum_sdna(prop, NULL, "lmap_mode");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_enum(srna,
                      "poly_mapping",
                      rna_enum_dt_method_poly_items,
                      MREMAP_MODE_POLY_NEAREST,
                      "Face Mapping",
                      "Method used to map source faces to destination ones");
  RNA_def_property_enum_sdna(prop, NULL, "pmap_mode");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  /* Mapping options and filtering. */
  prop = RNA_def_boolean(
      srna,
      "use_max_distance",
      false,
      "Only Neighbor Geometry",
      "Source elements must be closer than given distance from destination one");
  RNA_def_property_boolean_sdna(prop, NULL, "flags", MOD_DATATRANSFER_MAP_MAXDIST);
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_float(
      srna,
      "max_distance",
      1.0f,
      0.0f,
      FLT_MAX,
      "Max Distance",
      "Maximum allowed distance between source and destination element, for non-topology mappings",
      0.0f,
      100.0f);
  RNA_def_property_float_sdna(prop, NULL, "map_max_distance");
  RNA_def_property_subtype(prop, PROP_DISTANCE);
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_float(
      srna,
      "ray_radius",
      0.0f,
      0.0f,
      FLT_MAX,
      "Ray Radius",
      "'Width' of rays (especially useful when raycasting against vertices or edges)",
      0.0f,
      10.0f);
  RNA_def_property_float_sdna(prop, NULL, "map_ray_radius");
  RNA_def_property_subtype(prop, PROP_DISTANCE);
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_float(
      srna,
      "islands_precision",
      0.0f,
      0.0f,
      1.0f,
      "Islands Precision",
      "Factor controlling precision of islands handling "
      "(typically, 0.1 should be enough, higher values can make things really slow)",
      0.0f,
      1.0f);
  RNA_def_property_subtype(prop, PROP_DISTANCE);
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  /* How to handle multi-layers types of data. */
  prop = RNA_def_enum(srna,
                      "layers_vgroup_select_src",
                      rna_enum_dt_layers_select_src_items,
                      DT_LAYERS_ALL_SRC,
                      "Source Layers Selection",
                      "Which layers to transfer, in case of multi-layers types");
  RNA_def_property_enum_sdna(prop, NULL, "layers_select_src[DT_MULTILAYER_INDEX_MDEFORMVERT]");
  RNA_def_property_enum_funcs(
      prop, NULL, NULL, "rna_DataTransferModifier_layers_select_src_itemf");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

#  if 0
  prop = RNA_def_enum(srna,
                      "layers_shapekey_select_src",
                      rna_enum_dt_layers_select_src_items,
                      DT_LAYERS_ALL_SRC,
                      "Source Layers Selection",
                      "Which layers to transfer, in case of multi-layers types");
  RNA_def_property_enum_sdna(prop, NULL, "layers_select_src[DT_MULTILAYER_INDEX_SHAPEKEY]");
  RNA_def_property_enum_funcs(
      prop, NULL, NULL, "rna_DataTransferModifier_layers_select_src_itemf");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");
#  endif

  prop = RNA_def_enum(srna,
                      "layers_vcol_vert_select_src",
                      rna_enum_dt_layers_select_src_items,
                      DT_LAYERS_ALL_SRC,
                      "Source Layers Selection",
                      "Which layers to transfer, in case of multi-layers types");
  RNA_def_property_enum_sdna(prop, NULL, "layers_select_src[DT_MULTILAYER_INDEX_VCOL_VERT]");
  RNA_def_property_enum_funcs(
      prop, NULL, NULL, "rna_DataTransferModifier_layers_select_src_itemf");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_enum(srna,
                      "layers_vcol_loop_select_src",
                      rna_enum_dt_layers_select_src_items,
                      DT_LAYERS_ALL_SRC,
                      "Source Layers Selection",
                      "Which layers to transfer, in case of multi-layers types");
  RNA_def_property_enum_sdna(prop, NULL, "layers_select_src[DT_MULTILAYER_INDEX_VCOL_LOOP]");
  RNA_def_property_enum_funcs(
      prop, NULL, NULL, "rna_DataTransferModifier_layers_select_src_itemf");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_enum(srna,
                      "layers_uv_select_src",
                      rna_enum_dt_layers_select_src_items,
                      DT_LAYERS_ALL_SRC,
                      "Source Layers Selection",
                      "Which layers to transfer, in case of multi-layers types");
  RNA_def_property_enum_sdna(prop, NULL, "layers_select_src[DT_MULTILAYER_INDEX_UV]");
  RNA_def_property_enum_funcs(
      prop, NULL, NULL, "rna_DataTransferModifier_layers_select_src_itemf");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_enum(srna,
                      "layers_vgroup_select_dst",
                      rna_enum_dt_layers_select_dst_items,
                      DT_LAYERS_NAME_DST,
                      "Destination Layers Matching",
                      "How to match source and destination layers");
  RNA_def_property_enum_sdna(prop, NULL, "layers_select_dst[DT_MULTILAYER_INDEX_MDEFORMVERT]");
  RNA_def_property_enum_funcs(
      prop, NULL, NULL, "rna_DataTransferModifier_layers_select_dst_itemf");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

#  if 0
  prop = RNA_def_enum(srna,
                      "layers_shapekey_select_dst",
                      rna_enum_dt_layers_select_dst_items,
                      DT_LAYERS_NAME_DST,
                      "Destination Layers Matching",
                      "How to match source and destination layers");
  RNA_def_property_enum_sdna(prop, NULL, "layers_select_dst[DT_MULTILAYER_INDEX_SHAPEKEY]");
  RNA_def_property_enum_funcs(
      prop, NULL, NULL, "rna_DataTransferModifier_layers_select_dst_itemf");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");
#  endif

  prop = RNA_def_enum(srna,
                      "layers_vcol_vert_select_dst",
                      rna_enum_dt_layers_select_dst_items,
                      DT_LAYERS_NAME_DST,
                      "Destination Layers Matching",
                      "How to match source and destination layers");
  RNA_def_property_enum_sdna(prop, NULL, "layers_select_dst[DT_MULTILAYER_INDEX_VCOL_VERT]");
  RNA_def_property_enum_funcs(
      prop, NULL, NULL, "rna_DataTransferModifier_layers_select_dst_itemf");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_enum(srna,
                      "layers_vcol_loop_select_dst",
                      rna_enum_dt_layers_select_dst_items,
                      DT_LAYERS_NAME_DST,
                      "Destination Layers Matching",
                      "How to match source and destination layers");
  RNA_def_property_enum_sdna(prop, NULL, "layers_select_dst[DT_MULTILAYER_INDEX_VCOL_LOOP]");
  RNA_def_property_enum_funcs(
      prop, NULL, NULL, "rna_DataTransferModifier_layers_select_dst_itemf");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_enum(srna,
                      "layers_uv_select_dst",
                      rna_enum_dt_layers_select_dst_items,
                      DT_LAYERS_NAME_DST,
                      "Destination Layers Matching",
                      "How to match source and destination layers");
  RNA_def_property_enum_sdna(prop, NULL, "layers_select_dst[DT_MULTILAYER_INDEX_UV]");
  RNA_def_property_enum_funcs(
      prop, NULL, NULL, "rna_DataTransferModifier_layers_select_dst_itemf");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  /* Mix stuff */
  prop = RNA_def_enum(srna,
                      "mix_mode",
                      rna_enum_dt_mix_mode_items,
                      CDT_MIX_TRANSFER,
                      "Mix Mode",
                      "How to affect destination elements with source values");
  RNA_def_property_enum_funcs(prop, NULL, NULL, "rna_DataTransferModifier_mix_mode_itemf");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_float_factor(
      srna,
      "mix_factor",
      0.0f,
      0.0f,
      1.0f,
      "Mix Factor",
      "Factor to use when applying data to destination (exact behavior depends on mix mode, "
      "multiplied with weights from vertex group when defined)",
      0.0f,
      1.0f);
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_string(srna,
                        "vertex_group",
                        NULL,
                        MAX_VGROUP_NAME,
                        "Vertex Group",
                        "Vertex group name for selecting the affected areas");
  RNA_def_property_string_sdna(prop, NULL, "defgrp_name");
  RNA_def_property_string_funcs(prop, NULL, NULL, "rna_DataTransferModifier_defgrp_name_set");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_boolean(
      srna, "invert_vertex_group", false, "Invert", "Invert vertex group influence");
  RNA_def_property_boolean_sdna(prop, NULL, "flags", MOD_DATATRANSFER_INVERT_VGROUP);
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_modifier_normaledit(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem prop_mode_items[] = {
      {MOD_NORMALEDIT_MODE_RADIAL,
       "RADIAL",
       0,
       "Radial",
       "From an ellipsoid (shape defined by the boundbox's dimensions, target is optional)"},
      {MOD_NORMALEDIT_MODE_DIRECTIONAL,
       "DIRECTIONAL",
       0,
       "Directional",
       "Normals 'track' (point to) the target object"},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem prop_mix_mode_items[] = {
      {MOD_NORMALEDIT_MIX_COPY, "COPY", 0, "Copy", "Copy new normals (overwrite existing)"},
      {MOD_NORMALEDIT_MIX_ADD, "ADD", 0, "Add", "Copy sum of new and old normals"},
      {MOD_NORMALEDIT_MIX_SUB, "SUB", 0, "Subtract", "Copy new normals minus old normals"},
      {MOD_NORMALEDIT_MIX_MUL,
       "MUL",
       0,
       "Multiply",
       "Copy product of old and new normals (not cross product)"},
      {0, NULL, 0, NULL, NULL},
  };

  srna = RNA_def_struct(brna, "NormalEditModifier", "Modifier");
  RNA_def_struct_ui_text(
      srna, "Normal Edit Modifier", "Modifier affecting/generating custom normals");
  RNA_def_struct_sdna(srna, "NormalEditModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_NORMALEDIT);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, prop_mode_items);
  RNA_def_property_ui_text(prop, "Mode", "How to affect (generate) normals");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_float_array(srna,
                             "offset",
                             3,
                             NULL,
                             -FLT_MAX,
                             FLT_MAX,
                             "Offset",
                             "Offset from object's center",
                             -100.0f,
                             100.0f);
  RNA_def_property_subtype(prop, PROP_COORDS);
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "mix_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, prop_mix_mode_items);
  RNA_def_property_ui_text(prop, "Mix Mode", "How to mix generated normals with existing ones");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "mix_factor", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_range(prop, 0.0, 1.0);
  RNA_def_property_ui_text(
      prop, "Mix Factor", "How much of generated normals to mix with existing ones");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "mix_limit", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_range(prop, 0.0, DEG2RADF(180.0f));
  RNA_def_property_ui_text(prop, "Max Angle", "Maximum angle between old and new normals");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "no_polynors_fix", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_NORMALEDIT_NO_POLYNORS_FIX);
  RNA_def_property_boolean_default(prop, false);
  RNA_def_property_ui_text(prop,
                           "Lock Polygon Normals",
                           "Do not flip polygons when their normals are not consistent "
                           "with their newly computed custom vertex normals");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "defgrp_name");
  RNA_def_property_ui_text(
      prop, "Vertex Group", "Vertex group name for selecting/weighting the affected areas");
  RNA_def_property_string_funcs(prop, NULL, NULL, "rna_NormalEditModifier_defgrp_name_set");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "invert_vertex_group", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_NORMALEDIT_INVERT_VGROUP);
  RNA_def_property_ui_text(prop, "Invert", "Invert vertex group influence");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "target", PROP_POINTER, PROP_NONE);
  RNA_def_property_ui_text(prop, "Target", "Target object used to affect normals");
  RNA_def_property_pointer_funcs(prop, NULL, "rna_NormalEditModifier_target_set", NULL, NULL);
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

  prop = RNA_def_property(srna, "use_direction_parallel", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_NORMALEDIT_USE_DIRECTION_PARALLEL);
  RNA_def_property_boolean_default(prop, true);
  RNA_def_property_ui_text(prop,
                           "Parallel Normals",
                           "Use same direction for all normals, from origin to target's center "
                           "(Directional mode only)");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_modifier_surfacedeform(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "SurfaceDeformModifier", "Modifier");
  RNA_def_struct_ui_text(srna, "SurfaceDeform Modifier", "");
  RNA_def_struct_sdna(srna, "SurfaceDeformModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_MESHDEFORM);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "target", PROP_POINTER, PROP_NONE);
  RNA_def_property_ui_text(prop, "Target", "Mesh object to deform with");
  RNA_def_property_pointer_funcs(
      prop, NULL, "rna_SurfaceDeformModifier_target_set", NULL, "rna_Mesh_object_poll");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

  prop = RNA_def_property(srna, "falloff", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 2.0f, 16.0f);
  RNA_def_property_ui_text(
      prop, "Interpolation Falloff", "Controls how much nearby polygons influence deformation");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "is_bound", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop, "rna_SurfaceDeformModifier_is_bound_get", NULL);
  RNA_def_property_ui_text(prop, "Bound", "Whether geometry has been bound to target mesh");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "defgrp_name");
  RNA_def_property_ui_text(
      prop, "Vertex Group", "Vertex group name for selecting/weighting the affected areas");
  RNA_def_property_string_funcs(prop, NULL, NULL, "rna_SurfaceDeformModifier_defgrp_name_set");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "invert_vertex_group", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flags", MOD_SDEF_INVERT_VGROUP);
  RNA_def_property_ui_text(prop, "Invert", "Invert vertex group influence");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_sparse_bind", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flags", MOD_SDEF_SPARSE_BIND);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop,
      "Sparse Bind",
      "Only record binding data for vertices matching the vertex group at the time of bind");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "strength", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, -100, 100);
  RNA_def_property_ui_range(prop, -100, 100, 10, 2);
  RNA_def_property_ui_text(prop, "Strength", "Strength of modifier deformations");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_modifier_weightednormal(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static EnumPropertyItem prop_weighting_mode_items[] = {
      {MOD_WEIGHTEDNORMAL_MODE_FACE,
       "FACE_AREA",
       0,
       "Face Area",
       "Generate face area weighted normals"},
      {MOD_WEIGHTEDNORMAL_MODE_ANGLE,
       "CORNER_ANGLE",
       0,
       "Corner Angle",
       "Generate corner angle weighted normals"},
      {MOD_WEIGHTEDNORMAL_MODE_FACE_ANGLE,
       "FACE_AREA_WITH_ANGLE",
       0,
       "Face Area & Angle",
       "Generated normals weighted by both face area and angle"},
      {0, NULL, 0, NULL, NULL},
  };

  srna = RNA_def_struct(brna, "WeightedNormalModifier", "Modifier");
  RNA_def_struct_ui_text(srna, "WeightedNormal Modifier", "");
  RNA_def_struct_sdna(srna, "WeightedNormalModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_NORMALEDIT);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "weight", PROP_INT, PROP_NONE);
  RNA_def_property_range(prop, 1, 100);
  RNA_def_property_ui_range(prop, 1, 100, 1, -1);
  RNA_def_property_ui_text(prop,
                           "Weight",
                           "Corrective factor applied to faces' weights, 50 is neutral, "
                           "lower values increase weight of weak faces, "
                           "higher values increase weight of strong faces");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, prop_weighting_mode_items);
  RNA_def_property_ui_text(prop, "Weighting Mode", "Weighted vertex normal mode to use");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "thresh", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 0, 10);
  RNA_def_property_ui_range(prop, 0, 10, 1, 2);
  RNA_def_property_ui_text(
      prop, "Threshold", "Threshold value for different weights to be considered equal");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "keep_sharp", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_WEIGHTEDNORMAL_KEEP_SHARP);
  RNA_def_property_ui_text(prop,
                           "Keep Sharp",
                           "Keep sharp edges as computed for default split normals, "
                           "instead of setting a single weighted normal for each vertex");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "defgrp_name");
  RNA_def_property_ui_text(
      prop, "Vertex Group", "Vertex group name for modifying the selected areas");
  RNA_def_property_string_funcs(prop, NULL, NULL, "rna_WeightedNormalModifier_defgrp_name_set");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "invert_vertex_group", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_WEIGHTEDNORMAL_INVERT_VGROUP);
  RNA_def_property_ui_text(prop, "Invert", "Invert vertex group influence");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_face_influence", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_WEIGHTEDNORMAL_FACE_INFLUENCE);
  RNA_def_property_ui_text(prop, "Face Influence", "Use influence of face for weighting");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_modifier_nodes(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "NodesModifier", "Modifier");
  RNA_def_struct_ui_text(srna, "Nodes Modifier", "");
  RNA_def_struct_sdna(srna, "NodesModifierData");
  RNA_def_struct_idprops_func(srna, "rna_NodesModifier_properties");
  RNA_def_struct_ui_icon(srna, ICON_GEOMETRY_NODES);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "node_group", PROP_POINTER, PROP_NONE);
  RNA_def_property_ui_text(prop, "Node Group", "Node group that controls what this modifier does");
  RNA_def_property_pointer_funcs(prop, NULL, NULL, NULL, "rna_NodesModifier_node_group_poll");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_update(prop, 0, "rna_NodesModifier_node_group_update");

  prop = RNA_def_property(srna, "simulation_bake_directory", PROP_STRING, PROP_DIRPATH);
  RNA_def_property_ui_text(
      prop, "Simulation Bake Directory", "Location on disk where the bake data is stored");
  RNA_def_property_update(prop, 0, NULL);

  RNA_define_lib_overridable(false);
}

static void rna_def_modifier_mesh_to_volume(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static EnumPropertyItem resolution_mode_items[] = {
      {MESH_TO_VOLUME_RESOLUTION_MODE_VOXEL_AMOUNT,
       "VOXEL_AMOUNT",
       0,
       "Voxel Amount",
       "Desired number of voxels along one axis"},
      {MESH_TO_VOLUME_RESOLUTION_MODE_VOXEL_SIZE,
       "VOXEL_SIZE",
       0,
       "Voxel Size",
       "Desired voxel side length"},
      {0, NULL, 0, NULL, NULL},
  };

  srna = RNA_def_struct(brna, "MeshToVolumeModifier", "Modifier");
  RNA_def_struct_ui_text(srna, "Mesh to Volume Modifier", "");
  RNA_def_struct_sdna(srna, "MeshToVolumeModifierData");
  RNA_def_struct_ui_icon(srna, ICON_VOLUME_DATA); /* TODO: Use correct icon. */

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
  RNA_def_property_ui_text(prop, "Object", "Object");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

  prop = RNA_def_property(srna, "resolution_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, resolution_mode_items);
  RNA_def_property_ui_text(
      prop, "Resolution Mode", "Mode for how the desired voxel size is specified");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "voxel_size", PROP_FLOAT, PROP_NONE);
  RNA_def_property_ui_text(
      prop, "Voxel Size", "Smaller values result in a higher resolution output");
  RNA_def_property_range(prop, 0.0, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0, FLT_MAX, 0.01, 4);
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "voxel_amount", PROP_INT, PROP_NONE);
  RNA_def_property_ui_text(prop, "Voxel Amount", "Approximate number of voxels along one axis");
  RNA_def_property_range(prop, 0, INT_MAX);
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "interior_band_width", PROP_FLOAT, PROP_NONE);
  RNA_def_property_ui_text(
      prop, "Interior Band Width", "Width of the gradient inside of the mesh");
  RNA_def_property_range(prop, 0.0, FLT_MAX);
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "density", PROP_FLOAT, PROP_NONE);
  RNA_def_property_ui_text(prop, "Density", "Density of the new volume");
  RNA_def_property_range(prop, 0.0, FLT_MAX);
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_modifier_volume_displace(BlenderRNA *brna)
{
  static const EnumPropertyItem prop_texture_map_mode_items[] = {
      {MOD_VOLUME_DISPLACE_MAP_LOCAL,
       "LOCAL",
       0,
       "Local",
       "Use the local coordinate system for the texture coordinates"},
      {MOD_VOLUME_DISPLACE_MAP_GLOBAL,
       "GLOBAL",
       0,
       "Global",
       "Use the global coordinate system for the texture coordinates"},
      {MOD_VOLUME_DISPLACE_MAP_OBJECT,
       "OBJECT",
       0,
       "Object",
       "Use the linked object's local coordinate system for the texture coordinates"},
      {0, NULL, 0, NULL, NULL},
  };

  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "VolumeDisplaceModifier", "Modifier");
  RNA_def_struct_ui_text(srna, "Volume Displace Modifier", "");
  RNA_def_struct_sdna(srna, "VolumeDisplaceModifierData");
  RNA_def_struct_ui_icon(srna, ICON_VOLUME_DATA); /* TODO: Use correct icon. */

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "strength", PROP_FLOAT, PROP_NONE);
  RNA_def_property_ui_text(prop, "Strength", "Strength of the displacement");
  RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 0.1, 4);
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "texture", PROP_POINTER, PROP_NONE);
  RNA_def_property_ui_text(prop, "Texture", "");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

  prop = RNA_def_property(srna, "texture_map_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, prop_texture_map_mode_items);
  RNA_def_property_ui_text(prop, "Texture Mapping Mode", "");
  RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

  prop = RNA_def_property(srna, "texture_map_object", PROP_POINTER, PROP_NONE);
  RNA_def_property_ui_text(prop, "Object", "Object to use for texture mapping");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

  prop = RNA_def_property(srna, "texture_mid_level", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_ui_text(
      prop, "Texture Mid Level", "Subtracted from the texture color to get a displacement vector");
  RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.1f, 5);
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "texture_sample_radius", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_ui_text(
      prop,
      "Texture Sample Radius",
      "Smaller values result in better performance but might cut off the volume");
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.1f, 5);
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_modifier_volume_to_mesh(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static EnumPropertyItem resolution_mode_items[] = {
      {VOLUME_TO_MESH_RESOLUTION_MODE_GRID,
       "GRID",
       0,
       "Grid",
       "Use resolution of the volume grid"},
      {VOLUME_TO_MESH_RESOLUTION_MODE_VOXEL_AMOUNT,
       "VOXEL_AMOUNT",
       0,
       "Voxel Amount",
       "Desired number of voxels along one axis"},
      {VOLUME_TO_MESH_RESOLUTION_MODE_VOXEL_SIZE,
       "VOXEL_SIZE",
       0,
       "Voxel Size",
       "Desired voxel side length"},
      {0, NULL, 0, NULL, NULL},
  };

  srna = RNA_def_struct(brna, "VolumeToMeshModifier", "Modifier");
  RNA_def_struct_ui_text(srna, "Volume to Mesh Modifier", "");
  RNA_def_struct_sdna(srna, "VolumeToMeshModifierData");
  RNA_def_struct_ui_icon(srna, ICON_VOLUME_DATA); /* TODO: Use correct icon. */

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
  RNA_def_property_ui_text(prop, "Object", "Object");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

  prop = RNA_def_property(srna, "threshold", PROP_FLOAT, PROP_NONE);
  RNA_def_property_ui_text(
      prop, "Threshold", "Voxels with a larger value are inside the generated mesh");
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.001f, 1.0f, 0.1f, 5);
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "adaptivity", PROP_FLOAT, PROP_NONE);
  RNA_def_property_ui_text(
      prop,
      "Adaptivity",
      "Reduces the final face count by simplifying geometry where detail is not needed");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "use_smooth_shade", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", VOLUME_TO_MESH_USE_SMOOTH_SHADE);
  RNA_def_property_ui_text(
      prop, "Smooth Shading", "Output faces with smooth shading rather than flat shaded");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "grid_name", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(
      prop, "Grid Name", "Grid in the volume object that is converted to a mesh");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "resolution_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, resolution_mode_items);
  RNA_def_property_ui_text(
      prop, "Resolution Mode", "Mode for how the desired voxel size is specified");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "voxel_size", PROP_FLOAT, PROP_NONE);
  RNA_def_property_ui_text(
      prop, "Voxel Size", "Smaller values result in a higher resolution output");
  RNA_def_property_range(prop, 0.0, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0, FLT_MAX, 0.01, 4);
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "voxel_amount", PROP_INT, PROP_NONE);
  RNA_def_property_ui_text(prop, "Voxel Amount", "Approximate number of voxels along one axis");
  RNA_def_property_range(prop, 0, INT_MAX);
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  RNA_define_lib_overridable(false);
}

void RNA_def_modifier(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  /* data */
  srna = RNA_def_struct(brna, "Modifier", NULL);
  RNA_def_struct_ui_text(srna, "Modifier", "Modifier affecting the geometry data of an object");
  RNA_def_struct_refine_func(srna, "rna_Modifier_refine");
  RNA_def_struct_path_func(srna, "rna_Modifier_path");
  RNA_def_struct_sdna(srna, "ModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MODIFIER);

  /* strings */
  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(prop, NULL, NULL, "rna_Modifier_name_set");
  RNA_def_property_ui_text(prop, "Name", "Modifier name");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER | NA_RENAME, "rna_Modifier_name_update");
  RNA_def_struct_name_property(srna, prop);

  /* enums */
  prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_enum_sdna(prop, NULL, "type");
  RNA_def_property_enum_items(prop, rna_enum_object_modifier_type_items);
  RNA_def_property_ui_text(prop, "Type", "");

  /* flags */
  prop = RNA_def_property(srna, "show_viewport", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "mode", eModifierMode_Realtime);
  RNA_def_property_ui_text(prop, "Realtime", "Display modifier in viewport");
  RNA_def_property_flag(prop, PROP_LIB_EXCEPTION);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_update(prop, 0, "rna_Modifier_update");
  RNA_def_property_ui_icon(prop, ICON_RESTRICT_VIEW_ON, 1);

  prop = RNA_def_property(srna, "show_render", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "mode", eModifierMode_Render);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Render", "Use modifier during render");
  RNA_def_property_ui_icon(prop, ICON_RESTRICT_RENDER_ON, 1);
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, NULL);

  prop = RNA_def_property(srna, "show_in_editmode", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "mode", eModifierMode_Editmode);
  RNA_def_property_ui_text(prop, "Edit Mode", "Display modifier in Edit mode");
  RNA_def_property_update(prop, 0, "rna_Modifier_update");
  RNA_def_property_ui_icon(prop, ICON_EDITMODE_HLT, 0);

  prop = RNA_def_property(srna, "show_on_cage", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "mode", eModifierMode_OnCage);
  RNA_def_property_ui_text(prop, "On Cage", "Adjust edit cage to modifier result");
  RNA_def_property_ui_icon(prop, ICON_MESH_DATA, 0);
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "show_expanded", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(
      prop, "rna_Modifier_show_expanded_get", "rna_Modifier_show_expanded_set");
  RNA_def_property_flag(prop, PROP_NO_DEG_UPDATE);
  RNA_def_property_boolean_sdna(prop, NULL, "ui_expand_flag", 0);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Expanded", "Set modifier expanded in the user interface");
  RNA_def_property_ui_icon(prop, ICON_DISCLOSURE_TRI_RIGHT, 1);
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, NULL);

  prop = RNA_def_property(srna, "is_active", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", eModifierFlag_Active);
  RNA_def_property_boolean_funcs(prop, NULL, "rna_Modifier_is_active_set");
  RNA_def_property_flag(prop, PROP_NO_DEG_UPDATE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Active", "The active modifier in the list");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, NULL);

  prop = RNA_def_boolean(srna,
                         "is_override_data",
                         false,
                         "Override Modifier",
                         "In a local override object, whether this modifier comes from the linked "
                         "reference object, or is local to the override");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", eModifierFlag_OverrideLibrary_Local);

  prop = RNA_def_property(srna, "use_apply_on_spline", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "mode", eModifierMode_ApplyOnSpline);
  RNA_def_property_ui_text(
      prop,
      "Apply on Spline",
      "Apply this and all preceding deformation modifiers on splines' points rather than "
      "on filled curve/surface");
  RNA_def_property_ui_icon(prop, ICON_SURFACE_DATA, 0);
  RNA_def_property_update(prop, 0, "rna_Modifier_update");

  prop = RNA_def_property(srna, "execution_time", PROP_FLOAT, PROP_TIME_ABSOLUTE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop,
      "Execution Time",
      "Time in seconds that the modifier took to evaluate. This is only set on evaluated objects. "
      "If multiple modifiers run in parallel, execution time is not a reliable metric");

  /* types */
  rna_def_modifier_subsurf(brna);
  rna_def_modifier_lattice(brna);
  rna_def_modifier_curve(brna);
  rna_def_modifier_build(brna);
  rna_def_modifier_mirror(brna);
  rna_def_modifier_decimate(brna);
  rna_def_modifier_wave(brna);
  rna_def_modifier_armature(brna);
  rna_def_modifier_hook(brna);
  rna_def_modifier_softbody(brna);
  rna_def_modifier_boolean(brna);
  rna_def_modifier_array(brna);
  rna_def_modifier_edgesplit(brna);
  rna_def_modifier_displace(brna);
  rna_def_modifier_uvproject(brna);
  rna_def_modifier_smooth(brna);
  rna_def_modifier_correctivesmooth(brna);
  rna_def_modifier_cast(brna);
  rna_def_modifier_meshdeform(brna);
  rna_def_modifier_particlesystem(brna);
  rna_def_modifier_particleinstance(brna);
  rna_def_modifier_explode(brna);
  rna_def_modifier_cloth(brna);
  rna_def_modifier_collision(brna);
  rna_def_modifier_bevel(brna);
  rna_def_modifier_shrinkwrap(brna);
  rna_def_modifier_mask(brna);
  rna_def_modifier_simpledeform(brna);
  rna_def_modifier_warp(brna);
  rna_def_modifier_multires(brna);
  rna_def_modifier_surface(brna);
  rna_def_modifier_fluid(brna);
  rna_def_modifier_solidify(brna);
  rna_def_modifier_screw(brna);
  rna_def_modifier_uvwarp(brna);
  rna_def_modifier_weightvgedit(brna);
  rna_def_modifier_weightvgmix(brna);
  rna_def_modifier_weightvgproximity(brna);
  rna_def_modifier_dynamic_paint(brna);
  rna_def_modifier_ocean(brna);
  rna_def_modifier_remesh(brna);
  rna_def_modifier_skin(brna);
  rna_def_modifier_laplaciansmooth(brna);
  rna_def_modifier_triangulate(brna);
  rna_def_modifier_meshcache(brna);
  rna_def_modifier_laplaciandeform(brna);
  rna_def_modifier_weld(brna);
  rna_def_modifier_wireframe(brna);
  rna_def_modifier_datatransfer(brna);
  rna_def_modifier_normaledit(brna);
  rna_def_modifier_meshseqcache(brna);
  rna_def_modifier_surfacedeform(brna);
  rna_def_modifier_weightednormal(brna);
  rna_def_modifier_nodes(brna);
  rna_def_modifier_mesh_to_volume(brna);
  rna_def_modifier_volume_displace(brna);
  rna_def_modifier_volume_to_mesh(brna);
}

#endif
