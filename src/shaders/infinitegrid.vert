VARYING vec3 gridWorldPosition;
VARYING vec2 gridPlanePosition;

void MAIN()
{
    gridWorldPosition = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
    gridPlanePosition = VERTEX.xy / 50.0;
    POSITION = MODELVIEWPROJECTION_MATRIX * vec4(VERTEX, 1.0);
}
