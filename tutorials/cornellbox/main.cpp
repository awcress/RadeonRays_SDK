#include "radeon_rays.h"
#include <GL/glew.h>
#include <GLUT/GLUT.h>
#include <cassert>
#include <iostream>
#include <memory>
#include "../tools/shader_manager.h"
#include "../tools/tiny_obj_loader.h"

using namespace RadeonRays;
using namespace tinyobj;

namespace {
    std::vector<shape_t> g_objshapes;
    std::vector<material_t> g_objmaterials;
    
    GLuint g_vertex_buffer, g_index_buffer;
    GLuint g_texture;
    int g_window_width = 640;
    int g_window_height = 480;
    std::unique_ptr<ShaderManager> g_shader_manager;
}

void CheckGlErr()
{
    auto err = glGetError();
    while (err != GL_NO_ERROR)
    {
        std::cout << "err " << err << std::endl;
        err = glGetError();
    }
}

void InitData()
{
    std::string basepath = "../../Resources/CornellBox/"; 
    std::string filename = basepath + "orig.objm";
    std::string res = LoadObj(g_objshapes, g_objmaterials, filename.c_str(), basepath.c_str());
    if (res != "")
    {
        throw std::runtime_error(res);
    }

}

float3 ConvertFromBarycentric(const float* vec, const int* ind, int prim_id, const float4& uvwt)
{
    float3 a = { vec[ind[prim_id * 3] * 3],
                vec[ind[prim_id * 3] * 3 + 1],
                vec[ind[prim_id * 3] * 3 + 2], };

    float3 b = { vec[ind[prim_id * 3 + 1] * 3],
                vec[ind[prim_id * 3 + 1] * 3 + 1],
                vec[ind[prim_id * 3 + 1] * 3 + 2], };

    float3 c = { vec[ind[prim_id * 3 + 2] * 3],
                vec[ind[prim_id * 3 + 2] * 3 + 1],
                vec[ind[prim_id * 3 + 2] * 3 + 2], };
    return a * (1 - uvwt.x - uvwt.y) + b * uvwt.x + c * uvwt.y;
}

void InitGl()
{
    g_shader_manager.reset(new ShaderManager());

    glClearColor(0.0, 0.0, 0.0, 0.0);
    glCullFace(GL_NONE);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);

    glGenBuffers(1, &g_vertex_buffer);
    glGenBuffers(1, &g_index_buffer);

    // create Vertex buffer
    glBindBuffer(GL_ARRAY_BUFFER, g_vertex_buffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_index_buffer);

    float quad_vdata[] =
    {
        -1, -1, 0.5, 0, 0,
        1, -1, 0.5, 1, 0,
        1, 1, 0.5, 1, 1,
        -1, 1, 0.5, 0, 1
    };

    GLshort quad_idata[] =
    {
        0, 1, 3,
        3, 1, 2
    };

    // fill data
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vdata), quad_vdata, GL_STATIC_DRAW);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(quad_idata), quad_idata, GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);


    glGenTextures(1, &g_texture);
    glBindTexture(GL_TEXTURE_2D, g_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, g_window_width, g_window_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    glBindTexture(GL_TEXTURE_2D, 0);
}

void DrawScene()
{

    glDisable(GL_DEPTH_TEST);
    glViewport(0, 0, g_window_width, g_window_height);

    glClear(GL_COLOR_BUFFER_BIT);

    glBindBuffer(GL_ARRAY_BUFFER, g_vertex_buffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_index_buffer);

    GLuint program = g_shader_manager->GetProgram("simple");
    glUseProgram(program);

    GLuint texloc = glGetUniformLocation(program, "g_Texture");
    assert(texloc >= 0);

    glUniform1i(texloc, 0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_texture);

    GLuint position_attr = glGetAttribLocation(program, "inPosition");
    GLuint texcoord_attr = glGetAttribLocation(program, "inTexcoord");

    glVertexAttribPointer(position_attr, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 5, 0);
    glVertexAttribPointer(texcoord_attr, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 5, (void*)(sizeof(float) * 3));

    glEnableVertexAttribArray(position_attr);
    glEnableVertexAttribArray(texcoord_attr);

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr);

    glDisableVertexAttribArray(texcoord_attr);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glUseProgram(0);

    glFinish();
    

    glutSwapBuffers();
}

