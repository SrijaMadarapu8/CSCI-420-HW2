/*
  CSCI 420 Computer Graphics, Computer Science, USC
  Assignment 1: Height Fields with Shaders.
  C/C++ starter code

  Student username: <type your USC username here>
*/

#include "openGLHeader.h"
#include "glutHeader.h"
#include "openGLMatrix.h"
#include "imageIO.h"
#include "pipelineProgram.h"
#include "vbo.h"
#include "vao.h"

#include <iostream>
#include <cstring>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>

#if defined(WIN32) || defined(_WIN32)
#ifdef _DEBUG
#pragma comment(lib, "glew32d.lib")
#else
#pragma comment(lib, "glew32.lib")
#endif
#endif

#if defined(WIN32) || defined(_WIN32)
char shaderBasePath[1024] = SHADER_BASE_PATH;
#else
char shaderBasePath[1024] = "../openGLHelper";
#endif

using namespace std;

int mousePos[2]; // x,y screen coordinates of the current mouse position

int leftMouseButton = 0; // 1 if pressed, 0 if not 
int middleMouseButton = 0; // 1 if pressed, 0 if not
int rightMouseButton = 0; // 1 if pressed, 0 if not

typedef enum { ROTATE, TRANSLATE, SCALE } CONTROL_STATE;
CONTROL_STATE controlState = ROTATE;

typedef enum { S,T,D,E,H } DISPLAY_STATE;
DISPLAY_STATE displayState = S;

// Transformations of the terrain.
float terrainRotate[3] = { 0.0f, 0.0f, 0.0f };
// terrainRotate[0] gives the rotation around x-axis (in degrees)
// terrainRotate[1] gives the rotation around y-axis (in degrees)
// terrainRotate[2] gives the rotation around z-axis (in degrees)
float terrainTranslate[3] = { 0.0f, 0.0f, 0.0f };
float terrainScale[3] = { 1.0f, 1.0f, 1.0f };

// Width and height of the OpenGL window, in pixels.
int windowWidth = 1280;
int windowHeight = 720;
char windowTitle[512] = "CSCI 420 Homework 1";

// Stores the image loaded from disk.
ImageIO* heightmapImage;
GLuint texHandle;

// Number of vertices in the single triangle (starter code).
int numVertices;

// CSCI 420 helper classes.
OpenGLMatrix matrix;
PipelineProgram* pipelineProgram = nullptr, * groundpipelineProgram = nullptr, * skyboxpipelineProgram = nullptr, * objpipelineProgram = nullptr;
VBO* vboVertices_right = nullptr, * vboVertices_left = nullptr, *vboVertices_center = nullptr,*vboVertices_obj=nullptr;
VBO* vboColors_right = nullptr, * vboColors_left = nullptr,* vboColors_center = nullptr,*vboColors_obj=nullptr;
VAO* vao_right = nullptr,* vao_left = nullptr,*vao_center=nullptr, * groundvao = nullptr, * skyboxvao = nullptr, * vao_obj = nullptr;
float* positions_center = nullptr, * tangents_center = nullptr, * normals_center = nullptr, * binormals_center = nullptr, * colors_center = nullptr;
float* positions_right = nullptr, * tangents_right = nullptr, * normals_right = nullptr, * binormals_right = nullptr, * colors_right = nullptr;
float* positions_left = nullptr, * tangents_left = nullptr, * normals_left = nullptr, * binormals_left = nullptr, * colors_left = nullptr;

float* vertices_left = nullptr, * vertices_right = nullptr, * vertices_center = nullptr;


// number of points in spline sequence
int u_start = 0.000f;
int u_end = 1.000f;
float u_interval = 0.001f;
int u_NumPoints = 1001;
double s = 0.5;
int cam = 0;
int move_cam = 0;
unsigned int cubemapTexture;
float offsetAmount = 0.09f;

int animation = 0, animation_c = 0;

int numVertice_obj = 0;

// Represents one spline control point.
struct Point
{
    double x, y, z;
};

// Contains the control points of the spline.
struct Spline
{
    int numControlPoints;
    Point* points;
} spline;

void loadSpline(char* argv)
{
    FILE* fileSpline = fopen(argv, "r");
    if (fileSpline == NULL)
    {
        printf("Cannot open file %s.\n", argv);
        exit(1);
    }

    // Read the number of spline control points.
    fscanf(fileSpline, "%d\n", &spline.numControlPoints);
    printf("Detected %d control points.\n", spline.numControlPoints);

    // Allocate memory.
    spline.points = (Point*)malloc(spline.numControlPoints * sizeof(Point));
    // Load the control points.
    for (int i = 0; i < spline.numControlPoints; i++)
    {
        if (fscanf(fileSpline, "%lf %lf %lf",
            &spline.points[i].x,
            &spline.points[i].y,
            &spline.points[i].z) != 3)
        {
            printf("Error: incorrect number of control points in file %s.\n", argv);
            exit(1);
        }
    }
}

// Multiply C = A * B, where A is a m x p matrix, and B is a p x n matrix.
// All matrices A, B, C must be pre-allocated (say, using malloc or similar).
// The memory storage for C must *not* overlap in memory with either A or B. 
// That is, you **cannot** do C = A * C, or C = C * B. However, A and B can overlap, and so C = A * A is fine, as long as the memory buffer for A is not overlaping in memory with that of C.
// Very important: All matrices are stored in **column-major** format.
// Example. Suppose 
//      [ 1 8 2 ]
//  A = [ 3 5 7 ]
//      [ 0 2 4 ]
//  Then, the storage in memory is
//   1, 3, 0, 8, 5, 2, 2, 7, 4. 
void MultiplyMatrices(int m, int p, int n, const double* A, const double* B, double* C)
{
    for (int i = 0; i < m; i++)
    {
        for (int j = 0; j < n; j++)
        {
            double entry = 0.0;
            for (int k = 0; k < p; k++)
                entry += A[k * m + i] * B[j * p + k];
            C[m * j + i] = entry;
        }
    }
}

int initTexture(const char* imageFilename, GLuint textureHandle)
{
    // Read the texture image.
    ImageIO img;
    ImageIO::fileFormatType imgFormat;
    ImageIO::errorType err = img.load(imageFilename, &imgFormat);

    if (err != ImageIO::OK)
    {
        printf("Loading texture from %s failed.\n", imageFilename);
        return -1;
    }

    // Check that the number of bytes is a multiple of 4.
    if (img.getWidth() * img.getBytesPerPixel() % 4)
    {
        printf("Error (%s): The width*numChannels in the loaded image must be a multiple of 4.\n", imageFilename);
        return -1;
    }

    // Allocate space for an array of pixels.
    int width = img.getWidth();
    int height = img.getHeight();
    unsigned char* pixelsRGBA = new unsigned char[4 * width * height]; // we will use 4 bytes per pixel, i.e., RGBA
    printf(" %d  %d ", width, height);
    // Fill the pixelsRGBA array with the image pixels.
    memset(pixelsRGBA, 0, 4 * width * height); // set all bytes to 0
    for (int h = 0; h < height; h++)
    {
        for (int w = 0; w < width; w++)
        {
            // assign some default byte values (for the case where img.getBytesPerPixel() < 4)
            pixelsRGBA[4 * (h * width + w) + 0] = 0; // red
            pixelsRGBA[4 * (h * width + w) + 1] = 0; // green
            pixelsRGBA[4 * (h * width + w) + 2] = 0; // blue
            pixelsRGBA[4 * (h * width + w) + 3] = 255; // alpha channel; fully opaque

            // set the RGBA channels, based on the loaded image
            int numChannels = img.getBytesPerPixel();

            for (int c = 0; c < numChannels; c++) // only set as many channels as are available in the loaded image; the rest get the default value
            {
                pixelsRGBA[4 * (h * width + w) + c] = img.getPixel(w, h, c);

            }
        }
    }
    // Bind the texture.
    glBindTexture(GL_TEXTURE_2D, textureHandle);

    // Initialize the texture.
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixelsRGBA);

    // Generate the mipmaps for this texture.
    glGenerateMipmap(GL_TEXTURE_2D);

    // Set the texture parameters.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    // Query support for anisotropic texture filtering.
    GLfloat fLargest;
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &fLargest);
    printf("Max available anisotropic samples: %f\n", fLargest);
    // Set anisotropic texture filtering.
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 0.5f * fLargest);

    // Query for any errors.
    GLenum errCode = glGetError();
    if (errCode != 0)
    {
        printf("Texture initialization error. Error code: %d.\n", errCode);
        return -1;
    }

    // De-allocate the pixel array -- it is no longer needed.
    delete[] pixelsRGBA;

    return 0;
}

