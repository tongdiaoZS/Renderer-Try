#include <vector>
#include <iostream>
#include <algorithm>
#include "tgaimage.h"
#include "model.h"
#include "geometry.h"
#include "our_gl.h"

Model *model     = NULL;
const int width  = 800;
const int height = 800;

Vec3f light_dir(0,1,1);
Vec3f       eye(1,0.5,1.5);
Vec3f    center(0,0,0);
Vec3f        up(0,1,0);

//Gouradud Shader
struct GouraudShader : public IShader {
    //vertex shader will write data into  varying_intensity
    //fragment shader read data from varying_intensity
    Vec3f varying_intensity; 
    mat<2, 3, float> varying_uv;
    //input (face index，point index)
    virtual Vec4f vertex(int iface, int nthvert) {
        // Read the corresponding vertices of the model according to the face number and vertex number, and expand them to 4 dimensions 
        Vec4f gl_Vertex = embed<4>(model->vert(iface, nthvert));
        varying_uv.set_col(nthvert, model->uv(iface, nthvert));
        // tranform
        mat<4, 4, float> uniform_M = Projection * ModelView;
        mat<4, 4, float> uniform_MIT = ModelView.invert_transpose();
        gl_Vertex = Viewport* uniform_M *gl_Vertex;
        // Calculating light intensity（point's normal * light_dir）
        Vec3f normal = proj<3>(embed<4>(model->normal(iface, nthvert))).normalize();
        varying_intensity[nthvert] = std::max(0.f, model->normal(iface, nthvert) *light_dir); // get diffuse lighting intensity
        return gl_Vertex;
    }
    // 根据传入的coordinates，color，varying_intensity计算出当前像素的颜色
    virtual bool fragment(Vec3f bar, TGAColor &color) {
        Vec2f uv = varying_uv * bar;
        TGAColor c = model->diffuse(uv);
        float intensity = varying_intensity*bar;
        color = c*intensity; 
        return false;                              
    }
};

// The light intensity within a certain threshold is given as a replacement
struct ToonShader : public IShader {
    mat<3, 3, float> varying_tri;
    Vec3f          varying_ity;

    virtual ~ToonShader() {}

    virtual Vec4f vertex(int iface, int nthvert) {
        Vec4f gl_Vertex = embed<4>(model->vert(iface, nthvert));
        gl_Vertex = Projection * ModelView * gl_Vertex;
        varying_tri.set_col(nthvert, proj<3>(gl_Vertex / gl_Vertex[3]));

        varying_ity[nthvert] = model->normal(iface, nthvert) * light_dir;

        gl_Vertex = Viewport * gl_Vertex;
        return gl_Vertex;
    }

    virtual bool fragment(Vec3f bar, TGAColor& color) {
        float intensity = varying_ity * bar;
        if (intensity > .85) intensity = 1;
        else if (intensity > .60) intensity = .80;
        else if (intensity > .45) intensity = .60;
        else if (intensity > .30) intensity = .45;
        else if (intensity > .15) intensity = .30;
        color = TGAColor(255, 155, 0) * intensity;
        return false;
    }
};

// No interpolation of the normal vector, which is derived from the fork product of the triangle sides
struct FlatShader : public IShader {
    // 3 points' data
    mat<3, 3, float> varying_tri;

    virtual ~FlatShader() {}

    virtual Vec4f vertex(int iface, int nthvert) {
        Vec4f gl_Vertex = embed<4>(model->vert(iface, nthvert));
        gl_Vertex = Projection * ModelView * gl_Vertex;
        varying_tri.set_col(nthvert, proj<3>(gl_Vertex / gl_Vertex[3]));
        gl_Vertex = Viewport * gl_Vertex;
        return gl_Vertex;
    }

    virtual bool fragment(Vec3f bar, TGAColor& color) {

        Vec3f n = cross(varying_tri.col(1) - varying_tri.col(0), varying_tri.col(2) - varying_tri.col(0)).normalize();
        float intensity = n * light_dir;
        color = TGAColor(255, 255, 255) * intensity;
        return false;
    }
};

//Phong shader
struct PhongShader : public IShader {
    mat<2, 3, float> varying_uv;  // same as above
    mat<4, 4, float> uniform_M = Projection * ModelView;
    mat<4, 4, float> uniform_MIT = ModelView.invert_transpose();
    virtual Vec4f vertex(int iface, int nthvert) {
        varying_uv.set_col(nthvert, model->uv(iface, nthvert));
        Vec4f gl_Vertex = embed<4>(model->vert(iface, nthvert)); // read the vertex from .obj file
        return Viewport * Projection * ModelView * gl_Vertex; // transform it to screen coordinates
    }
    virtual bool fragment(Vec3f bar, TGAColor& color) {
        Vec2f uv = varying_uv * bar;
        Vec3f n = proj<3>(uniform_MIT * embed<4>(model->normal(uv))).normalize();
        Vec3f l = proj<3>(uniform_M * embed<4>(light_dir)).normalize();
        Vec3f r = (n * (n * l * 2.f) - l).normalize();   // reflected light
        float spec = pow(std::max(r.z, 0.0f), model->specular(uv));
        float diff = std::max(0.f, n * l);
        TGAColor c = model->diffuse(uv);
        color = c;
        for (int i = 0; i < 3; i++) color[i] = std::min<float>(5 + c[i] * (diff + .6 * spec), 255);
        return false;
    }
};

int main(int argc, char** argv) {
    //load model
    if (2==argc) {
        model = new Model(argv[1]);
    } else {
        model = new Model("obj/african_head.obj");
    }
    //init Transformation matrix, projection matrix, view matrix
    lookat(eye, center, up);
    projection(-1.f/(eye-center).norm());
    viewport(width / 8, height / 8, width * 3 / 4, height * 3 / 4);
    light_dir.normalize();
    //init image and Zbuffer
    TGAImage image  (width, height, TGAImage::RGB);
    TGAImage zbuffer(width, height, TGAImage::GRAYSCALE);
    // 实例化
    //GouraudShader shader;
    // 实例化
    //PhongShader shader;
    // 实例化
	ToonShader shader;
    // 以模型面作为循环控制量
    for (int i=0; i<model->nfaces(); i++) {
        Vec4f screen_coords[3];
        for (int j=0; j<3; j++) {
            // Reading model vertices through vertex shader
            // 变换顶点坐标到屏幕坐标（视角矩阵*投影矩阵*变换矩阵*v） ***其实并不是真正的屏幕坐标，因为没有除以最后一个分量
            // Calculation of light intensity
            screen_coords[j] = shader.vertex(i, j);
        }
        // A triangle rasterization is completed after traversing 3 vertices
        // Draw triangles, triangle internal coloring of triangles by slice shader
        triangle(screen_coords, shader, image, zbuffer);
    }

    image.  flip_vertically();
    zbuffer.flip_vertically();
    image.  write_tga_file("output.tga");
    zbuffer.write_tga_file("zbuffer.tga");

    delete model;
    return 0;
}