int main(int argc, char* argv[])
{
    // GLUT Window Initialization:
    glutInit(&argc, (char**)argv);
    glutInitWindowSize(640, 480);
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
    glutCreateWindow("Triangle");
#ifndef __APPLE__
    GLenum err = glewInit();
    if (err != GLEW_OK)
    {
        std::cout << "GLEW initialization failed\n";
        return -1;
    }
#endif

    InitGl();
    InitData();

    int nativeidx = -1;

    // Always use OpenCL
    IntersectionApi::SetPlatform(DeviceInfo::kOpenCL);

    for (auto idx = 0U; idx < IntersectionApi::GetDeviceCount(); ++idx)
    {
        DeviceInfo devinfo;
        IntersectionApi::GetDeviceInfo(idx, devinfo);

        if (devinfo.type == DeviceInfo::kGpu && nativeidx == -1)
        {
            nativeidx = idx;
        }
    }

    assert(nativeidx != -1);
    IntersectionApi* api = IntersectionApi::Create(nativeidx);
    
    //Shapes
    float3 light = {-0.01f, 1.9f, 0.1f};
    for (int id = 0; id < g_objshapes.size(); ++id)
    {
        shape_t& objshape = g_objshapes[id];
        float* vertdata = objshape.mesh.positions.data();
        int nvert = objshape.mesh.positions.size();
        int* indices = objshape.mesh.indices.data();
        int nfaces = objshape.mesh.indices.size() / 3;
        Shape* shape = api->CreateMesh(vertdata, nvert, 3 * sizeof(float), indices, 0, nullptr, nfaces);

        assert(shape != nullptr);
        api->AttachShape(shape);
        shape->SetId(id);
    }

    const int k_raypack_size = g_window_height * g_window_width;
    // Rays
    std::vector<ray> rays(k_raypack_size);

    // Prepare the ray
    float4 camera_pos = { 0.f, 1.f, 3.f, 1000.f };
    for (int i = 0; i < g_window_height; ++i)
        for (int j = 0; j < g_window_width; ++j)
        {
            const float xstep = 2.f / (float)g_window_width;
            const float ystep = 2.f / (float)g_window_height;
            float x = -1.f + xstep * (float)j;
            float y = ystep * (float)i;
            float z = 1.f;
            rays[i * g_window_width + j].o = camera_pos;
            rays[i * g_window_width + j].d = float3(x - camera_pos.x, y - camera_pos.y, z - camera_pos.z);
        }

    // Intersection and hit data
    std::vector<Intersection> isect(k_raypack_size);

    Buffer* ray_buffer = api->CreateBuffer(rays.size() * sizeof(ray), rays.data());
    Buffer* isect_buffer = api->CreateBuffer(isect.size() * sizeof(Intersection), nullptr);
    
    api->Commit();
    api->QueryIntersection(ray_buffer, k_raypack_size, isect_buffer, nullptr, nullptr);
    Event* e = nullptr;
    Intersection* tmp = nullptr;
    api->MapBuffer(isect_buffer, kMapRead, 0, isect.size() * sizeof(Intersection), (void**)&tmp, &e);
    e->Wait();
    api->DeleteEvent(e);
    e = nullptr;
    
    for (int i = 0; i < k_raypack_size; ++i)
    {
        isect[i] = tmp[i];
    }

    std::vector<unsigned char> tex_data(k_raypack_size * 4);
    for (int i = 0; i < k_raypack_size ; ++i)
    {
        int shape_id = isect[i].shapeid;
        int prim_id = isect[i].primid;

        if (shape_id != kNullId && prim_id != kNullId)
        {
            mesh_t& mesh = g_objshapes[shape_id].mesh;
            int mat_id = mesh.material_ids[prim_id];
            material_t& mat = g_objmaterials[mat_id];

            float3 diff_col = { mat.diffuse[0],
                                mat.diffuse[1],
                                mat.diffuse[2] };

            float3 pos = ConvertFromBarycentric(mesh.positions.data(), mesh.indices.data(), prim_id, isect[i].uvwt);
            float3 norm = ConvertFromBarycentric(mesh.normals.data(), mesh.indices.data(), prim_id, isect[i].uvwt);
            norm.normalize();
            float3 col = { 0.f, 0.f, 0.f };
            float3 light_dir = light - pos;
            light_dir.normalize();
            float dot_prod = dot(norm, light_dir);

            if (dot_prod > 0)
                col += dot_prod * diff_col;

            tex_data[i * 4] = col[0] * 255;
            tex_data[i * 4 + 1] = col[1] * 255;
            tex_data[i * 4 + 2] = col[2] * 255;
            tex_data[i * 4 + 3] = 255;
        }
    }
    glBindTexture(GL_TEXTURE_2D, g_texture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, g_window_width, g_window_height, GL_RGBA, GL_UNSIGNED_BYTE, tex_data.data());
    glBindTexture(GL_TEXTURE_2D, NULL);

    glutDisplayFunc(DrawScene);
    glutMainLoop(); //Start the main loop
    IntersectionApi::Delete(api);

    return 0;
}