// Write a screenshot to the specified filename.
void saveScreenshot(const char* filename)
{
    unsigned char* screenshotData = new unsigned char[windowWidth * windowHeight * 3];
    glReadPixels(0, 0, windowWidth, windowHeight, GL_RGB, GL_UNSIGNED_BYTE, screenshotData);

    ImageIO screenshotImg(windowWidth, windowHeight, 3, screenshotData);

    if (screenshotImg.save(filename, ImageIO::FORMAT_JPEG) == ImageIO::OK)
        cout << "File " << filename << " saved successfully." << endl;
    else cout << "Failed to save file " << filename << '.' << endl;

    delete[] screenshotData;
}

void idleFunc()
{
    if (animation == 1) {
        if (cam % 20 == 0)
        {
            if (animation_c < 100)
            {
                move_cam = 1;

                string str = "animation/" + to_string(animation_c) + ".jpg";
                saveScreenshot(str.c_str());
                animation_c++;
            }
            else
            {
                if (animation_c >= 100 && animation_c < 200)
                {
                    displayState = D;


                    string str = "animation/" + to_string(animation_c) + ".jpg";
                    saveScreenshot(str.c_str());
                    animation_c++;
                }
                else
                {
                    if (animation_c >= 200 && animation_c < 300)
                    {
                        displayState = E;


                        string str = "animation/" + to_string(animation_c) + ".jpg";
                        saveScreenshot(str.c_str());
                        animation_c++;
                    }
                    else {
                        if (animation_c >= 300 && animation_c < 350)
                        {


                            string str = "animation/" + to_string(animation_c) + ".jpg";
                            saveScreenshot(str.c_str());
                            animation_c++;
                        }
                        else
                        {

                            if (animation_c >= 350 && animation_c < 450)
                            {
                                displayState = T;


                                string str = "animation/" + to_string(animation_c) + ".jpg";
                                saveScreenshot(str.c_str());
                                animation_c++;
                            }
                            else
                            {
                                if (animation_c >= 450 && animation_c < 880)
                                {
                                    displayState = H;


                                    string str = "animation/" + to_string(animation_c) + ".jpg";
                                    saveScreenshot(str.c_str());
                                    animation_c++;
                                }

                            }
                        }
                    }

                }
            }

        }
    }
    glutPostRedisplay();
}

void reshapeFunc(int w, int h)
{
    glViewport(0, 0, w, h);

    // When the window has been resized, we need to re-set our projection matrix.
    matrix.SetMatrixMode(OpenGLMatrix::Projection);
    matrix.LoadIdentity();
    // You need to be careful about setting the zNear and zFar. 
    // Anything closer than zNear, or further than zFar, will be culled.
    const float zNear = 0.1f;
    const float zFar = 10000.0f;
    const float humanFieldOfView = 60.0f;
    matrix.Perspective(humanFieldOfView, 1.0f * w / h, zNear, zFar);
}

void mouseMotionDragFunc(int x, int y)
{
    // Mouse has moved, and one of the mouse buttons is pressed (dragging).

    // the change in mouse position since the last invocation of this function
    int mousePosDelta[2] = { x - mousePos[0], y - mousePos[1] };

    switch (controlState)
    {
        // translate the terrain
    case TRANSLATE:
        if (leftMouseButton)
        {
            // control x,y translation via the left mouse button
            terrainTranslate[0] += mousePosDelta[0] * 0.01f;
            terrainTranslate[1] -= mousePosDelta[1] * 0.01f;
        }
        if (middleMouseButton)
        {
            // control z translation via the middle mouse button
            terrainTranslate[2] += mousePosDelta[1] * 0.01f;
        }
        break;

        // rotate the terrain
    case ROTATE:
        if (leftMouseButton)
        {
            // control x,y rotation via the left mouse button
            terrainRotate[0] += mousePosDelta[1];
            terrainRotate[1] += mousePosDelta[0];
        }
        if (middleMouseButton)
        {
            // control z rotation via the middle mouse button
            terrainRotate[2] += mousePosDelta[1];
        }
        break;

        // scale the terrain
    case SCALE:
        if (leftMouseButton)
        {
            // control x,y scaling via the left mouse button
            terrainScale[0] *= 1.0f + mousePosDelta[0] * 0.01f;
            terrainScale[1] *= 1.0f - mousePosDelta[1] * 0.01f;
        }
        if (middleMouseButton)
        {
            // control z scaling via the middle mouse button
            terrainScale[2] *= 1.0f - mousePosDelta[1] * 0.01f;
        }
        break;
    }

    // store the new mouse position
    mousePos[0] = x;
    mousePos[1] = y;
}

void mouseMotionFunc(int x, int y)
{
    // Mouse has moved.
    // Store the new mouse position.
    mousePos[0] = x;
    mousePos[1] = y;
}

void mouseButtonFunc(int button, int state, int x, int y)
{
    // A mouse button has has been pressed or depressed.

    // Keep track of the mouse button state, in leftMouseButton, middleMouseButton, rightMouseButton variables.
    switch (button)
    {
    case GLUT_LEFT_BUTTON:
        leftMouseButton = (state == GLUT_DOWN);
        break;

    case GLUT_MIDDLE_BUTTON:
        middleMouseButton = (state == GLUT_DOWN);
        break;

    case GLUT_RIGHT_BUTTON:
        rightMouseButton = (state == GLUT_DOWN);
        break;
    }

    // Keep track of whether CTRL and SHIFT keys are pressed.
    switch (glutGetModifiers())
    {
    case GLUT_ACTIVE_CTRL:
        controlState = TRANSLATE;
        break;

    case GLUT_ACTIVE_SHIFT:
        controlState = SCALE;
        break;

        // If CTRL and SHIFT are not pressed, we are in rotate mode.
    default:
        controlState = ROTATE;
        break;
    }

    // Store the new mouse position.
    mousePos[0] = x;
    mousePos[1] = y;
}

void keyboardFunc(unsigned char key, int x, int y)
{
    switch (key)
    {
    case 27: // ESC key
        exit(0); // exit the program
        break;

    case ' ':
        cout << "You pressed the spacebar." << endl;
        break;

    case 'x':
        // Take a screenshot.
        saveScreenshot("screenshot.jpg");
        break;
    case 'm':
        move_cam = 1;
        break;
    case 's':
        move_cam = 0;
        break;
    case'a':
        animation = 1;
        break;
    case'1':
        displayState = S;
        break;
    case'2':
        displayState = D;
        break;
    case't':
        displayState = T;
        break;
    case'e':
        displayState = E;
        break;
    case'h':
        displayState = H;
        break;
    }
}

