VARYING vec3 gridWorldPosition;
VARYING vec2 gridPlanePosition;

float screenGradient(float value)
{
    return max(length(vec2(dFdx(value), dFdy(value))), 0.000001);
}

float antialiasedLine(float distanceInPixels, float widthInPixels)
{
    float halfWidth = widthInPixels * 0.5;
    float feather = 0.75;
    float innerEdge = max(0.0, halfWidth - feather);
    float outerEdge = halfWidth + feather;
    return 1.0 - smoothstep(innerEdge, outerEdge, distanceInPixels);
}

float gridCoverage(vec2 worldPosition, float spacing, float widthInPixels)
{
    vec2 gridPosition = worldPosition / spacing;
    vec2 distanceToLine = abs(fract(gridPosition - 0.5) - 0.5);
    float xGradient = screenGradient(gridPosition.x);
    float yGradient = screenGradient(gridPosition.y);
    float distanceInPixels = min(distanceToLine.x / xGradient,
                                 distanceToLine.y / yGradient);
    return antialiasedLine(distanceInPixels, widthInPixels);
}

float axisCoverage(float coordinate, float widthInPixels)
{
    return antialiasedLine(abs(coordinate) / screenGradient(coordinate),
                           widthInPixels);
}

void MAIN()
{
    vec2 worldPosition = gridWorldPosition.xz;
    float spacing = max(gridSpacing, 0.000001);
    float worldUnitsPerPixel = max(screenGradient(worldPosition.x),
                                   screenGradient(worldPosition.y));

    float minorCoverage = gridCoverage(worldPosition, spacing, gridPixelWidth);
    float majorCoverage = gridCoverage(worldPosition, spacing * 10.0, gridPixelWidth);

    // Fade levels before their cells become too small to render cleanly.
    float minorCellPixels = spacing / max(worldUnitsPerPixel, 0.000001);
    float majorCellPixels = spacing * 10.0 / max(worldUnitsPerPixel, 0.000001);
    float minorVisibility = smoothstep(3.0, 8.0, minorCellPixels);
    float majorVisibility = smoothstep(2.0, 5.0, majorCellPixels);
    minorCoverage *= minorVisibility;
    majorCoverage *= majorVisibility;

    vec4 gridColor = minorGridColor;
    float gridCoverageValue = minorCoverage * minorGridColor.a;
    float majorCoverageValue = majorCoverage * majorGridColor.a;
    if (majorCoverageValue > gridCoverageValue) {
        gridColor = majorGridColor;
        gridCoverageValue = majorCoverageValue;
    }
    gridCoverageValue *= gridEnabled;

    // The X axis is z == 0; the Z axis is x == 0. Both use exactly the
    // same pixel width and horizon fade as the major grid lines.
    float xCoverage = axisCoverage(worldPosition.y, gridPixelWidth)
                    * majorVisibility * xGridAxisColor.a * axesEnabled;
    float zCoverage = axisCoverage(worldPosition.x, gridPixelWidth)
                    * majorVisibility * zGridAxisColor.a * axesEnabled;

    vec3 color = gridColor.rgb;
    float coverage = gridCoverageValue;
    if (xCoverage > coverage) {
        color = xGridAxisColor.rgb;
        coverage = xCoverage;
    }
    if (zCoverage > coverage) {
        color = zGridAxisColor.rgb;
        coverage = zCoverage;
    }

    // The quad tracks the view. Fading its distant edge hides the finite
    // backing geometry while retaining the appearance of an infinite grid.
    float edgeDistance = max(abs(gridPlanePosition.x),
                             abs(gridPlanePosition.y));
    coverage *= 1.0 - smoothstep(0.75, 0.98, edgeDistance);

    FRAGCOLOR = vec4(color, clamp(coverage, 0.0, 1.0));
}
