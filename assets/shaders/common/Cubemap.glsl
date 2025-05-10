vec3 GetCubemapDir(vec2 ts, uint sideId)
{
    vec3 dir;

    #define RIGHT 0
    #define LEFT 1
    #define BOTTOM 2
    #define TOP 3
    #define FRONT 4
    #define BACK 5

    switch(sideId)
    {
        case TOP:    {dir = vec3(ts.x, -1.0, -ts.y); break;}
        case BOTTOM: {dir = vec3(ts.x,  1.0, ts.y); break;}
        case RIGHT:  {dir = vec3( 1.0, -ts.y, -ts.x); break;}
        case LEFT:   {dir = vec3(-1.0, -ts.y, ts.x); break;}
        case FRONT:  {dir = vec3(ts.x, -ts.y,  1.0); break;}
        case BACK:   {dir = vec3(-ts.x, -ts.y, -1.0); break;}
    }

    return normalize(dir);
}