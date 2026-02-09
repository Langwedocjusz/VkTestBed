#ifndef PI
#define PI 3.1415926535
#endif

float stripes(float t, float freq, float width, float eps)
{
    float u = sin(freq * PI * t);
    
    float adj_width = freq * width;

    float lines = smoothstep(-adj_width-eps, -adj_width+eps, u)
                * smoothstep(-adj_width-eps, -adj_width+eps,-u);
    
    return 1.0 - lines;
}

float multistripes(float t)
{
    const float eps = 0.02;
    const float width = 0.1;
    
    float main = stripes(t, 1.0, width, eps);
    float secondary = stripes(t, 10.0, 0.2*width, eps);
    
    secondary = mix(secondary, 1.0, 0.5);
    
    return main*secondary;
}

vec3 debug_grid(vec2 uv, vec3 ok_col)
{
    bool ok_x = (0.0 < uv.x && uv.x < 1.0);
    bool ok_y = (0.0 < uv.y && uv.y < 1.0);
    
    vec3 base_col = (ok_x && ok_y) ? ok_col : vec3(0.95);

    float grid = multistripes(uv.x) * multistripes(uv.y);

    return base_col * grid;
}