void displayFunc()
{
    // This function performs the actual rendering.

    // First, clear the screen.
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Set up the camera position, focus point, and the up vector.
    matrix.SetMatrixMode(OpenGLMatrix::ModelView);
    matrix.LoadIdentity();
    //matrix.LookAt(0, 1, 0, 0, 0,0, 0, 1, 0);
    //move the camer when the key 'm' is pressed and stops when key 's' is pressed
    
    if (move_cam == 1)
    {
        matrix.LookAt(positions_center[cam * 3] + normals_center[cam * 3], positions_center[cam * 3 + 1] + normals_center[cam * 3 + 1], positions_center[cam * 3 + 2] + normals_center[cam * 3 + 2],
            positions_center[cam * 3] + tangents_center[cam * 3], positions_center[cam * 3 + 1] + tangents_center[cam * 3 + 1], positions_center[cam * 3 + 2] + tangents_center[cam * 3 + 2],
            normals_center[cam * 3], normals_center[cam * 3 + 1], normals_center[cam * 3 + 2]);
        if (cam > numVertices)
        {
            cam = 0;
        }
        else
        {
            cam++;
        }

    }
    else
    {
        matrix.LookAt(positions_center[cam * 3] + normals_center[cam * 3], positions_center[cam * 3 + 1] + normals_center[cam * 3 + 1], positions_center[cam * 3 + 2] + normals_center[cam * 3 + 2],
            positions_center[cam * 3] + tangents_center[cam * 3], positions_center[cam * 3 + 1] + tangents_center[cam * 3 + 1], positions_center[cam * 3 + 2] + tangents_center[cam * 3 + 2],
            normals_center[cam * 3], normals_center[cam * 3 + 1], normals_center[cam * 3 + 2]);
    }

    float view[16];
    matrix.GetMatrix(view);

    // In here, you can do additional modeling on the object, such as performing translations, rotations and scales.
    // ...
    matrix.Translate(terrainTranslate[0], terrainTranslate[1], terrainTranslate[2]);
    matrix.Rotate(terrainRotate[0], 1.0, 0.0, 0.0);
    matrix.Rotate(terrainRotate[1], 0.0, 1.0, 0.0);
    matrix.Rotate(terrainRotate[2], 0.0, 0.0, 1.0);
    matrix.Scale(terrainScale[0], terrainScale[1], terrainScale[2]);

    // Read the current modelview and projection matrices from our helper class.
    // The matrices are only read here; nothing is actually communicated to OpenGL yet.
    float modelViewMatrix[16];
    matrix.SetMatrixMode(OpenGLMatrix::ModelView);
    matrix.GetMatrix(modelViewMatrix);

    float projectionMatrix[16];
    matrix.SetMatrixMode(OpenGLMatrix::Projection);
    matrix.GetMatrix(projectionMatrix);

    float n[16];
    matrix.SetMatrixMode(OpenGLMatrix::ModelView);
    matrix.GetNormalMatrix(n);



    //ground
    float groundmodelViewMatrix[16];
    matrix.SetMatrixMode(OpenGLMatrix::ModelView);
    matrix.GetMatrix(groundmodelViewMatrix);

    float groundprojectionMatrix[16];
    matrix.SetMatrixMode(OpenGLMatrix::Projection);
    matrix.GetMatrix(groundprojectionMatrix);

     //Upload the modelview and projection matrices to the GPU. Note that these are "uniform" variables.
     //Important: these matrices must be uploaded to *all* pipeline programs used.
     //In hw1, there is only one pipeline program, but in hw2 there will be several of them.
     //In such a case, you must separately upload to *each* pipeline program.
     //Important: do not make a typo in the variable name below; otherwise, the program will malfunction.
    pipelineProgram->Bind();
    pipelineProgram->SetUniformVariableMatrix4fv("modelViewMatrix", GL_FALSE, modelViewMatrix);
    pipelineProgram->SetUniformVariableMatrix4fv("projectionMatrix", GL_FALSE, projectionMatrix);
    pipelineProgram->SetUniformVariableMatrix4fv("normalMatrix", GL_FALSE, n);

    /* Execute the rendering.
     Bind the VAO that we want to render. Remember, one object = one VAO. 
     Render the VAO, by rendering "numVertices", starting from vertex 0.*/

    //Phong shading
    float lightDirection[3] = { 0, -1, 0 }; // the “Sun” at noon
    float viewLightDirection[3]; // light direction in the view space
    /* the following line is pseudo-code:
    viewLightDirection = (view * float4(lightDirection, 0.0)).xyz;*/
    glm::vec3 l_norm;
    for (int i = 0; i < 3; ++i) {
        l_norm[i] = 0.0f;
        for (int j = 0; j < 3; ++j) {
            l_norm[i] += view[j * 4 + i] * lightDirection[j];
        }
    }
    l_norm = glm::normalize(l_norm);

    viewLightDirection[0] = l_norm[0];
    viewLightDirection[1] = l_norm[1];
    viewLightDirection[2] = l_norm[2];


    float la[4] = { 0.2f, 0.2f, 0.2f, 1.0f }, ld[4] = { 0.8f, 0.8f, 0.8f, 1.0f }, ls[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    float Ka[4] = { 0.1f, 0.1f, 0.1f, 1.0f }, Kd[4] = { 0.7f, 0.7f, 0.7f, 1.0f }, Ks[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    float a = 2.0f;

    pipelineProgram->SetUniformVariable4fv("La", la);
    pipelineProgram->SetUniformVariable4fv("Ld", ld);
    pipelineProgram->SetUniformVariable4fv("Ls", ls);

    pipelineProgram->SetUniformVariable4fv("ka", Ka);
    pipelineProgram->SetUniformVariable4fv("kd", Kd);
    pipelineProgram->SetUniformVariable4fv("ks", Ks);
    pipelineProgram->SetUniformVariablef("alpha", a);
    // upload viewLightDirection to the GPU
    pipelineProgram->SetUniformVariable3fv("viewLightDirection", viewLightDirection);
    GLint textureLocation = glGetUniformLocation(groundpipelineProgram->GetProgramHandle(), "textureImage");
    switch (displayState)
    {
    case S:
        
        vao_center->Bind();
        glDrawArrays(GL_TRIANGLES, 0, numVertices * 24);
        break;
    case D:
        vao_right->Bind();
        glDrawArrays(GL_TRIANGLES, 0, numVertices * 24);
        vao_left->Bind();
        glDrawArrays(GL_TRIANGLES, 0, numVertices * 24);
        break;
    case  T:
        groundpipelineProgram->Bind();
        groundpipelineProgram->SetUniformVariableMatrix4fv("modelViewMatrix", GL_FALSE, modelViewMatrix);
        groundpipelineProgram->SetUniformVariableMatrix4fv("projectionMatrix", GL_FALSE, projectionMatrix);
        glActiveTexture(GL_TEXTURE0);
        glUniform1i(textureLocation, 0);
        glBindTexture(GL_TEXTURE_2D, texHandle);
        glDrawArrays(GL_TRIANGLES, 0, 1);
        vao_center->Bind();
        glDrawArrays(GL_TRIANGLES, 0, numVertices * 24);
        break;
    case E:
        groundpipelineProgram->Bind();
        groundpipelineProgram->SetUniformVariableMatrix4fv("modelViewMatrix", GL_FALSE, modelViewMatrix);
        groundpipelineProgram->SetUniformVariableMatrix4fv("projectionMatrix", GL_FALSE, projectionMatrix);
        glActiveTexture(GL_TEXTURE0);
        glUniform1i(textureLocation, 0);
        glBindTexture(GL_TEXTURE_2D, texHandle);
        glDrawArrays(GL_TRIANGLES, 0, 1);
        pipelineProgram->Bind();
        vao_right->Bind();
        glDrawArrays(GL_TRIANGLES, 0, numVertices * 24);
        vao_left->Bind();
        glDrawArrays(GL_TRIANGLES, 0, numVertices * 24);
        break;
    case H:
        vao_center->Bind();
        glDrawArrays(GL_TRIANGLES, 0, numVertices * 24);
        objpipelineProgram->Bind();
        objpipelineProgram->SetUniformVariableMatrix4fv("modelViewMatrix", GL_FALSE, modelViewMatrix);
        objpipelineProgram->SetUniformVariableMatrix4fv("projectionMatrix", GL_FALSE, projectionMatrix);
        vao_obj->Bind();
        glDrawArrays(GL_TRIANGLES, 0, 12);
    }
    
   
   
    glDepthFunc(GL_LEQUAL);
    
    skyboxpipelineProgram->Bind();
    skyboxpipelineProgram->SetUniformVariableMatrix4fv("modelViewMatrix", GL_FALSE, modelViewMatrix);
    skyboxpipelineProgram->SetUniformVariableMatrix4fv("projectionMatrix", GL_FALSE, projectionMatrix);
    

    skyboxvao->Bind();

    //skybox
    glEnable(GL_TEXTURE_CUBE_MAP);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glDisable(GL_TEXTURE_CUBE_MAP);
    glDepthFunc(GL_LESS);
    glutSwapBuffers();
}

//to caluclate the color_normals of a triagnle
void color_normal(glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, glm::vec3& n)
{
    glm::vec3 a = v1 - v0;
    glm::vec3 b = v2 - v0;

    glm::vec3 n_norm = glm::cross(a, b);
    n = glm::normalize(n_norm);
}

//void differece_height(int h_max, int &h_current)
//{
//    float gravity = 9.81f;
//    h_current= (2*gravity*(h_max-h_current));
//}
//
//float magnitude(glm::vec3 t)
//{
//    return sqrt(t[0]*t[0] + t[1] * t[1] + t[2] * t[2]);
//}

//to calculate the normals and binormals 
void normalsandbi(float*&normals, float*&binormals, float*&tangents)
{
    
    glm::vec3 v(0, 0, 1);

    glm::vec3 t0(tangents[0], tangents[1], tangents[2]);
    glm::vec3 n_norm = glm::cross(t0, v);
    glm::vec3 n = glm::normalize(n_norm);
  
    normals[0] = n[0];
    normals[1] = n[1];
    normals[2] = n[2];

    glm::vec3 b_norm = glm::cross(t0, n);
    glm::vec3 b0 = glm::normalize(b_norm);
    binormals[0] = b0[0];
    binormals[1] = b0[1];
    binormals[2] = b0[2];

    for (int i = 1; i < numVertices; i++) {
        glm::vec3 t(tangents[i * 3], tangents[i * 3 + 1], tangents[i * 3 + 2]);
        glm::vec3 b(binormals[(i - 1) * 3], binormals[(i - 1) * 3 + 1], binormals[(i - 1) * 3 + 2]);
        n_norm = glm::cross(b, t);

        n = glm::normalize(n_norm);

        normals[i * 3] = n[0];
        normals[i * 3 + 1] = n[1];
        normals[i * 3 + 2] = n[2];

        b_norm = glm::cross(t, n);
        b = glm::normalize(b_norm);

        binormals[i * 3] = b[0];
        binormals[i * 3 + 1] = b[1];
        binormals[i * 3 + 2] = b[2];   
    }

}

//to calulate the vertices for the rail.
void railcoordinates(float*& vertices, float* &colors,float*&positions,float*&normals,float*&binormals)
{
    int totalvertices = numVertices * 24;
    int c_vertices = 0;
    
    glm::vec3 n;
    float alpha = 0.025f;
    for (int i = 0; i < numVertices - 1; i++)
    {
        glm::vec3 p(positions[i * 3], positions[i * 3 + 1], positions[i * 3 + 2]);
        glm::vec3 n(normals[i * 3], normals[i * 3 + 1], normals[i * 3 + 2]);
        glm::vec3 b(binormals[i * 3], binormals[i * 3 + 1], binormals[i * 3 + 2]);

        glm::vec3 v0 = p + alpha * (-n + b);
        glm::vec3 v1 = p + alpha * (n + b);
        glm::vec3 v2 = p + alpha * (n - b);
        glm::vec3 v3 = p + alpha * (-n - b);

        p = glm::vec3(positions[(i + 1) * 3], positions[(i + 1) * 3 + 1], positions[(i + 1) * 3 + 2]);
        n = glm::vec3(normals[(i + 1) * 3], normals[(i + 1) * 3 + 1], normals[(i + 1) * 3 + 2]);
        b = glm::vec3(binormals[(i + 1) * 3], binormals[(i + 1) * 3 + 1], binormals[(i + 1) * 3 + 2]);

        glm::vec3 v4 = p + alpha * (-n + b);
        glm::vec3 v5 = p + alpha * (n + b);
        glm::vec3 v6 = p + alpha * (n - b);
        glm::vec3 v7 = p + alpha * (-n - b);

        //face1 triangle1 -- right
        color_normal(v0, v4, v5, n);

        vertices[c_vertices * 3] = v4[0];
        vertices[c_vertices * 3 + 1] = v4[1];
        vertices[c_vertices * 3 + 2] = v4[2];

        colors[c_vertices * 4] = n[0];
        colors[c_vertices * 4 + 1] = n[1];
        colors[c_vertices * 4 + 2] = n[2];
        colors[c_vertices * 4 + 3] = 1.0f;
        c_vertices++;

        vertices[c_vertices * 3] = v0[0];
        vertices[c_vertices * 3 + 1] = v0[1];
        vertices[c_vertices * 3 + 2] = v0[2];

        colors[c_vertices * 4] = n[0];
        colors[c_vertices * 4 + 1] = n[1];
        colors[c_vertices * 4 + 2] = n[2];
        colors[c_vertices * 4 + 3] = 1.0f;
        c_vertices++;

        vertices[c_vertices * 3] = v1[0];
        vertices[c_vertices * 3 + 1] = v1[1];
        vertices[c_vertices * 3 + 2] = v1[2];
        colors[c_vertices * 4] = n[0];
        colors[c_vertices * 4 + 1] = n[1];
        colors[c_vertices * 4 + 2] = n[2];
        colors[c_vertices * 4 + 3] = 1.0f;
        c_vertices++;

        //face1 triangle2 -- right
        color_normal(v0, v5, v1, n);

        vertices[c_vertices * 3] = v1[0];
        vertices[c_vertices * 3 + 1] = v1[1];
        vertices[c_vertices * 3 + 2] = v1[2];
        colors[c_vertices * 4] = n[0];
        colors[c_vertices * 4 + 1] = n[1];
        colors[c_vertices * 4 + 2] = n[2];
        colors[c_vertices * 4 + 3] = 1.0f;
        c_vertices++;

        vertices[c_vertices * 3] = v4[0];
        vertices[c_vertices * 3 + 1] = v4[1];
        vertices[c_vertices * 3 + 2] = v4[2];
        colors[c_vertices * 4] = n[0];
        colors[c_vertices * 4 + 1] = n[1];
        colors[c_vertices * 4 + 2] = n[2];
        colors[c_vertices * 4 + 3] = 1.0f;
        c_vertices++;

        vertices[c_vertices * 3] = v5[0];
        vertices[c_vertices * 3 + 1] = v5[1];
        vertices[c_vertices * 3 + 2] = v5[2];
        colors[c_vertices * 4] = n[0];
        colors[c_vertices * 4 + 1] = n[1];
        colors[c_vertices * 4 + 2] = n[2];
        colors[c_vertices * 4 + 3] = 1.0f;
        c_vertices++;

        //face2 triangle1 -- top
        color_normal(v1, v5, v6, n);
        vertices[c_vertices * 3] = v5[0];
        vertices[c_vertices * 3 + 1] = v5[1];
        vertices[c_vertices * 3 + 2] = v5[2];
        colors[c_vertices * 4] = n[0];
        colors[c_vertices * 4 + 1] = n[1];
        colors[c_vertices * 4 + 2] = n[2];
        colors[c_vertices * 4 + 3] = 1.0f;
        c_vertices++;

        vertices[c_vertices * 3] = v1[0];
        vertices[c_vertices * 3 + 1] = v1[1];
        vertices[c_vertices * 3 + 2] = v1[2];
        colors[c_vertices * 4] = n[0];
        colors[c_vertices * 4 + 1] = n[1];
        colors[c_vertices * 4 + 2] = n[2];
        colors[c_vertices * 4 + 3] = 1.0f;
        c_vertices++;

        vertices[c_vertices * 3] = v6[0];
        vertices[c_vertices * 3 + 1] = v6[1];
        vertices[c_vertices * 3 + 2] = v6[2];
        colors[c_vertices * 4] = n[0];
        colors[c_vertices * 4 + 1] = n[1];
        colors[c_vertices * 4 + 2] = n[2];
        colors[c_vertices * 4 + 3] = 1.0f;
        c_vertices++;

        //face2 triangle2 -- top
        color_normal(v1, v6, v2, n);
        vertices[c_vertices * 3] = v1[0];
        vertices[c_vertices * 3 + 1] = v1[1];
        vertices[c_vertices * 3 + 2] = v1[2];
        colors[c_vertices * 4] = n[0];
        colors[c_vertices * 4 + 1] = n[1];
        colors[c_vertices * 4 + 2] = n[2];
        colors[c_vertices * 4 + 3] = 1.0f;
        c_vertices++;

        vertices[c_vertices * 3] = v6[0];
        vertices[c_vertices * 3 + 1] = v6[1];
        vertices[c_vertices * 3 + 2] = v6[2];
        colors[c_vertices * 4] = n[0];
        colors[c_vertices * 4 + 1] = n[1];
        colors[c_vertices * 4 + 2] = n[2];
        colors[c_vertices * 4 + 3] = 1.0f;
        c_vertices++;

        vertices[c_vertices * 3] = v2[0];
        vertices[c_vertices * 3 + 1] = v2[1];
        vertices[c_vertices * 3 + 2] = v2[2];
        colors[c_vertices * 4] = n[0];
        colors[c_vertices * 4 + 1] = n[1];
        colors[c_vertices * 4 + 2] = n[2];
        colors[c_vertices * 4 + 3] = 1.0f;
        c_vertices++;

        //face3 triangle1 -- left
        color_normal(v2, v6, v7, n);
        vertices[c_vertices * 3] = v6[0];
        vertices[c_vertices * 3 + 1] = v6[1];
        vertices[c_vertices * 3 + 2] = v6[2];
        colors[c_vertices * 4] = n[0];
        colors[c_vertices * 4 + 1] = n[1];
        colors[c_vertices * 4 + 2] = n[2];
        colors[c_vertices * 4 + 3] = 1.0f;
        c_vertices++;

        vertices[c_vertices * 3] = v2[0];
        vertices[c_vertices * 3 + 1] = v2[1];
        vertices[c_vertices * 3 + 2] = v2[2];
        colors[c_vertices * 4] = n[0];
        colors[c_vertices * 4 + 1] = n[1];
        colors[c_vertices * 4 + 2] = n[2];
        colors[c_vertices * 4 + 3] = 1.0f;
        c_vertices++;

        vertices[c_vertices * 3] = v7[0];
        vertices[c_vertices * 3 + 1] = v7[1];
        vertices[c_vertices * 3 + 2] = v7[2];
        colors[c_vertices * 4] = n[0];
        colors[c_vertices * 4 + 1] = n[1];
        colors[c_vertices * 4 + 2] = n[2];
        colors[c_vertices * 4 + 3] = 1.0f;
        c_vertices++;

        //face3 triangle2 -- left
        color_normal(v2, v7, v3, n);
        vertices[c_vertices * 3] = v2[0];
        vertices[c_vertices * 3 + 1] = v2[1];
        vertices[c_vertices * 3 + 2] = v2[2];
        colors[c_vertices * 4] = n[0];
        colors[c_vertices * 4 + 1] = n[1];
        colors[c_vertices * 4 + 2] = n[2];
        colors[c_vertices * 4 + 3] = 1.0f;
        c_vertices++;

        vertices[c_vertices * 3] = v7[0];
        vertices[c_vertices * 3 + 1] = v7[1];
        vertices[c_vertices * 3 + 2] = v7[2];
        colors[c_vertices * 4] = n[0];
        colors[c_vertices * 4 + 1] = n[1];
        colors[c_vertices * 4 + 2] = n[2];
        colors[c_vertices * 4 + 3] = 1.0f;
        c_vertices++;

        vertices[c_vertices * 3] = v3[0];
        vertices[c_vertices * 3 + 1] = v3[1];
        vertices[c_vertices * 3 + 2] = v3[2];
        colors[c_vertices * 4] = n[0];
        colors[c_vertices * 4 + 1] = n[1];
        colors[c_vertices * 4 + 2] = n[2];
        colors[c_vertices * 4 + 3] = 1.0f;
        c_vertices++;

        //face4 triangle1 -- bottom
        color_normal(v3, v7, v4, n);
        vertices[c_vertices * 3] = v7[0];
        vertices[c_vertices * 3 + 1] = v7[1];
        vertices[c_vertices * 3 + 2] = v7[2];
        colors[c_vertices * 4] = n[0];
        colors[c_vertices * 4 + 1] = n[1];
        colors[c_vertices * 4 + 2] = n[2];
        colors[c_vertices * 4 + 3] = 1.0f;
        c_vertices++;

        vertices[c_vertices * 3] = v3[0];
        vertices[c_vertices * 3 + 1] = v3[1];
        vertices[c_vertices * 3 + 2] = v3[2];
        colors[c_vertices * 4] = n[0];
        colors[c_vertices * 4 + 1] = n[1];
        colors[c_vertices * 4 + 2] = n[2];
        colors[c_vertices * 4 + 3] = 1.0f;
        c_vertices++;

        vertices[c_vertices * 3] = v4[0];
        vertices[c_vertices * 3 + 1] = v4[1];
        vertices[c_vertices * 3 + 2] = v4[2];
        colors[c_vertices * 4] = n[0];
        colors[c_vertices * 4 + 1] = n[1];
        colors[c_vertices * 4 + 2] = n[2];
        colors[c_vertices * 4 + 3] = 1.0f;
        c_vertices++;

        //face4 triangle2 -- bottom
        color_normal(v3, v4, v0, n);
        vertices[c_vertices * 3] = v3[0];
        vertices[c_vertices * 3 + 1] = v3[1];
        vertices[c_vertices * 3 + 2] = v3[2];
        colors[c_vertices * 4] = n[0];
        colors[c_vertices * 4 + 1] = n[1];
        colors[c_vertices * 4 + 2] = n[2];
        colors[c_vertices * 4 + 3] = 1.0f;
        c_vertices++;

        vertices[c_vertices * 3] = v4[0];
        vertices[c_vertices * 3 + 1] = v4[1];
        vertices[c_vertices * 3 + 2] = v4[2];
        colors[c_vertices * 4] = n[0];
        colors[c_vertices * 4 + 1] = n[1];
        colors[c_vertices * 4 + 2] = n[2];
        colors[c_vertices * 4 + 3] = 1.0f;
        c_vertices++;

        vertices[c_vertices * 3] = v0[0];
        vertices[c_vertices * 3 + 1] = v0[1];
        vertices[c_vertices * 3 + 2] = v0[2];
        colors[c_vertices * 4] = n[0];
        colors[c_vertices * 4 + 1] = n[1];
        colors[c_vertices * 4 + 2] = n[2];
        colors[c_vertices * 4 + 3] = 1.0f;
        c_vertices++;

    }
}


void initScene(int argc, char* argv[])
{
    // Load spline from the provided filename.
    loadSpline(argv[1]);

    printf("Loaded spline with %d control point(s).\n", spline.numControlPoints);

    // Set the background color.
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f); // Black color.

    // Enable z-buffering (i.e., hidden surface removal using the z-buffer algorithm).
    glEnable(GL_DEPTH_TEST);


    // Create a pipeline program. This operation must be performed BEFORE we initialize any VAOs.
    // A pipeline program contains our shaders. Different pipeline programs may contain different shaders.
    // In this homework, we only have one set of shaders, and therefore, there is only one pipeline program.
    // In hw2, we will need to shade different objects with different shaders, and therefore, we will have
    // several pipeline programs (e.g., one for the rails, one for the ground/sky, etc.).
    pipelineProgram = new PipelineProgram(); // Load and set up the pipeline program, including its shaders.

    // Load and set up the pipeline program, including its shaders.
    if (pipelineProgram->BuildShadersFromFiles(shaderBasePath, "lightvertexShader.glsl", "lightfragmentShader.glsl") != 0)
    {
        cout << "Failed to build the pipeline program." << endl;
        throw 1;
    }
    cout << "Successfully built the pipeline program." << endl;

    // Bind the pipeline program that we just created. 
    // The purpose of binding a pipeline program is to activate the shaders that it contains, i.e.,
    // any object rendered from that point on, will use those shaders.
    // When the application starts, no pipeline program is bound, which means that rendering is not set up.
    // So, at some point (such as below), we need to bind a pipeline program.
    // From that point on, exactly one pipeline program is bound at any moment of time.
    pipelineProgram->Bind();

    // Prepare the triangle position and color data for the VBO. 
    // The code below sets up a single triangle (3 vertices).
    // The triangle will be rendered using GL_TRIANGLES (in displayFunc()).

    
    numVertices = (spline.numControlPoints - 3) * u_NumPoints;

    // Vertex positions.
    positions_center = (float*)malloc(numVertices * 3 * sizeof(float)); // 3 floats per vertex, i.e., x,y,z
    colors_center = (float*)malloc(numVertices * 4 * sizeof(float));
    tangents_center = (float*)malloc(numVertices * 3 * sizeof(float));

    printf(" %d ", numVertices);
    int count = 0;
    double m[16] = { -s,2.0f * s,-s,0,2.0f - s,s - 3.0f,0,1,s - 2.0f,3 - (2.0f * s),s,0,s,-s,0,0 };
    glm::vec3 h_max{ 0.0f,0.0f,0.0f };

    //Computer control matrix for every 4 points
    for (int i = 0; i < spline.numControlPoints - 3; i++)
    {
        double C[12] = { spline.points[i].x,spline.points[i + 1].x,spline.points[i + 2].x,spline.points[i + 3].x,spline.points[i].y,spline.points[i + 1].y,spline.points[i + 2].y,spline.points[i + 3].y,spline.points[i].z,spline.points[i + 1].z,spline.points[i + 2].z,spline.points[i + 3].z };
        //iterate over u from [0,1] of the interval 0.001
        for (float j = u_start; j <= u_end; j = j + 0.001)
        {
            double u[4] = { pow(j,3),pow(j,2),j,1 };
            //p(u) = [u^3 u^2 u 1] M C
            double a[4];
            double p[3];
            MultiplyMatrices(1, 4, 4, u, m, a);
            MultiplyMatrices(1, 4, 3, a, C, p);

            glm::vec3 p_current{ p[0],p[1],p[2] };
            h_max = glm::max(h_max, p_current);

            positions_center[3 * count] = p[0];
            positions_center[3 * count + 1] = p[1];
            positions_center[3 * count + 2] = p[2];

            colors_center[count * 4] = 1.0f;
            colors_center[count * 4 + 1] = 0.0f;
            colors_center[count * 4 + 2] = 0.0f;
            colors_center[count * 4 + 3] = 1.0f;

            double t[4] = { 3 * pow(j,2),2 * j,1,0 };
            //p'(u) = [3*u^2 u*2 1 0] M C
            double at[4];
            double pt[3];
            MultiplyMatrices(1, 4, 4, t, m, at);
            MultiplyMatrices(1, 4, 3, at, C, pt);

            tangents_center[3 * count] = pt[0];
            tangents_center[3 * count + 1] = pt[1];
            tangents_center[3 * count + 2] = pt[2];

            count = count + 1;
        }
    }
    normals_center = (float*)malloc(numVertices * 3 * sizeof(float));
    binormals_center = (float*)malloc(numVertices * 3 * sizeof(float));
    normalsandbi(normals_center,binormals_center,tangents_center);
    

    positions_left = (float*)malloc(numVertices * 3 * sizeof(float)); // 3 floats per vertex, i.e., x,y,z
    colors_left = (float*)malloc(numVertices * 4 * sizeof(float));
    tangents_left = (float*)malloc(numVertices * 3 * sizeof(float));
    positions_right = (float*)malloc(numVertices * 3 * sizeof(float)); // 3 floats per vertex, i.e., x,y,z
    colors_right = (float*)malloc(numVertices * 4 * sizeof(float));
    tangents_right = (float*)malloc(numVertices * 3 * sizeof(float));
    count = 0;

   /* for (int i = 0; i < numVertices; i++)
    {
        glm::vec3 u_current { positions[i * 3],positions[i * 3 + 1],positions[i * 3 + 2] };
        glm::vec3 t_current{ tangents[i * 3],tangents[i * 3 + 1],tangents[i * 3 + 2] };
        differece_height(h_max, u_current);
        glm::vec3 u_new = u_current + u_interval * (u_current / magnitude(t_current));
        positions_new[3 * count] = u_new[0];
        positions_new[3 * count + 1] = u_new[1];
        positions_new[3 * count + 2] = u_new[2];
    }*/
    /*positions_new = (float*)malloc(numVertices * sizeof(float));
    for (int i = u_start; i < u_end; i = i + u_interval)
    {
        glm::vec3 t_current{ tangents[i * 3],tangents[i * 3 + 1],tangents[i * 3 + 2] };
        differece_height(u_NumPoints, i);
        int u_new =i + u_interval * (i / magnitude(t_current));
        positions_new[i] = u_new;
    }*/
    for (int i = 0; i < numVertices; i++)
    {
        glm::vec3 b(binormals_center[3 * i], binormals_center[3 * i + 1], binormals_center[3 * i + 2]);
       
        glm::vec3 leftOffset = -b * offsetAmount; // Adjust offsetAmount as needed
        glm::vec3 rightOffset = b * offsetAmount;
        // Calculate left and right positions by applying offsets to the center spline positions
        glm::vec3 centerPosition(positions_center[3 * i], positions_center[3 * i + 1], positions_center[3 * i + 2]);
        glm::vec3 leftPosition = centerPosition + leftOffset;
        glm::vec3 rightPosition = centerPosition + rightOffset;

        positions_left[3 * i] = leftPosition[0];
        positions_left[3 * i+1] = leftPosition[1];
        positions_left[3 * i+2] = leftPosition[2];

        positions_right[3 * i] = rightPosition[0];
        positions_right[3 * i + 1] = rightPosition[1];
        positions_right[3 * i + 2] = rightPosition[2];       
    }
    normals_left = (float*)malloc(numVertices * 3 * sizeof(float));
    binormals_left = (float*)malloc(numVertices * 3 * sizeof(float));
    normalsandbi(normals_left, binormals_left, tangents_center);
    normals_right = (float*)malloc(numVertices * 3 * sizeof(float));
    binormals_right = (float*)malloc(numVertices * 3 * sizeof(float));
    normalsandbi(normals_right, binormals_right, tangents_center);
  
    vertices_left = (float*)malloc(numVertices*24 * 3 * sizeof(float));
    colors_left = (float*)malloc(numVertices*24* 4 * sizeof(float)); // 4 floats per vertex, i.e., r,g,b,a
    railcoordinates(vertices_left, colors_left, positions_left, normals_left, binormals_left);
    vertices_right = (float*)malloc(numVertices*24 * 3 * sizeof(float));
    colors_right = (float*)malloc(numVertices*24 * 4 * sizeof(float)); // 4 floats per vertex, i.e., r,g,b,a
    railcoordinates(vertices_right, colors_right, positions_right, normals_right, binormals_right);

    vertices_center = (float*)malloc(numVertices * 24 * 3 * sizeof(float));
    colors_center = (float*)malloc(numVertices * 24 * 4 * sizeof(float)); // 4 floats per vertex, i.e., r,g,b,a
    railcoordinates(vertices_center, colors_center, positions_center, normals_center, binormals_center);
    // Create the VBOs. 
    // We make a separate VBO for vertices and colors. 
    // This operation must be performed BEFORE we initialize any VAOs.

    vboVertices_center = new VBO(numVertices * 24, 3, vertices_center, GL_STATIC_DRAW); // 3 values per position
    vboColors_center = new VBO(numVertices * 24, 4, colors_center, GL_STATIC_DRAW); // 4 values per color

    vboVertices_left = new VBO(numVertices*24, 3, vertices_right, GL_STATIC_DRAW); // 3 values per position
    vboColors_left = new VBO(numVertices*24, 4, colors_right, GL_STATIC_DRAW); // 4 values per color

    vboVertices_right = new VBO(numVertices * 24, 3, vertices_left, GL_STATIC_DRAW); // 3 values per position
    vboColors_right = new VBO(numVertices * 24, 4, colors_left, GL_STATIC_DRAW); // 4 values per color

    // Create the VAOs. There is a single VAO in this example.
    // Important: this code must be executed AFTER we created our pipeline program, and AFTER we set up our VBOs.
    // A VAO contains the geometry for a single object. There should be one VAO per object.
    // In this homework, "geometry" means vertex positions and colors. In homework 2, it will also include
    // vertex normal and vertex texture coordinates for texture mapping.
    vao_left = new VAO();
    vao_right = new VAO();
    vao_center = new VAO();
    // Set up the relationship between the "position" shader variable and the VAO.
    // Important: any typo in the shader variable name will lead to malfunction.
    // Set up the relationship between the "color" shader variable and the VAO.
    // Important: any typo in the shader variable name will lead to malfunction.
    vao_center->ConnectPipelineProgramAndVBOAndShaderVariable(pipelineProgram, vboVertices_center, "position");
    vao_center->ConnectPipelineProgramAndVBOAndShaderVariable(pipelineProgram, vboColors_center, "color");

    vao_left->ConnectPipelineProgramAndVBOAndShaderVariable(pipelineProgram, vboVertices_left, "position");
    vao_left->ConnectPipelineProgramAndVBOAndShaderVariable(pipelineProgram, vboColors_left, "color");

    vao_right->ConnectPipelineProgramAndVBOAndShaderVariable(pipelineProgram, vboVertices_right, "position");
    vao_right->ConnectPipelineProgramAndVBOAndShaderVariable(pipelineProgram, vboColors_right, "color");

    // We don't need this data any more, as we have already uploaded it to the VBO. And so we can destroy it, to avoid a memory leak.
    
     //ground
    groundpipelineProgram = new PipelineProgram();//ground
    if (groundpipelineProgram->BuildShadersFromFiles(shaderBasePath, "groundvertexShader.glsl", "groundfragmentShader.glsl") != 0)
    {
        cout << "Failed to build the ground pipeline program." << endl;
        throw 1;
    }
    cout << "Successfully built the ground pipeline program." << endl;
    groundpipelineProgram->Bind();

   glGenTextures(1, &texHandle);
    int code = initTexture("skybox/right.jpg", texHandle);
    if (code != 0)
    {
        printf("Error loading the texture image");
        exit(EXIT_FAILURE);
    }
    float pos1[] = { 0,0,0 };
    float uvs[] = { 0,0 };
    VBO* vboPos1 = new VBO(1, 3, pos1, GL_STATIC_DRAW);
    VBO* vbouvs = new VBO(1, 2, uvs, GL_STATIC_DRAW);
    groundvao = new VAO();
    groundvao->ConnectPipelineProgramAndVBOAndShaderVariable(groundpipelineProgram, vboPos1, "position");
    groundvao->ConnectPipelineProgramAndVBOAndShaderVariable(groundpipelineProgram, vbouvs, "texCoord");
    

    skyboxpipelineProgram = new PipelineProgram();//ground
    if (skyboxpipelineProgram->BuildShadersFromFiles(shaderBasePath, "texturevertexShader.glsl", "texturefragmentShader.glsl") != 0)
    {
        cout << "Failed to build the ground pipeline program." << endl;
        throw 1;
    }
    cout << "Successfully built the ground pipeline program." << endl;
    skyboxpipelineProgram->Bind();

    float p = 1024.0f;
    //   Coordinates
    float pos[] = {
        // Right
      p, -p,  p, p, -p, -p, p,  p, -p,
      p,  p, -p, p,  p,  p, p, -p,  p,
      // Left
      -p, -p,  p, -p,  p,  p, -p,  p, -p,
      -p,  p, -p, -p, -p, -p, -p, -p,  p,
      // Top
      -p,  p,  p, p,  p,  p, p,  p, -p,
      p,  p, -p, -p,  p, -p, -p,  p, p,
      // Bottom
      -p, -p,  p, -p, -p, -p, p, -p, -p,
      p, -p, -p, p, -p,  p, -p, -p,  p,
      // Back
      -p, -p,  p, p, -p,  p,p,  p,  p,
      p,  p,  p, -p,  p,  p, -p, -p,  p,
      // Front
      -p, -p, -p, -p,  p, -p, p,  p, -p,
      p,  p, -p, p, -p, -p, -p, -p, -p

    };

    // All the faces of the cubemap (make sure they are in this exact order)
    char* facesCubemap[6] =
    {
        "skybox/right.jpg",
        "skybox/left.jpg",
        "skybox/bottom.jpg",
        "skybox/top.jpg",
        "skybox/front.jpg",
        "skybox/back.jpg"
    };
    // Creates the cubemap texture object
    
    glGenTextures(1, &cubemapTexture);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
   
    // Cycles through all the textures and attaches them to the cubemap object
    for ( int i = 0; i < 6; i++)
    {
        
        // Read the texture image.
        ImageIO img;
        ImageIO::fileFormatType imgFormat;
        ImageIO::errorType err = img.load(facesCubemap[i], &imgFormat);

        if (err != ImageIO::OK)
        {
            printf("Loading texture from %s failed.\n", facesCubemap[i]);

        }

        // Check that the number of bytes is a multiple of 4.
        if (img.getWidth() * img.getBytesPerPixel() % 4)
        {
            printf("Error (%s): The width*numChannels in the loaded image must be a multiple of 4.\n", facesCubemap[i]);

        }

        // Allocate space for an array of pixels.
        int width = img.getWidth();
        int height = img.getHeight();
        unsigned char* pixelsRGBA = new unsigned char[4 * width * height]; // we will use 4 bytes per pixel, i.e., RGBA
        printf(" %d %d ", width, height);
        // Fill the pixelsRGBA array with the image pixels.
        memset(pixelsRGBA, 0, 4 * width * height); // set all bytes to 0
        for (int h = 0; h < height; h++)
        {
            for (int w = 0; w < width; w++)
            {
                // assign some default byte values (for the case where img.getBytesPerPixel() < 4)
                pixelsRGBA[4 * (h * width + w) + 0] = 0; // red
                pixelsRGBA[4 * (h * width + w) + 1] = 0; // green
                pixelsRGBA[4 * (h * width + w) + 2] = 0; // blue
                pixelsRGBA[4 * (h * width + w) + 3] = 255; // alpha channel; fully opaque

                // set the RGBA channels, based on the loaded image
                int numChannels = img.getBytesPerPixel();

                for (int c = 0; c < numChannels; c++) // only set as many channels as are available in the loaded image; the rest get the default value
                {
                    pixelsRGBA[4 * (h * width + w) + c] = img.getPixel(w, h, c);

                }
            }
        }
        // Initialize the texture.
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixelsRGBA);
        
    }
    // Set the texture parameters.
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    // These are very important to prevent seams
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    // This might help with seams on some systems
    //glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);


    VBO* vboPos = new VBO(36, 3, pos, GL_STATIC_DRAW);
    skyboxvao = new VAO();
    skyboxvao->ConnectPipelineProgramAndVBOAndShaderVariable(skyboxpipelineProgram, vboPos, "position");

    objpipelineProgram = new PipelineProgram(); // Load and set up the pipeline program, including its shaders.

    // Load and set up the pipeline program, including its shaders.
    if (objpipelineProgram->BuildShadersFromFiles(shaderBasePath, "vertexShader.glsl", "fragmentShader.glsl") != 0)
    {
        cout << "Failed to build the pipeline program." << endl;
        throw 1;
    }
    cout << "Successfully built the pipeline program." << endl;
    
    float n = 10.0f;
    float pos_obj[] = { // First triangle
    n, n, n,    // Vertex 1: (n, n, n)
    n, -n, -n,  // Vertex 2: (n, -n, -n)
    -n, n, -n,  // Vertex 3: (-n, n, -n)

    // Second triangle
    -n, -n, n,  // Vertex 4: (-n, -n, n)
    n, -n, -n,  // Vertex 5: (n, -n, -n)
    n, n, n,  // Vertex 6: (-n, n, -n)

    // Third triangle
    n, n, n,  // Vertex 7: (-n, -n, n)
    n, -n, -n,   // Vertex 8: (-n, n, n)
    -n, -n, n,  // Vertex 9: (n, -n, -n)

    // Fourth triangle
    n, -n, -n,   // Vertex 10: (-n, n, n)
    -n, -n, n,    // Vertex 11: (n, n, n)
    -n, n, -n   // Vertex 12: (n, -n, -n) 
    };
    float c_obj[] = { 1.0f,1.0f,1.0f,1.0f ,
        1.0f,1.0f,1.0f,1.0f ,
        1.0f,1.0f,1.0f,1.0f  ,
        1.0f,1.0f,1.0f,1.0f,
    1.0f,1.0f,1.0f,1.0f , 
    1.0f,1.0f,1.0f,1.0f , 
    1.0f,1.0f,1.0f,1.0f , 
    1.0f,1.0f,1.0f,1.0f , 
    1.0f,1.0f,1.0f,1.0f , 
    1.0f,1.0f,1.0f,1.0f , 
    1.0f,1.0f,1.0f,1.0f , 
    1.0f,1.0f,1.0f,1.0f };
        
   

    vboVertices_obj = new VBO(12, 3, pos_obj, GL_STATIC_DRAW);
    vboColors_obj = new VBO(12, 4, c_obj, GL_STATIC_DRAW);

    vao_obj = new VAO();
    vao_obj->ConnectPipelineProgramAndVBOAndShaderVariable(objpipelineProgram, vboVertices_obj, "position");
    vao_obj->ConnectPipelineProgramAndVBOAndShaderVariable(objpipelineProgram, vboColors_obj, "color");

    // Check for any OpenGL errors.
    std::cout << "GL error status is: " << glGetError() << std::endl;
}

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        printf("usage: %s <trackfile>\n", argv[0]);
        exit(0);
    }

    cout << "Initializing GLUT..." << endl;
    glutInit(&argc, argv);

    cout << "Initializing OpenGL..." << endl;

