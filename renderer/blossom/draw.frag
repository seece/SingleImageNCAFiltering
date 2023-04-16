/* framework header */
#version 430
layout(location = 0) out vec4 fragColor;
layout(location = 0) uniform vec4 iResolution;
layout(location = 1) uniform int iFrame;
layout(location = 3) uniform int iPhase;
layout(binding = 0) uniform sampler2D pastTex;

const float pi = acos(-1.);

// µNCA based on https://www.shadertoy.com/view/slGGzD and https://www.shadertoy.com/view/NlG3Wm

// update rules for draw.frag
vec4 model1(vec4 s, vec4 p) {
    return 1e-03*(vec4(20,3,43,8)+
      mat4(74,0,-61,-2,-11,107,-42,47,-17,-11,21,120,-33,32,53,-13)*s+
      mat4(98,-46,-27,-32,-23,41,-4,17,0,0,1,3,1,-6,3,-9)*p+
      mat4(48,-7,-12,75,-1,-10,-19,-26,9,7,-14,100,12,-29,-7,-19)*abs(s)+
      mat4(-9,-11,-1,-1,28,-20,4,-2,-12,5,5,-31,-10,4,7,-17)*abs(p));
  }

vec4 model2(vec4 s, vec4 p) {
    return 1e-03*(vec4(-13,-16,-29,-14)+
      mat4(-126,18,0,22,13,-97,69,42,9,-29,-117,-21,8,-16,14,-89)*s+
      mat4(27,-79,-2,-5,6,51,-15,27,2,-5,9,2,0,-3,1,0)*p+
      mat4(-7,-5,2,-30,-8,54,49,49,-27,-29,-49,3,-11,-13,5,7)*abs(s)+
      mat4(-13,-11,28,15,7,3,-3,-6,-6,4,16,2,-1,0,2,1)*abs(p));
  }

// preprocessing for draw.frag
mat4 model_pre1_w = mat4(1.335,-0.660,-0.380,-0.241,0.035,1.376,-0.066,-0.054,0.183,0.075,0.868,-0.559,0.246,-0.435,-0.037,0.008);
vec4 model_pre1_bias = vec4(-0.031,0.199,-0.347,0.007);
mat4x3 model_post2_w = mat4x3(0.909,0.094,0.089,0.061,1.010,0.255,0.053,0.167,0.928,0.118,0.309,-0.063);

const int MAT_SKY = 99;
const int MAT_ROCK = 1;
const int MAT_RED = 2;
const int MAT_DIRT = 3;

int global_seed;

// iqint2 with four output channels https://www.shadertoy.com/view/XlXcW4
const uint k = 1664525U;

vec4 hash43u( uvec3 x )
{
    x = ((x>>8U)^x.yzx)*k;
    x = ((x>>8U)^x.yzx)*k;
    x = ((x>>8U)^x.yzx)*k;
    return vec4(x,((x>>8U)^x.yzx)*k)/float(0xffffffffU);
}

vec4 smoothnoise2d(vec2 x) {
    vec2 lower = floor(x);
    vec2 t = smoothstep(0., 1.0, fract(x));
    uvec3 o = uvec3(1,0,0);
    uvec3 xf = uvec3(ivec3(lower,0)); // cast float->int->uint so that negative values wrap as expected
    return mix(mix(hash43u(xf),hash43u(xf+o.xyy),t.x), mix(hash43u(xf+o.yxy),hash43u(xf+o.xxy),t.x), t.y);
}

vec4 fbm2d(vec2 x) {
    vec4 nn =vec4(0.0);
    float a=1.0;
    const int NUM=8;
    for (int i=0;i<NUM;i++) {
        nn+=a*(smoothnoise2d(x)-vec4(0.5));
        x*=2.;
        a*=0.7;
    }
    return nn/float(NUM);
}

float sdBox( vec3 p, vec3 b )
{
  vec3 q = abs(p) - b;
  return length(max(q,0.0)) + min(max(q.x,max(q.y,q.z)),0.0);
}

float sdSphere(vec3 p, float r) {
    return length(p)-r;
}

float sdEllipsoid( vec3 p, vec3 r )
{
  float k0 = length(p/r);
  float k1 = length(p/(r*r));
  return k0*(k0-1.0)/k1;
}

float sdTorus( vec3 p, vec2 t )
{
  vec2 q = vec2(length(p.xz)-t.x,p.y);
  return length(q)-t.y;
}

float combine(float old, float new, int newmat, inout int mat) {
    if (new < old) {
        mat = newmat;
        return new;
    }

    return old;
}

void rotate(inout vec2 p, float a) {
  p = mat2(cos(a), sin(a), -sin(a), cos(a)) * p;
}

