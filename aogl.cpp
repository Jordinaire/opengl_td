#ifdef _MSC_VER
#define _USE_MATH_DEFINES
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <iostream>

#include <cmath>

#include "glew/glew.h"

#include "GLFW/glfw3.h"
#include "stb/stb_image.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw_gl3.h"

#include "GLError.h"

#include "glm/glm.hpp"
#include "glm/vec3.hpp" // glm::vec3
#include "glm/vec4.hpp" // glm::vec4, glm::ivec4
#include "glm/mat4x4.hpp" // glm::mat4
#include "glm/gtc/matrix_transform.hpp" // glm::translate, glm::rotate, glm::scale, glm::perspective
#include "glm/gtc/type_ptr.hpp" // glm::value_ptr

#ifndef DEBUG_PRINT
#define DEBUG_PRINT 1
#endif

#if DEBUG_PRINT == 0
#define debug_print(FORMAT, ...) ((void)0)
#else
#ifdef _MSC_VER
#define debug_print(FORMAT, ...) \
    fprintf(stderr, "%s() in %s, line %i: " FORMAT "\n", \
        __FUNCTION__, __FILE__, __LINE__, __VA_ARGS__)
#else
#define debug_print(FORMAT, ...) \
    fprintf(stderr, "%s() in %s, line %i: " FORMAT "\n", \
        __func__, __FILE__, __LINE__, __VA_ARGS__)
#endif
#endif

// Font buffers
extern const unsigned char DroidSans_ttf[];
extern const unsigned int DroidSans_ttf_len;

// Shader utils
int check_link_error(GLuint program);
int check_compile_error(GLuint shader, const char ** sourceBuffer);
int check_link_error(GLuint program);
GLuint compile_shader(GLenum shaderType, const char * sourceBuffer, int bufferSize);
GLuint compile_shader_from_file(GLenum shaderType, const char * fileName);

// OpenGL utils
bool checkError(const char* title);

struct Camera
{
    float radius;
    float theta;
    float phi;
    glm::vec3 o;
    glm::vec3 eye;
    glm::vec3 up;
};
void camera_defaults(Camera & c);
void camera_zoom(Camera & c, float factor);
void camera_turn(Camera & c, float phi, float theta);
void camera_pan(Camera & c, float x, float y);

struct GUIStates
{
    bool panLock;
    bool turnLock;
    bool zoomLock;
    int lockPositionX;
    int lockPositionY;
    int camera;
    double time;
    bool playing;
    static const float MOUSE_PAN_SPEED;
    static const float MOUSE_ZOOM_SPEED;
    static const float MOUSE_TURN_SPEED;
};
const float GUIStates::MOUSE_PAN_SPEED = 0.001f;
const float GUIStates::MOUSE_ZOOM_SPEED = 0.05f;
const float GUIStates::MOUSE_TURN_SPEED = 0.005f;
void init_gui_states(GUIStates & guiStates);




int main( int argc, char **argv )
{
    int width = 1280, height= 720;
    float widthf = (float) width, heightf = (float) height;
    double t;
    float fps = 0.f;

    // Initialise GLFW
    if( !glfwInit() )
    {
        fprintf( stderr, "Failed to initialize GLFW\n" );
        exit( EXIT_FAILURE );
    }
    glfwInit();
    glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
    glfwWindowHint(GLFW_VISIBLE, GL_TRUE);
    glfwWindowHint(GLFW_DECORATED, GL_TRUE);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);

#if defined(__APPLE__)
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    int const DPI = 2; // For retina screens only
#else
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_FALSE);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
    int const DPI = 1;
