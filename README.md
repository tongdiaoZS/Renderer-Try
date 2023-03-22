# Renderer-Try
Implemented a dependency-free rasterized renderer, which was originally done to understand how the graphics API works.


## Introduction
1. Bresenham's algorithm is used to draw line segments for efficiency;
2. The center of gravity coordinates are used to determine whether the point is inside the triangle to color the triangle;
3. Implemented perspective projection function;
4. Implemented texture mapping;
5. Implemented a basic rendering pipeline flow, including Vertex Shader, Rasterizer, Fragment Shader, Z-test.