float scene_mat(vec3 p, inout int material) {
    material = MAT_SKY;
    float dist=1e9;
    float shapex = -0.2;
    float shapez = -8.;
    vec3 po=p - vec3(-0.2, 1.5, -10.0);
    rotate(po.xz, 0.6);
    rotate(po.yz, -0.3);
    float box = sdBox(po, vec3(1.));
    box = max(box, -sdBox(po-vec3(-0.5, 0.5, 0.5), vec3(0.51)));

    vec3 ps = p - vec3(1.1, 0.6, -5.0);
    float sphere = sdSphere(ps, 0.6);

    dist = combine(dist, box, MAT_ROCK, material);
    dist = combine(dist, sphere, MAT_RED, material);
    dist = combine(dist, p.y, MAT_DIRT, material);

    return dist;
}

float scene(vec3 p) {
    int temp;
    return scene_mat(p, temp);
}

vec3 calcnormal(vec3 p)
{
    vec2 e = vec2(1,-1)*1e-3;
    return normalize(scene(p+e.xxy)*e.xxy+scene(p+e.xyx)*e.xyx+
    scene(p+e.yxx)*e.yxx+scene(p+e.y)*e.y);
}

float march_ray(vec3 p, vec3 dir, float max_t, int max_steps) {
    vec3 o=p;
    float t=0.0;
    for (int i=0;i<max_steps;i++) {
        p = o+t*dir;
        float d = scene(p);
        t += d;
        if (d < 1e-3) {
            break;
        }
    }
    return t;
}

float trace_ao(vec3 p, vec3 d) {
    const int numRays = 10;
    float opening=0.0;
    const float max_t=40.0;
    for (int i=0;i<numRays;i++) {
        vec4 noise = hash43u(uvec3(gl_FragCoord.xy,i+global_seed*99));
        vec3 o=p+5e-3*d;
        float t = march_ray(o, normalize(d + 2.*noise.xyz-vec3(1.0)), max_t, 20);
        opening += min(max_t,t) / max_t;

    }
    return opening/float(numRays);
}

vec4 render(vec2 coord, vec3 noise, out int material, out vec3 position, out vec3 normal) {
    vec2 uv = (coord-iResolution.xy*0.25)/iResolution.x*0.5; // quarter rez rendering
    float pixelSize = 1.0/iResolution.x;
    uv.xy += pixelSize * (vec2(-0.5)+hash43u(uvec3(iResolution.yx, global_seed+999)).xy);
    vec3 dir = normalize(vec3(uv, -0.32) - vec3(0., 0.04, 0.0));

    vec3 cam = vec3(0.5,3.0,5.0);

    float t = 0.1;

    material = MAT_SKY;
    vec3 color = vec3(0.);
    float sky_shade = pow(max(0, 0.13-dir.y)*4., 0.8);

    color += sky_shade * vec3(1.0, 1.0, 1.0);

    float out_shade = sky_shade;

    #define MAX_ITER (700)
    int i;
    vec3 p;
    bool hit=false;
    for (i=0;i<MAX_ITER;i++) {
        p = cam+t*dir;
        float d = scene_mat(p, material);
        if (d < 1e-3) {
            hit=true;
            break;
        }
        t += d;
    }


    if (hit) {
        vec3 to_light = normalize(vec3(-0.9, 0.8, 1.3));

        if (material != MAT_SKY) {
            vec3 base_color;
            float specular_exp;

            if (material == MAT_DIRT) {
                base_color = vec3(0.6, 0.5, 0.3);
                specular_exp = 999.0;
            } else if (material == MAT_ROCK) {
                base_color = vec3(0.4, 0.5, 0.8);
                base_color *= 1+0.8*fbm2d(p.xy).rgb;
                specular_exp = 5.0;
            } else if (material == MAT_RED) {
                base_color = 1.2*vec3(0.5, 0.2, 0.1);
                specular_exp = 20.0;
            }

            normal = calcnormal(p);
            float ao = max(0.01, trace_ao(p - 1e-3*dir, normal));

            const float sun_max_t = 10.0;

            float sun = 1.0f;
            if (march_ray(p-1e-2*dir, to_light, sun_max_t, 100) < sun_max_t) {
                sun = 0.0;
            }

            float ambient_term = 0.5*ao;
            float ndotl = max(0., dot(normal, to_light));
            float diffuse_term = 0.5 * sun * ndotl;
            float specular_term = 0.5*pow(ndotl, specular_exp);

            out_shade = 0.25 * ambient_term + 0.75 * diffuse_term + 0.5 * specular_term;

            vec3 ambient = ambient_term * vec3(0.9, 0.9, 1.0);

            color = (ambient + diffuse_term) * base_color + specular_term*vec3(1.0);
        } else {
            color = vec3(1.0, 0.0, 0.0);
        }
    }

    // color = vec3(uv,0);
    position = p;

    const float margin=220;
    if (gl_FragCoord.x > 960-margin || gl_FragCoord.x<margin || gl_FragCoord.y > 540) {
        color=mix(vec3(0.1), color, 0.1);
    }

    color = (color - vec3(0.5));

    return vec4(color, out_shade);
}

