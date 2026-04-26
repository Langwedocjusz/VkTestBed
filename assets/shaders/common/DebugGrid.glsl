#ifndef PI
#define PI 3.1415926535
#endif

float Stripes(float t, float freq, float width, float eps)
{
    float u = sin(freq * PI * t);
    
    float adj_width = freq * width;

    float lines = smoothstep(-adj_width-eps, -adj_width+eps, u)
                * smoothstep(-adj_width-eps, -adj_width+eps,-u);
    
    return 1.0 - lines;
}

float Multistripes(float t)
{
    const float eps = 0.02;
    const float width = 0.05;
    
    float main = Stripes(t, 1.0, width, eps);
    float secondary = Stripes(t, 10.0, 0.2*width, eps);
    float tertiary = Stripes(t, 100.0, 0.04*width, eps);
    
    secondary = mix(secondary, 1.0, 0.5);
    tertiary = mix(tertiary, 1.0, 0.75);
    
    return main*secondary*tertiary;
}

vec3 DebugGrid(vec2 uv, vec3 ok_col)
{
    bool ok_x = (0.0 < uv.x && uv.x < 1.0);
    bool ok_y = (0.0 < uv.y && uv.y < 1.0);
    
    vec3 base_col = (ok_x && ok_y) ? ok_col : vec3(0.95);

    float grid = Multistripes(uv.x) * Multistripes(uv.y);

    return base_col * grid;
}