# endif

    // Open a window and create its OpenGL context
    GLFWwindow * window = glfwCreateWindow(width/DPI, height/DPI, "aogl", 0, 0);
    if( ! window )
    {
        fprintf( stderr, "Failed to open GLFW window\n" );
        glfwTerminate();
        exit( EXIT_FAILURE );
    }
    glfwMakeContextCurrent(window);

    // Init glew
    glewExperimental = GL_TRUE;
    GLenum err = glewInit();
    if (GLEW_OK != err)
    {
          /* Problem: glewInit failed, something is seriously wrong. */
          fprintf(stderr, "Error: %s\n", glewGetErrorString(err));
          exit( EXIT_FAILURE );
    }

    // Ensure we can capture the escape key being pressed below
    glfwSetInputMode( window, GLFW_STICKY_KEYS, GL_TRUE );

    // Enable vertical sync (on cards that support it)
    glfwSwapInterval( 1 );
    GLenum glerr = GL_NO_ERROR;
    glerr = glGetError();

    ImGui_ImplGlfwGL3_Init(window, true);

    // Init viewer structures
    Camera camera;
    camera_defaults(camera);
    GUIStates guiStates;
    init_gui_states(guiStates);
    int instanceCount = 1000;
    int pointLightCount = 20;
    int dirLightCount = 0;
    int spotLightCount = 0;
    float speed = 0.1;

    // Load images and upload textures
    GLuint textures[2];
    glGenTextures(2, textures);
    int x;
    int y;
    int comp;

    unsigned char * diffuse = stbi_load("textures/spnza_bricks_a_diff.tga", &x, &y, &comp, 3);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textures[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, x, y, 0, GL_RGB, GL_UNSIGNED_BYTE, diffuse);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    fprintf(stderr, "Diffuse %dx%d:%d\n", x, y, comp);

    glGenerateMipmap(GL_TEXTURE_2D);

    unsigned char * spec = stbi_load("textures/spnza_bricks_a_spec.tga", &x, &y, &comp, 1);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, textures[1]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, x, y, 0, GL_RED, GL_UNSIGNED_BYTE, spec);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    fprintf(stderr, "Spec %dx%d:%d\n", x, y, comp);
    checkError("Texture Initialization");

    glGenerateMipmap(GL_TEXTURE_2D);

    // Try to load and compile shaders
    GLuint vertShaderId = compile_shader_from_file(GL_VERTEX_SHADER, "aogl.vert");
    GLuint fragShaderId = compile_shader_from_file(GL_FRAGMENT_SHADER, "aogl.frag");
    GLuint programObject = glCreateProgram();
    glAttachShader(programObject, vertShaderId);
    glAttachShader(programObject, fragShaderId);
    glLinkProgram(programObject);
    if (check_link_error(programObject) < 0)
        exit(1);

    // Try to load and compile shaders
    GLuint vertBlitShaderId = compile_shader_from_file(GL_VERTEX_SHADER, "blit.vert");
    GLuint fragBlitShaderId = compile_shader_from_file(GL_FRAGMENT_SHADER, "blit.frag");
    GLuint blitProgramObject = glCreateProgram();
    glAttachShader(blitProgramObject, vertBlitShaderId);
    glAttachShader(blitProgramObject, fragBlitShaderId);
    glLinkProgram(blitProgramObject);
    if (check_link_error(blitProgramObject) < 0)
        exit(1);

    // Try to load and compile shaders
    GLuint fragPointLightShaderId = compile_shader_from_file(GL_FRAGMENT_SHADER, "pointlight.frag");
    GLuint pointLightProgramObject = glCreateProgram();
    glAttachShader(pointLightProgramObject, vertBlitShaderId);
    glAttachShader(pointLightProgramObject, fragPointLightShaderId);
    glLinkProgram(pointLightProgramObject);
    if (check_link_error(pointLightProgramObject) < 0)
        exit(1);

    // Try to load and compile shaders
    GLuint fragDirLightShaderId = compile_shader_from_file(GL_FRAGMENT_SHADER, "directionallight.frag");
    GLuint dirLightProgramObject = glCreateProgram();
    glAttachShader(dirLightProgramObject, vertBlitShaderId);
    glAttachShader(dirLightProgramObject, fragDirLightShaderId);
    glLinkProgram(dirLightProgramObject);
    if (check_link_error(dirLightProgramObject) < 0)
        exit(1);

    // Try to load and compile shaders
    GLuint fragSpotLightShaderId = compile_shader_from_file(GL_FRAGMENT_SHADER, "spotlight.frag");
    GLuint spotLightProgramObject = glCreateProgram();
    glAttachShader(spotLightProgramObject, vertBlitShaderId);
    glAttachShader(spotLightProgramObject, fragSpotLightShaderId);
    glLinkProgram(spotLightProgramObject);
    if (check_link_error(spotLightProgramObject) < 0)
        exit(1);

    // Upload uniforms
    GLuint mvpLocation = glGetUniformLocation(programObject, "MVP");
    GLuint mvLocation = glGetUniformLocation(programObject, "MV");
    GLuint timeLocation = glGetUniformLocation(programObject, "Time");
    GLuint diffuseLocation = glGetUniformLocation(programObject, "Diffuse");
    GLuint specLocation = glGetUniformLocation(programObject, "Specular");
    GLuint lightLocation = glGetUniformLocation(programObject, "Light");
    GLuint specularPowerLocation = glGetUniformLocation(programObject, "SpecularPower");
    GLuint instanceCountLocation = glGetUniformLocation(programObject, "InstanceCount");
    glProgramUniform1i(programObject, diffuseLocation, 0);
    glProgramUniform1i(programObject, specLocation, 1);


    GLuint pointLightscreenToWorldLocation = glGetUniformLocation(pointLightProgramObject, "ScreenToWorld");
    GLuint pointLightColorLocation = glGetUniformLocation(pointLightProgramObject, "ColorBuffer");
    GLuint pointLightNormalLocation = glGetUniformLocation(pointLightProgramObject, "NormalBuffer");
    GLuint pointLightDepthLocation = glGetUniformLocation(pointLightProgramObject, "DepthBuffer");

    GLuint dirLightscreenToWorldLocation = glGetUniformLocation(dirLightProgramObject, "ScreenToWorld");
    GLuint dirLightColorLocation = glGetUniformLocation(dirLightProgramObject, "ColorBuffer");
    GLuint dirLightNormalLocation = glGetUniformLocation(dirLightProgramObject, "NormalBuffer");
    GLuint dirLightDepthLocation = glGetUniformLocation(dirLightProgramObject, "DepthBuffer");

    GLuint spotLightscreenToWorldLocation = glGetUniformLocation(spotLightProgramObject, "ScreenToWorld");
    GLuint spotLightColorLocation = glGetUniformLocation(spotLightProgramObject, "ColorBuffer");
    GLuint spotLightNormalLocation = glGetUniformLocation(spotLightProgramObject, "NormalBuffer");
    GLuint spotLightDepthLocation = glGetUniformLocation(spotLightProgramObject, "DepthBuffer");


    if (!checkError("Uniforms"))
        exit(1);

    // Load geometry
    int cube_triangleCount = 12;
    int cube_triangleList[] = {0, 1, 2, 2, 1, 3, 4, 5, 6, 6, 5, 7, 8, 9, 10, 10, 9, 11, 12, 13, 14, 14, 13, 15, 16, 17, 18, 19, 17, 20, 21, 22, 23, 24, 25, 26, };
    float cube_uvs[] = {0.f, 0.f, 0.f, 1.f, 1.f, 0.f, 1.f, 1.f, 0.f, 0.f, 0.f, 1.f, 1.f, 0.f, 1.f, 1.f, 0.f, 0.f, 0.f, 1.f, 1.f, 0.f, 1.f, 1.f, 0.f, 0.f, 0.f, 1.f, 1.f, 0.f, 1.f, 1.f, 0.f, 0.f, 0.f, 1.f, 1.f, 0.f,  1.f, 0.f,  1.f, 1.f,  0.f, 1.f,  1.f, 1.f,  0.f, 0.f, 0.f, 0.f, 1.f, 1.f,  1.f, 0.f,  };
    float cube_vertices[] = {-0.5, -0.5, 0.5, 0.5, -0.5, 0.5, -0.5, 0.5, 0.5, 0.5, 0.5, 0.5, -0.5, 0.5, 0.5, 0.5, 0.5, 0.5, -0.5, 0.5, -0.5, 0.5, 0.5, -0.5, -0.5, 0.5, -0.5, 0.5, 0.5, -0.5, -0.5, -0.5, -0.5, 0.5, -0.5, -0.5, -0.5, -0.5, -0.5, 0.5, -0.5, -0.5, -0.5, -0.5, 0.5, 0.5, -0.5, 0.5, 0.5, -0.5, 0.5, 0.5, -0.5, -0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, -0.5, -0.5, -0.5, -0.5, -0.5, -0.5, 0.5, -0.5, 0.5, -0.5, -0.5, 0.5, -0.5, -0.5, -0.5, 0.5, -0.5, 0.5, 0.5 };
    float cube_normals[] = {0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 0, -1, 0, 0, -1, 0, 0, -1, 0, 0, -1, 0, -1, 0, 0, -1, 0, 0, -1, 0, 0, -1, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, -1, 0, 0, -1, 0, 0, -1, 0, 0, -1, 0, 0, -1, 0, 0, -1, 0, 0, };
    int plane_triangleCount = 2;
    int plane_triangleList[] = {0, 1, 2, 2, 1, 3};
    float plane_uvs[] = {0.f, 0.f, 0.f, 50.f, 50.f, 0.f, 50.f, 50.f};
    float plane_vertices[] = {-50.0, -1.0, 50.0, 50.0, -1.0, 50.0, -50.0, -1.0, -50.0, 50.0, -1.0, -50.0};
    float plane_normals[] = {0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0};

    // Load blit screens geometry
    int quad_triangleCount = 2;
    int quad_triangleList[] = {0, 1, 2, 2, 1, 3};
    float quad_vertices[] =  {-1.0, -1.0, 1.0, -1.0, -1.0, 1.0, 1.0, 1.0};

    // Define Lights struct
    struct PointLight
    {
        glm::vec3 position;
        int padding;
        glm::vec3 color;
        float intensity;
        float boundingRadius;
    };

    struct DirectionalLight
    {
        glm::vec3 direction;
        int padding;
        glm::vec3 color;
        float intensity;
    };

    struct SpotLight
    {
        glm::vec3 position;
        float angle;
        glm::vec3 direction;
        float penumbraAngle;
        glm::vec3 color;
        float intensity;
    };


    // Framebuffer object handle
    GLuint gbufferFbo;
    // Texture handles
    GLuint gbufferTextures[3];
    glGenTextures(3, gbufferTextures);
    // 2 draw buffers for color and normal
    GLuint gbufferDrawBuffers[2];

    // Create color texture
    glBindTexture(GL_TEXTURE_2D, gbufferTextures[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Create normal texture
    glBindTexture(GL_TEXTURE_2D, gbufferTextures[1]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Create depth texture
    glBindTexture(GL_TEXTURE_2D, gbufferTextures[2]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Create Framebuffer Object
    glGenFramebuffers(1, &gbufferFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, gbufferFbo);
    // Initialize DrawBuffers
    gbufferDrawBuffers[0] = GL_COLOR_ATTACHMENT0;
    gbufferDrawBuffers[1] = GL_COLOR_ATTACHMENT1;
    glDrawBuffers(2, gbufferDrawBuffers);

    // Attach textures to framebuffer
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 , GL_TEXTURE_2D, gbufferTextures[0], 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1 , GL_TEXTURE_2D, gbufferTextures[1], 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, gbufferTextures[2], 0);

    if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        fprintf(stderr, "Error on building framebuffer\n");
        exit( EXIT_FAILURE );
    }

    // Back to the default framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Vertex Array Object
    GLuint vao[3];
    glGenVertexArrays(3, vao);

    // Vertex Buffer Objects
    GLuint vbo[10];
    glGenBuffers(10, vbo);

    // Cube
    glBindVertexArray(vao[0]);
    // Bind indices and upload data
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo[0]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cube_triangleList), cube_triangleList, GL_STATIC_DRAW);
    // Bind vertices and upload data
    glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GL_FLOAT)*3, (void*)0);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cube_vertices), cube_vertices, GL_STATIC_DRAW);
    // Bind normals and upload data
    glBindBuffer(GL_ARRAY_BUFFER, vbo[2]);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(GL_FLOAT)*3, (void*)0);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cube_normals), cube_normals, GL_STATIC_DRAW);
    // Bind uv coords and upload data
    glBindBuffer(GL_ARRAY_BUFFER, vbo[3]);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(GL_FLOAT)*2, (void*)0);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cube_uvs), cube_uvs, GL_STATIC_DRAW);

    // Plane
    glBindVertexArray(vao[1]);
    // Bind indices and upload data
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo[4]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(plane_triangleList), plane_triangleList, GL_STATIC_DRAW);
    // Bind vertices and upload data
    glBindBuffer(GL_ARRAY_BUFFER, vbo[5]);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GL_FLOAT)*3, (void*)0);
    glBufferData(GL_ARRAY_BUFFER, sizeof(plane_vertices), plane_vertices, GL_STATIC_DRAW);
    // Bind normals and upload data
    glBindBuffer(GL_ARRAY_BUFFER, vbo[6]);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(GL_FLOAT)*3, (void*)0);
    glBufferData(GL_ARRAY_BUFFER, sizeof(plane_normals), plane_normals, GL_STATIC_DRAW);
    // Bind uv coords and upload data
    glBindBuffer(GL_ARRAY_BUFFER, vbo[7]);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(GL_FLOAT)*2, (void*)0);
    glBufferData(GL_ARRAY_BUFFER, sizeof(plane_uvs), plane_uvs, GL_STATIC_DRAW);

    // Quad
    glBindVertexArray(vao[2]);
    // Bind indices and upload data
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo[8]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(quad_triangleList), quad_triangleList, GL_STATIC_DRAW);
    // Bind vertices and upload data
    glBindBuffer(GL_ARRAY_BUFFER, vbo[9]);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(GL_FLOAT)*2, (void*)0);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), quad_vertices, GL_STATIC_DRAW);

    // Unbind everything. Potentially illegal on some implementations
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    // Generate Shader Storage Objects
    GLuint ssbo[3];
    glGenBuffers(3, ssbo);

    checkError("Buffer Init");

    do
    {
        t = glfwGetTime() * speed / 10.f;

        // Mouse states
        int leftButton = glfwGetMouseButton( window, GLFW_MOUSE_BUTTON_LEFT );
        int rightButton = glfwGetMouseButton( window, GLFW_MOUSE_BUTTON_RIGHT );
        int middleButton = glfwGetMouseButton( window, GLFW_MOUSE_BUTTON_MIDDLE );

        if( leftButton == GLFW_PRESS )
            guiStates.turnLock = true;
        else
            guiStates.turnLock = false;

        if( rightButton == GLFW_PRESS )
            guiStates.zoomLock = true;
        else
            guiStates.zoomLock = false;

        if( middleButton == GLFW_PRESS )
            guiStates.panLock = true;
        else
            guiStates.panLock = false;

        // Camera movements
        int altPressed = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT);
        if (!altPressed && (leftButton == GLFW_PRESS || rightButton == GLFW_PRESS || middleButton == GLFW_PRESS))
        {
            double x; double y;
            glfwGetCursorPos(window, &x, &y);
            guiStates.lockPositionX = x;
            guiStates.lockPositionY = y;
        }
        if (altPressed == GLFW_PRESS)
        {
            double mousex; double mousey;
            glfwGetCursorPos(window, &mousex, &mousey);
            int diffLockPositionX = mousex - guiStates.lockPositionX;
            int diffLockPositionY = mousey - guiStates.lockPositionY;
            if (guiStates.zoomLock)
            {
                float zoomDir = 0.0;
                if (diffLockPositionX > 0)
                    zoomDir = -1.f;
                else if (diffLockPositionX < 0 )
                    zoomDir = 1.f;
                camera_zoom(camera, zoomDir * GUIStates::MOUSE_ZOOM_SPEED);
            }
            else if (guiStates.turnLock)
            {
                camera_turn(camera, diffLockPositionY * GUIStates::MOUSE_TURN_SPEED,
                            diffLockPositionX * GUIStates::MOUSE_TURN_SPEED);

            }
            else if (guiStates.panLock)
            {
                camera_pan(camera, diffLockPositionX * GUIStates::MOUSE_PAN_SPEED,
                            diffLockPositionY * GUIStates::MOUSE_PAN_SPEED);
            }
            guiStates.lockPositionX = mousex;
            guiStates.lockPositionY = mousey;
        }

        // Default states
        glEnable(GL_DEPTH_TEST);

        // Clear the front buffer
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Viewport
        glViewport( 0, 0, width, height  );

        // Bind gbuffer
        glBindFramebuffer(GL_FRAMEBUFFER, gbufferFbo);

        // Clear the gbuffer
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Get camera matrices
        glm::mat4 projection = glm::perspective(45.0f, widthf / heightf, 0.1f, 100.f);
        glm::mat4 worldToView = glm::lookAt(camera.eye, camera.o, camera.up);
        glm::mat4 objectToWorld;
        glm::mat4 mv = worldToView * objectToWorld;
        glm::mat4 mvp = projection * mv;

        // Compute the inverse worldToScreen matrix
        glm::mat4 screenToWorld = glm::inverse(projection);

        // Select textures
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textures[0]);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, textures[1]);

        // Upload uniforms
        glProgramUniformMatrix4fv(programObject, mvpLocation, 1, 0, glm::value_ptr(mvp));
        glProgramUniformMatrix4fv(programObject, mvLocation, 1, 0, glm::value_ptr(mv));
        glProgramUniform1i(programObject, instanceCountLocation, (int) instanceCount);
        glProgramUniform1f(programObject, specularPowerLocation, 30.f);
        glProgramUniform1f(programObject, timeLocation, t);
        glProgramUniform1i(programObject, diffuseLocation, 0);
        glProgramUniform1i(programObject, specLocation, 1);

        glProgramUniformMatrix4fv(pointLightProgramObject, pointLightscreenToWorldLocation, 1, 0, glm::value_ptr(screenToWorld));
        glProgramUniform1i(pointLightProgramObject, pointLightColorLocation, 0);
        glProgramUniform1i(pointLightProgramObject, pointLightNormalLocation, 1);
        glProgramUniform1i(pointLightProgramObject, pointLightDepthLocation, 2);

        glProgramUniformMatrix4fv(dirLightProgramObject, dirLightscreenToWorldLocation, 1, 0, glm::value_ptr(screenToWorld));
        glProgramUniform1i(dirLightProgramObject, dirLightColorLocation, 0);
        glProgramUniform1i(dirLightProgramObject, dirLightNormalLocation, 1);
        glProgramUniform1i(dirLightProgramObject, dirLightDepthLocation, 2);

        glProgramUniformMatrix4fv(spotLightProgramObject, spotLightscreenToWorldLocation, 1, 0, glm::value_ptr(screenToWorld));
        glProgramUniform1i(spotLightProgramObject, spotLightColorLocation, 0);
        glProgramUniform1i(spotLightProgramObject, spotLightNormalLocation, 1);
        glProgramUniform1i(spotLightProgramObject, spotLightDepthLocation, 2);


        int pointLightBufferSize = sizeof(PointLight) * pointLightCount + sizeof(int) * 4;
        int dirLightBufferSize = sizeof(DirectionalLight) * dirLightCount + sizeof(int) * 4;
        int spotLightBufferSize = sizeof(SpotLight) * spotLightCount + sizeof(int) * 4;

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo[0]);
        glBufferData(GL_SHADER_STORAGE_BUFFER, pointLightBufferSize, 0, GL_DYNAMIC_COPY);
        void * pointLightBuffer = glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_WRITE_ONLY);
        ((int*) pointLightBuffer)[0] = pointLightCount;
        for (int i = 0; i < pointLightCount; ++i) {
            PointLight p = { glm::vec3( worldToView * glm::vec4((pointLightCount) * sinf(t*i) / 30.f, 2.0, fabsf(pointLightCount) * cosf(t*i) / 30.f, 1.0)) , 0,
                             glm::vec3(fabsf(cos(t+i*2.f)), 1.-fabsf(sinf(t+i)) , 0.5f + 0.5f-fabsf(cosf(t+i)) ),
                             5.f, 20.f};
            ((PointLight*) ((int*) pointLightBuffer + 4))[i] = p;
        }
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo[1]);
        glBufferData(GL_SHADER_STORAGE_BUFFER, dirLightBufferSize, 0, GL_DYNAMIC_COPY);
        void * dirLightBuffer = glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_WRITE_ONLY);
        ((int*) dirLightBuffer)[0] = dirLightCount;
        for (int i = 0; i < dirLightCount; ++i) {
            DirectionalLight directionalLight = { glm::vec3( worldToView * glm::vec4(sinf(t*10.0+i), -1.0, 0.0, 0.0)), 0,
                                                  glm::vec3(1.0, 1.0, 1.0),
                                                  0.01f};
            ((DirectionalLight*) ((int*) dirLightBuffer + 4))[i] = directionalLight;
        }
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo[2]);
        glBufferData(GL_SHADER_STORAGE_BUFFER, spotLightBufferSize, 0, GL_DYNAMIC_COPY);
        void * spotLightBuffer = glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_WRITE_ONLY);
        ((int*) spotLightBuffer)[0] = spotLightCount;
        for (int i = 0; i < spotLightCount; ++i)  {
            SpotLight spotLight = { glm::vec3( worldToView * glm::vec4((spotLightCount*sinf(t))  * cosf(t*i) / 20.f, 1.f + sinf(t * i), fabsf(spotLightCount*cosf(t)) * sinf(t*i), 1.0)), 45.f + 20.f * cos(t + i) / 20.f,
                                    glm::vec3( worldToView * glm::vec4(sinf(t*10.0+i), -1.0, 0.0, 0.0)), 60.f + 20.f * cos(t + i),
                                    glm::vec3(fabsf(cos(t+i*2.f)), 1.-fabsf(sinf(t+i)) , 0.5f + 0.5f-fabsf(cosf(t+i))),  1.0};
            ((SpotLight*) ((int*) spotLightBuffer + 4))[i] = spotLight;
        }
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);

        // Select shader
        glUseProgram(programObject);

        // Render vaos
        glBindVertexArray(vao[0]);
        glDrawElementsInstanced(GL_TRIANGLES, cube_triangleCount * 3, GL_UNSIGNED_INT, (void*)0, (int) instanceCount);
        glProgramUniform1f(programObject, timeLocation, 0.f);
        // glBindVertexArray(vao[1]);
        // glDrawElements(GL_TRIANGLES, plane_triangleCount * 3, GL_UNSIGNED_INT, (void*)0);

        // Unbind the frambuffer
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);

        // Select textures
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, gbufferTextures[0]);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, gbufferTextures[1]);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, gbufferTextures[2]);

        // Bind quad vao
        glBindVertexArray(vao[2]);

        // Use deferred point light shader
        glUseProgram(pointLightProgramObject);
        glBindBufferRange(GL_SHADER_STORAGE_BUFFER, 0, ssbo[0], 0, pointLightBufferSize);
        glDrawElements(GL_TRIANGLES, quad_triangleCount * 3, GL_UNSIGNED_INT, (void*)0);

        // Use deferred dir light shader
        glUseProgram(dirLightProgramObject);
        glBindBufferRange(GL_SHADER_STORAGE_BUFFER, 1, ssbo[1], 0, dirLightBufferSize);
        glDrawElements(GL_TRIANGLES, quad_triangleCount * 3, GL_UNSIGNED_INT, (void*)0);

        // Use deferred dir light shader
        glUseProgram(spotLightProgramObject);
        glBindBufferRange(GL_SHADER_STORAGE_BUFFER, 2, ssbo[2], 0, spotLightBufferSize);
        glDrawElements(GL_TRIANGLES, quad_triangleCount * 3, GL_UNSIGNED_INT, (void*)0);

        glDisable(GL_BLEND);

        // Use the blit program
        glUseProgram(blitProgramObject);

        int gBufferSize = sizeof(gbufferTextures) / sizeof(gbufferTextures[0]);

        glActiveTexture(GL_TEXTURE0);

        for(int i = 0; i < gBufferSize; ++i)
        {
            glViewport( i * width/gBufferSize, 0, width/gBufferSize, height/4 );
            glBindTexture(GL_TEXTURE_2D, gbufferTextures[i]);
            glDrawElements(GL_TRIANGLES, quad_triangleCount * 3, GL_UNSIGNED_INT, (void*)0);
        }

        // check OpenGL error
        GLenum err;
        while ((err = glGetError()) != GL_NO_ERROR) {
            std::cerr << "OpenGL error: " << err << std::endl;
        }


        // Draw UI
        ImGui_ImplGlfwGL3_NewFrame();
        ImGui::SetNextWindowSize(ImVec2(200,100), ImGuiSetCond_FirstUseEver);
        ImGui::Begin("aogl");
        ImGui::SliderFloat("Speed", &speed, 0.01, 1.0);
        ImGui::SliderInt("Point Lights", &pointLightCount, 0, 1024);
        ImGui::SliderInt("Directional Lights", &dirLightCount, 0, 1024);
        ImGui::SliderInt("Spot Lights", &spotLightCount, 0, 1024);
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
        ImGui::End();

        ImGui::Render();
        // Check for errors
        checkError("End loop");

        glfwSwapBuffers(window);
        glfwPollEvents();

        double newTime = glfwGetTime();
        fps = 1.f/ (newTime - t);
    } // Check if the ESC key was pressed
    while( glfwGetKey( window, GLFW_KEY_ESCAPE ) != GLFW_PRESS );

    // Close OpenGL window and terminate GLFW
    ImGui_ImplGlfwGL3_Shutdown();
    glfwTerminate();

    exit( EXIT_SUCCESS );
}

