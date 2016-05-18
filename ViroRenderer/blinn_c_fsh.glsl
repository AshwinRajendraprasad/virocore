#version 300 es
#include blinn_functions_fsh

uniform highp vec3 camera_position;
uniform lowp vec3 ambient_light_color;
uniform lowp vec4 material_diffuse_surface_color;
uniform lowp float material_diffuse_intensity;
uniform lowp float material_alpha;
uniform lowp float material_shininess;

uniform VROSceneLightingUniforms lighting;
uniform sampler2D specular_texture;

in lowp vec3 v_normal;
in highp vec2 v_texcoord;
in highp vec3 v_surface_position;

out lowp vec4 frag_color;

void main() {
    VROBlinnLighting blinn;
    blinn.normal = v_normal;
    blinn.texcoord = v_texcoord;
    blinn.surface_position = v_surface_position;
    blinn.camera_position = camera_position;

    blinn.ambient_color = ambient_light_color * material_diffuse_surface_color.xyz;
    blinn.material_color = material_diffuse_surface_color;
    blinn.material_shininess = material_shininess;
    blinn.diffuse_intensity = material_diffuse_intensity;
    blinn.material_alpha = material_alpha;

    frag_color = blinn_lighting_diffuse_fixed(blinn, specular_texture, lighting);
}

