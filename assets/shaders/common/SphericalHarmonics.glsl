struct SHData {
    vec4 SH_L0;
    vec4 SH_L1_M_1;
    vec4 SH_L1_M0;
    vec4 SH_L1_M1;
    vec4 SH_L2_M_2;
    vec4 SH_L2_M_1;
    vec4 SH_L2_M0;
    vec4 SH_L2_M1;
    vec4 SH_L2_M2;
};

vec3 SH_HemisphereConvolve(SHData data, vec3 dir)
{
    float x = dir.x, y = dir.y, z = dir.z;

    float SH_L0     = 0.28209479177;
    float SH_L1_M_1 = 0.48860251190 * y;
    float SH_L1_M0  = 0.48860251190 * z;
    float SH_L1_M1  = 0.48860251190 * x;
    float SH_L2_M_2 = 1.09254843059 * x*y;
    float SH_L2_M_1 = 1.09254843059 * y*z;
    float SH_L2_M0  = 0.31539156525 * (3.0*z*z - 1.0);
    float SH_L2_M1  = 1.09254843059 * x*z;
    float SH_L2_M2  = 0.54627421529 * (x*x - y*y);

    vec3 res = vec3(0);

    float A0 = 3.141593, A1 = 2.094395, A2 = 0.785398;

    res += A0 * data.SH_L0.rgb     * SH_L0;
    res += A1 * data.SH_L1_M_1.rgb * SH_L1_M_1;
    res += A1 * data.SH_L1_M0.rgb  * SH_L1_M0;
    res += A1 * data.SH_L1_M1.rgb  * SH_L1_M1;
    res += A2 * data.SH_L2_M_2.rgb * SH_L2_M_2;
    res += A2 * data.SH_L2_M_1.rgb * SH_L2_M_1;
    res += A2 * data.SH_L2_M0.rgb  * SH_L2_M0;
    res += A2 * data.SH_L2_M1.rgb  * SH_L2_M1;
    res += A2 * data.SH_L2_M2.rgb  * SH_L2_M2;

    return res;
}