// No windows implementation of strsep
char * strsep_custom(char **stringp, const char *delim)
{
    register char *s;
    register const char *spanp;
    register int c, sc;
    char *tok;
    if ((s = *stringp) == NULL)
        return (NULL);
    for (tok = s; ; ) {
        c = *s++;
        spanp = delim;
        do {
            if ((sc = *spanp++) == c) {
                if (c == 0)
                    s = NULL;
                else
                    s[-1] = 0;
                *stringp = s;
                return (tok);
            }
        } while (sc != 0);
    }
    return 0;
}

int check_compile_error(GLuint shader, const char ** sourceBuffer)
{
    // Get error log size and print it eventually
    int logLength;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
    if (logLength > 1)
    {
        char * log = new char[logLength];
        glGetShaderInfoLog(shader, logLength, &logLength, log);
        char *token, *string;
        string = strdup(sourceBuffer[0]);
        int lc = 0;
        while ((token = strsep_custom(&string, "\n")) != NULL) {
           printf("%3d : %s\n", lc, token);
           ++lc;
        }
        fprintf(stderr, "Compile : %s", log);
        delete[] log;
    }
    // If an error happend quit
    int status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status == GL_FALSE)
        return -1;
    return 0;
}

int check_link_error(GLuint program)
{
    // Get link error log size and print it eventually
    int logLength;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
    if (logLength > 1)
    {
        char * log = new char[logLength];
        glGetProgramInfoLog(program, logLength, &logLength, log);
        fprintf(stderr, "Link : %s \n", log);
        delete[] log;
    }
    int status;
    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if (status == GL_FALSE)
        return -1;
    return 0;
}