// Read from past state
vec4 R(float x, float y) {
    return texture(pastTex, vec2(x, y));
}


const float input_noise_mag = 0.25;

void main()
{
    vec2 dp = 1.0/iResolution.xy;
    vec2 uvpos = gl_FragCoord.xy*dp; // [0, 1]^2 uvs
    global_seed = 0;

    float x=uvpos.x, y=uvpos.y;
    float l=x-dp.x, r=x+dp.x, u=y-dp.y, d=y+dp.y;
    vec4 s = R(x,y);

    // perception
    vec4 p = R(l,u)*vec4(1,1,-1, 1) + R(x,u)*vec4(2,2,0, 2) + R(r,u)*vec4(1,1,1, 1)
        + R(l,y)*vec4(2,2,-2, 0) +  s*vec4(-12,-12,0, 0) + R(r,y)*vec4(2,2,2, 0)
        + R(l,d)*vec4(1,1,-1,-1) + R(x,d)*vec4(2,2,0,-2) + R(r,d)*vec4(1,1,1,-1);

    vec4 ds;

    int material=0;
    vec3 world_p, world_n;
    // vec4 world = render_plane(gl_FragCoord.xy, mask.xyz, material, world_p, world_n);

    // Phase 0: first 32 iterations
    // Phase 1: second 32 iterations after bilinear upscale
    if (iPhase == 0) {
        ds = model1(s, p);
    } else if (iPhase == 1) {
        // present.frag has applied latent space conversion between the two models
        ds = model2(s, p);
    }

    vec4 updated = s+ds;


    if (iFrame <= 0) {
        vec3 pos=vec3(0.), normal=vec3(0.);
        vec4 world=vec4(0.);

        const int SPP=32;

        for (int s=0;s<SPP;s++) {
            global_seed = s;
            vec3 spos, snormal;
            vec4 sworld = render(gl_FragCoord.xy, vec3(0.0), material, spos, snormal);
            pos += spos;
            normal += snormal;
            world += sworld;
        }

        pos/=SPP;
        normal/=SPP;
        world/=SPP;
        // world.xyz = world.aaa; // show shade


        float radial_depth = 1.0/length(pos);

        #if 0
        // gbuffer features where world.a is computed 'shade' brightness value
        vec4 input_raw = vec4(normal.xy, radial_depth, world.a);
        #elif 0
        // rgbd features. "world.rgb" should already be centered around zero
        vec4 input_raw = vec4(world.rgb, radial_depth);
        #else
        // r,b,nx,d features. "world.rgb" should already be centered around zero
        vec4 input_raw = vec4(world.rb, normal.x, radial_depth);
        #endif

        if (iFrame == -2) {
            // clean rendering with lighting
            updated = vec4(world.xyz, radial_depth*6);

            // HACK show input
            //input_raw += input_noise_mag * (hash43u(uvec3(gl_FragCoord.xy, iFrame)) - 0.5);
            //vec4 input_features = model_pre1_w * input_raw + model_pre1_bias;
            //updated = input_features;

            //input_raw += input_noise_mag * (hash43u(uvec3(gl_FragCoord.xy, iFrame)) - 0.5);
            //vec4 input_features = model_pre1_w * input_raw + model_pre1_bias;
            //updated = input_features;
        // } else if (iFrame == -1 || iFrame == 0) {
        } else if (iFrame == -1 || iFrame == 0) {
            // a clean feature rendering
            updated = input_raw;
        }
        if (iFrame == 0) {

            // a noisy feature rendering that starts processing
            input_raw += input_noise_mag * (hash43u(uvec3(gl_FragCoord.xy, iFrame)) - 0.5);
            vec4 input_features = model_pre1_w * input_raw + model_pre1_bias;
            updated = input_features;
        }
    }

    if (iPhase == 1 && iFrame == 63) {
        // Apply model 2 postproc's linear transform to the outgoing value on the last frame
        updated.xyz = model_post2_w * updated;
    }

    // The training code penalizes values outside the [-1.0, 1.0] range but looks like this
    // clamp still breaks something important so it's not used. We do have 32-bit floats so it's fine.
    //updated = clamp(updated, -1.5, 1.5);
    fragColor = updated;
}
