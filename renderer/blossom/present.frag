/* framework header */
#version 430
layout(location = 0) out vec4 fragColor;
layout(location = 0) uniform vec4 iResolution;
layout(location = 1) uniform int iFrame;
layout(location = 2) uniform int iMode;
layout(binding = 0) uniform sampler2D accumulatorTex;

// iqint2 with four output channels https://www.shadertoy.com/view/XlXcW4
const uint k = 1664525U;

vec4 hash43u( uvec3 x )
{
    x = ((x>>8U)^x.yzx)*k;
    x = ((x>>8U)^x.yzx)*k;
    x = ((x>>8U)^x.yzx)*k;
    return vec4(x,((x>>8U)^x.yzx)*k)/float(0xffffffffU);
}

// conversion between model1 and model2 latent space
// postprocessing transform for present.frag
mat4 model_pre2_w = mat4(1.290,0.117,0.514,-0.386,-0.304,1.041,-0.344,-0.590,0.063,0.644,0.640,-0.601,-0.067,-0.056,-0.014,0.433);
vec4 model_pre2_bias = vec4(0.404,0.641,-0.174,-0.759);

// The noise level here matches training code but can also be adjusted to taste.
const float middle_noise_mag = 0.25;

void main()
{
    if (iMode == 0) {
        vec4 tex = texelFetch(accumulatorTex,ivec2(gl_FragCoord.xy),0);
        tex.rgb = tex.rgb+vec3(0.5);
        fragColor = vec4(tex.rgb, 1.0);
    } else {
        vec2 uv = gl_FragCoord.xy / iResolution.xy;
        const float scale = 0.5;
        //uv = ((uv - vec2(0.5)) * scale) + vec2(0.5);
        uv *= scale;

        vec4 tex = texture(accumulatorTex, uv);
        tex = model_pre2_w * tex + model_pre2_bias;
        tex += middle_noise_mag * (hash43u(uvec3(gl_FragCoord.xy, iFrame)) - vec4(0.5));
        fragColor = tex;
    }
}