GLuint compile_shader(GLenum shaderType, const char * sourceBuffer, int bufferSize)
{
    GLuint shaderObject = glCreateShader(shaderType);
    const char * sc[1] = { sourceBuffer };
    glShaderSource(shaderObject,
                   1,
                   sc,
                   NULL);
    glCompileShader(shaderObject);
    check_compile_error(shaderObject, sc);
    return shaderObject;
}

GLuint compile_shader_from_file(GLenum shaderType, const char * path)
{
    FILE * shaderFileDesc = fopen( path, "rb" );
    if (!shaderFileDesc)
        return 0;
    fseek ( shaderFileDesc , 0 , SEEK_END );
    long fileSize = ftell ( shaderFileDesc );
    rewind ( shaderFileDesc );
    char * buffer = new char[fileSize + 1];
    fread( buffer, 1, fileSize, shaderFileDesc );
    buffer[fileSize] = '\0';
    GLuint shaderObject = compile_shader(shaderType, buffer, fileSize );
    delete[] buffer;
    return shaderObject;
}


bool checkError(const char* title)
{
    int error;
    if((error = glGetError()) != GL_NO_ERROR)
    {
        std::string errorString;
        switch(error)
        {
        case GL_INVALID_ENUM:
            errorString = "GL_INVALID_ENUM";
            break;
        case GL_INVALID_VALUE:
            errorString = "GL_INVALID_VALUE";
            break;
        case GL_INVALID_OPERATION:
            errorString = "GL_INVALID_OPERATION";
            break;
        case GL_INVALID_FRAMEBUFFER_OPERATION:
            errorString = "GL_INVALID_FRAMEBUFFER_OPERATION";
            break;
        case GL_OUT_OF_MEMORY:
            errorString = "GL_OUT_OF_MEMORY";
            break;
        default:
            errorString = "UNKNOWN";
            break;
        }
        fprintf(stdout, "OpenGL Error(%s): %s\n", errorString.c_str(), title);
    }
    return error == GL_NO_ERROR;
}

