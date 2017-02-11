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
  const uint32_t version = 18;

  enum
  {
    catalog,
    white_diffuse,
    nominal_normal,
    unit_quad,
    unit_cube,
    unit_cone,
    unit_sphere,
    line_quad,
    line_cube,
    shadow_vert,
    shadow_geom,
    shadow_frag,
    depth_vert,
    depth_frag,
    geometry_vert,
    geometry_frag,
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
    ssao_comp,
    envbrdf_lut,
    lighting_comp,
    ssr_comp,
    skybox_vert,
    skybox_frag,
    skybox_comp,
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
    wave_color,
    wave_normal,
    default_material,
    default_particle,
    loader_image,
    test_image,
    debug_font,
  };
}
