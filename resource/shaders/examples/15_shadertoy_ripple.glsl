// UNI2-Improvement-Mod shader pack: Shadertoy ripple
//
// A Shadertoy shader, pasted as it stands: one mainImage, iResolution, iTime and iChannel0. The mod
// wraps it - iChannel0 is the frame, fragCoord is in pixels with y up the way Shadertoy has it, and
// iTime is seconds since the mod loaded. Anything on Shadertoy that only reads iChannel0 as the
// screen drops in here; anything that wants a second buffer or a texture of its own does not.

#define STRENGTH 0.004
#define SPEED 1.6
#define RINGS 26.0

vec2 Ripple(vec2 uv, float time)
{
    vec2 centred = uv - 0.5;
    float radius = length(centred);
    float wave = sin(radius * RINGS - time * SPEED);

    return uv + normalize(centred + 1e-6) * wave * STRENGTH;
}

void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
    vec2 uv = fragCoord / iResolution.xy;
    vec2 warped = Ripple(uv, iTime);

    vec3 colour;
    colour.r = texture(iChannel0, warped + vec2(0.0006, 0.0)).r;
    colour.g = texture(iChannel0, warped).g;
    colour.b = texture(iChannel0, warped - vec2(0.0006, 0.0)).b;

    float shade = 1.0 - 0.25 * length(uv - 0.5);

    fragColor = vec4(colour * shade, 1.0);
}