void camera_compute(Camera & c)
{
    c.eye.x = cos(c.theta) * sin(c.phi) * c.radius + c.o.x;
    c.eye.y = cos(c.phi) * c.radius + c.o.y ;
    c.eye.z = sin(c.theta) * sin(c.phi) * c.radius + c.o.z;
    c.up = glm::vec3(0.f, c.phi < M_PI ?1.f:-1.f, 0.f);
}

void camera_defaults(Camera & c)
{
    c.phi = 3.14/2.f;
    c.theta = 3.14/2.f;
    c.radius = 10.f;
    camera_compute(c);
}

void camera_zoom(Camera & c, float factor)
{
    c.radius += factor * c.radius ;
    if (c.radius < 0.1)
    {
        c.radius = 10.f;
        c.o = c.eye + glm::normalize(c.o - c.eye) * c.radius;
    }
    camera_compute(c);
}

void camera_turn(Camera & c, float phi, float theta)
{
    c.theta += 1.f * theta;
    c.phi   -= 1.f * phi;
    if (c.phi >= (2 * M_PI) - 0.1 )
        c.phi = 0.00001;
    else if (c.phi <= 0 )
        c.phi = 2 * M_PI - 0.1;
    camera_compute(c);
}

void camera_pan(Camera & c, float x, float y)
{
    glm::vec3 up(0.f, c.phi < M_PI ?1.f:-1.f, 0.f);
    glm::vec3 fwd = glm::normalize(c.o - c.eye);
    glm::vec3 side = glm::normalize(glm::cross(fwd, up));
    c.up = glm::normalize(glm::cross(side, fwd));
    c.o[0] += up[0] * y * c.radius * 2;
    c.o[1] += up[1] * y * c.radius * 2;
    c.o[2] += up[2] * y * c.radius * 2;
    c.o[0] -= side[0] * x * c.radius * 2;
    c.o[1] -= side[1] * x * c.radius * 2;
    c.o[2] -= side[2] * x * c.radius * 2;
    camera_compute(c);
}

void init_gui_states(GUIStates & guiStates)
{
    guiStates.panLock = false;
    guiStates.turnLock = false;
    guiStates.zoomLock = false;
    guiStates.lockPositionX = 0;
    guiStates.lockPositionY = 0;
    guiStates.camera = 0;
    guiStates.time = 0.0;
    guiStates.playing = false;
}