#ifdef __APPLE__
    glutInitDisplayMode(GLUT_3_2_CORE_PROFILE | GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH | GLUT_STENCIL);
#else
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH | GLUT_STENCIL);
#endif

    glutInitWindowSize(windowWidth, windowHeight);
    glutInitWindowPosition(0, 0);
    glutCreateWindow(windowTitle);

    cout << "OpenGL Version: " << glGetString(GL_VERSION) << endl;
    cout << "OpenGL Renderer: " << glGetString(GL_RENDERER) << endl;
    cout << "Shading Language Version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << endl;

#ifdef __APPLE__
    // This is needed on recent Mac OS X versions to correctly display the window.
    glutReshapeWindow(windowWidth - 1, windowHeight - 1);
#endif

    // Tells GLUT to use a particular display function to redraw.
    glutDisplayFunc(displayFunc);
    // Perform animation inside idleFunc.
    glutIdleFunc(idleFunc);
    // callback for mouse drags
    glutMotionFunc(mouseMotionDragFunc);
    // callback for idle mouse movement
    glutPassiveMotionFunc(mouseMotionFunc);
    // callback for mouse button changes
    glutMouseFunc(mouseButtonFunc);
    // callback for resizing the window
    glutReshapeFunc(reshapeFunc);
    // callback for pressing the keys on the keyboard
    glutKeyboardFunc(keyboardFunc);

    // init glew
#ifdef __APPLE__
  // nothing is needed on Apple
#else
  // Windows, Linux
    GLint result = glewInit();
    if (result != GLEW_OK)
    {
        cout << "error: " << glewGetErrorString(result) << endl;
        exit(EXIT_FAILURE);
    }
#endif

    initScene(argc, argv);

    // Sink forever into the GLUT loop.
    glutMainLoop();
}

