//
// Datum - core asset pack contents
//

//
// Copyright (c) 2015 Peter Niekamp
//

#pragma once

#include <cstdint>

namespace CoreAsset
{
  const uint32_t magic = 0x65726F43;
  const uint32_t version = 33;

  enum
  {
    catalog,
    white_diffuse,
    nominal_normal,
    unit_quad,
    unit_cube,
    unit_cone,
    unit_hemi,
    unit_sphere,
    line_quad,
    line_cube,
    line_cone,
    cluster_comp,
    prepass_frag,
    geometry_frag,
    terrain_frag,
    shadow_geom,
    shadow_frag,
    model_shadow_vert,
    model_prepass_vert,
    model_geometry_vert,
    model_spotmap_vert,
    actor_shadow_vert,
    actor_prepass_vert,
    actor_geometry_vert,
    actor_spotmap_vert,
    depth_mip_comp,
    ssao_comp,
    envbrdf_lut,
    lighting_comp,
    skybox_vert,
    skybox_frag,
    ocean_vert,
    ocean_frag,
    fogplane_vert,
    fogplane_frag,
    translucent_vert,
    translucent_frag,
    water_vert,
    water_frag,
    particle_vert,
    particle_frag,
    ssr_comp,
    default_skybox,
    bloom_luma_comp,
    bloom_hblur_comp,
    bloom_vblur_comp,
    luminance_comp,
    composite_vert,
    composite_frag,
    sprite_vert,
    sprite_frag,
    gizmo_vert,
    gizmo_frag,
    wireframe_vert,
    wireframe_geom,
    wireframe_frag,
    stencilmask_vert,
    stencilmask_frag,
    stencilfill_vert,
    stencilfill_frag,
    stencilpath_vert,
    stencilpath_geom,
    stencilpath_frag,
    line_vert,
    line_geom,
    line_frag,
    outline_vert,
    outline_geom,
    outline_frag,
    convolve_comp,
    project_comp,
    skybox_gen_comp,
    spotmap_src_vert,
    spotmap_src_frag,
    spotmap_frag,
    ocean_sim_comp,
    ocean_fftx_comp,
    ocean_ffty_comp,
    ocean_map_comp,
    ocean_gen_comp,
    wave_color,
    wave_normal,
    wave_foam,
    cloud_density,
    cloud_normal,
    noise_normal,
    default_material,
    default_particle,
    loader_image,
    test_image,
    debug_font,

    core_asset_count
  };
}
