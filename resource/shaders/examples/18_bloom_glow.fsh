// UNI2-Improvement-Mod shader pack: Bloom glow
//
// Old style GLSL, the shape a .fsh usually has: a varying instead of an in, gl_FragColor instead of
// an out, texture2D instead of texture, and precision qualifiers. All four are rewritten on the way
// in, so a shader written for GLSL ES or for a 2010 era renderer still loads.

#ifdef GL_ES
precision mediump float;
#endif

uniform sampler2D u_texture;
uniform vec2 u_resolution;

varying vec2 v_texCoord;

#define THRESHOLD 0.62
#define GLOW 0.75
#define RADIUS 2.5

vec3 Bright(vec2 coordinate)
{
    vec3 colour = texture2D(u_texture, coordinate).rgb;
    float luma = dot(colour, vec3(0.2126, 0.7152, 0.0722));

    return colour * max(luma - THRESHOLD, 0.0) / max(1.0 - THRESHOLD, 0.001);
}

void main()
{
    vec2 step = RADIUS / u_resolution;

    vec3 glow = Bright(v_texCoord) * 4.0;

    glow += Bright(v_texCoord + vec2(step.x, 0.0)) * 2.0;
    glow += Bright(v_texCoord - vec2(step.x, 0.0)) * 2.0;
    glow += Bright(v_texCoord + vec2(0.0, step.y)) * 2.0;
    glow += Bright(v_texCoord - vec2(0.0, step.y)) * 2.0;

    glow += Bright(v_texCoord + step);
    glow += Bright(v_texCoord - step);
    glow += Bright(v_texCoord + vec2(step.x, -step.y));
    glow += Bright(v_texCoord + vec2(-step.x, step.y));

    glow /= 16.0;

    vec3 colour = texture2D(u_texture, v_texCoord).rgb;

    gl_FragColor = vec4(clamp(colour + glow * GLOW, 0.0, 1.0), 1.0);
}
