/* Copyright (c) 2013-2017, ARM Limited and Contributors
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* [Includes] */
#include <jni.h>
#include <android/log.h>

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <algorithm>
#include <chrono>
#include <vector>

using namespace std;
using namespace std::chrono;

#define LOG_TAG "libNative"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
/* [Includes] */

/* [Vertex source] */
static const char glVertexShader[] =
        "attribute vec4 vPosition;\n"
        "void main()\n"
        "{\n"
        "  gl_Position = vPosition;\n"
        "}\n";
/* [Vertex source] */

/* [Fragment source] */
static const char glFragmentShader[] =
        "precision mediump float;\n"
        "uniform lowp vec4 fColor;\n"
        "void main()\n"
        "{\n"
        "  gl_FragColor = fColor;\n"
        "}\n";
/* [Fragment source] */

/* [loadShader] */
GLuint loadShader(GLenum shaderType, const char* shaderSource)
{
    GLuint shader = glCreateShader(shaderType);
    if (shader)
    {
        glShaderSource(shader, 1, &shaderSource, NULL);
        glCompileShader(shader);

        GLint compiled = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);

        if (!compiled)
        {
            GLint infoLen = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);

            if (infoLen)
            {
                char * buf = (char*) malloc(infoLen);

                if (buf)
                {
                    glGetShaderInfoLog(shader, infoLen, NULL, buf);
                    LOGE("Could not Compile Shader %d:\n%s\n", shaderType, buf);
                    free(buf);
                }

                glDeleteShader(shader);
                shader = 0;
            }
        }
    }

    return shader;
}
/* [loadShader] */

/* [createProgram] */
GLuint createProgram(const char* vertexSource, const char * fragmentSource)
{
    GLuint vertexShader = loadShader(GL_VERTEX_SHADER, vertexSource);
    if (!vertexShader)
    {
        return 0;
    }

    GLuint fragmentShader = loadShader(GL_FRAGMENT_SHADER, fragmentSource);
    if (!fragmentShader)
    {
        return 0;
    }

    GLuint program = glCreateProgram();

    if (program)
    {
        glAttachShader(program , vertexShader);
        glAttachShader(program, fragmentShader);

        glLinkProgram(program);
        GLint linkStatus = GL_FALSE;

        glGetProgramiv(program , GL_LINK_STATUS, &linkStatus);

        if( linkStatus != GL_TRUE)
        {
            GLint bufLength = 0;

            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &bufLength);

            if (bufLength)
            {
                char* buf = (char*) malloc(bufLength);

                if (buf)
                {
                    glGetProgramInfoLog(program, bufLength, NULL, buf);
                    LOGE("Could not link program:\n%s\n", buf);
                    free(buf);
                }
            }
            glDeleteProgram(program);
            program = 0;
        }
    }

    return program;
}
/* [createProgram] */

/* [setupGraphics] */
GLuint simpleTriangleProgram;
GLuint vPosition;
GLuint fColor;

bool setupGraphics(int w, int h)
{
    simpleTriangleProgram = createProgram(glVertexShader, glFragmentShader);

    if (!simpleTriangleProgram)
    {
        LOGE ("Could not create program");
        return false;
    }

    vPosition = glGetAttribLocation(simpleTriangleProgram, "vPosition");
    fColor = glGetUniformLocation(simpleTriangleProgram, "fColor");

    LOGI("fColor=%d",fColor);

    glViewport(0, 0, w, h);

    return true;
}
/* [setupGraphics] */

/* [renderFrame] */
static bool initialized = false;
static GLfloat * verts = nullptr;

// Frame Duty
static int s_Step = 0;

// Statistics
static const int k_MaxStatCount = 100;
static int s_StatCount = 0;
static vector<int> s_TimingsO(k_MaxStatCount);
static vector<int> s_TimingsSMSR(k_MaxStatCount);
static vector<int> s_TimingsSMDR(k_MaxStatCount);
static vector<int> s_TimingsDM(k_MaxStatCount);
static vector<int> s_TimingsSMSRScissors(k_MaxStatCount);
static vector<int> s_TimingsSMSRColor(k_MaxStatCount);
static vector<int> s_TimingsSMSRDepth(k_MaxStatCount);

void renderFrame()
{
    const int k_Instances = 100;
    const bool useSingleMesh = false;

    if (!initialized)
    {
        int vertCount = k_Instances * 6;
        verts = new GLfloat[vertCount];
        for(int i = 0 ; i < vertCount; i += 6)
        {
            verts[i+0] = 0.0f;
            verts[i+1] = 0.1f;
            verts[i+2] = -0.1f;
            verts[i+3] = -0.1f;
            verts[i+4] = 0.1f;
            verts[i+5] = -0.1f;
        }

        initialized = true;
    }

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear (GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
    glUseProgram(simpleTriangleProgram);
    glEnableVertexAttribArray(vPosition);
    glUniform4f(fColor, 1, 0, 0, 1);

    // Warmup, Draw the last triangle
    {
        glVertexAttribPointer(vPosition, 2, GL_FLOAT, GL_FALSE, 0, &verts[(k_Instances-1)*6]);
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }

    // Draw Once
    if(s_Step == 0)
    {
        auto t0 = high_resolution_clock::now();
        glVertexAttribPointer(vPosition, 2, GL_FLOAT, GL_FALSE, 0, verts);
        glDrawArrays(GL_TRIANGLES, 0, 3 * k_Instances);
        auto t1 = high_resolution_clock::now();
        auto dt_us = duration_cast<microseconds>(t1 - t0);
        s_TimingsO.push_back(static_cast<int>(dt_us.count()));
    }

    // Draw Same Mesh, Same Range
    if(s_Step == 1)
    {
        auto t0 = high_resolution_clock::now();
        glVertexAttribPointer(vPosition, 2, GL_FLOAT, GL_FALSE, 0, verts);
        for (int i = 0; i < k_Instances; ++i)
            glDrawArrays(GL_TRIANGLES, 0, 3);
        auto t1 = high_resolution_clock::now();
        auto dt_us = duration_cast<microseconds>(t1 - t0);
        s_TimingsSMSR.push_back(static_cast<int>(dt_us.count()));
    }
    
    // Draw Same Mesh, Different Ranges
    if(s_Step == 2)
    {
        auto t0 = high_resolution_clock::now();
        glVertexAttribPointer(vPosition, 2, GL_FLOAT, GL_FALSE, 0, verts);
        for (int i = 0; i < k_Instances; ++i)
            glDrawArrays(GL_TRIANGLES, i * 3, 3);
        auto t1 = high_resolution_clock::now();
        auto dt_us = duration_cast<microseconds>(t1 - t0);
        s_TimingsSMDR.push_back(static_cast<int>(dt_us.count()));
    }

    // Draw Different Meshes
    if(s_Step == 3)
    {
        auto t0 = high_resolution_clock::now();
        for (int i = 0; i < k_Instances; ++i) {
            glVertexAttribPointer(vPosition, 2, GL_FLOAT, GL_FALSE, 0, &verts[i * 6]);
            glDrawArrays(GL_TRIANGLES, 0, 3);
        }
        auto t1 = high_resolution_clock::now();
        auto dt_us = duration_cast<microseconds>(t1 - t0);
        s_TimingsDM.push_back(static_cast<int>(dt_us.count()));
    }
    
    // Draw Same Mesh, Same Range, Scissors Change
    if(s_Step == 4)
    {
        auto t0 = high_resolution_clock::now();
        glVertexAttribPointer(vPosition, 2, GL_FLOAT, GL_FALSE, 0, verts);
        for (int i = 0; i < k_Instances; ++i)
        {
            glScissor(0, 0, 10+i, 10+i);
            glDrawArrays(GL_TRIANGLES, 0, 3);
        }
        auto t1 = high_resolution_clock::now();
        auto dt_us = duration_cast<microseconds>(t1 - t0);
        s_TimingsSMSRScissors.push_back(static_cast<int>(dt_us.count()));
    }

    // Draw Same Mesh, Same Range, Color Change
    if(s_Step == 5)
    {
        auto t0 = high_resolution_clock::now();
        glVertexAttribPointer(vPosition, 2, GL_FLOAT, GL_FALSE, 0, verts);
        for (int i = 0; i < k_Instances; ++i)
        {
            glUniform4f(fColor, 1, ((float)i) / k_Instances, 0, 1);
            glDrawArrays(GL_TRIANGLES, 0, 3);
        }
        auto t1 = high_resolution_clock::now();
        auto dt_us = duration_cast<microseconds>(t1 - t0);
        s_TimingsSMSRColor.push_back(static_cast<int>(dt_us.count()));
    }

    // Draw Same Mesh, Same Range, Depth Test Change
    if(s_Step == 6)
    {
        GLint depthBits;
        //glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_DEPTH_BITS, &depthBits);
        glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_DEPTH_SIZE, &depthBits);
        if(depthBits < 8 || depthBits > 32)
            LOGI("ERROR DEPTH BITS");

        auto t0 = high_resolution_clock::now();
        glVertexAttribPointer(vPosition, 2, GL_FLOAT, GL_FALSE, 0, verts);
        bool whatever = false;
        for (int i = 0; i < k_Instances; ++i)
        {
            glDepthFunc(whatever ? GL_EQUAL : GL_NOTEQUAL);
            glDrawArrays(GL_TRIANGLES, 0, 3);
            whatever = !whatever;
        }
        auto t1 = high_resolution_clock::now();
        auto dt_us = duration_cast<microseconds>(t1 - t0);
        s_TimingsSMSRDepth.push_back(static_cast<int>(dt_us.count()));
    }

    // Flush Statistics
    if(s_Step == 7 && ++s_StatCount == k_MaxStatCount)
    {
        sort(s_TimingsO.begin(), s_TimingsO.end());
        sort(s_TimingsSMSR.begin(), s_TimingsSMSR.end());
        sort(s_TimingsSMDR.begin(), s_TimingsSMDR.end());
        sort(s_TimingsDM.begin(), s_TimingsDM.end());
        sort(s_TimingsSMSRScissors.begin(), s_TimingsSMSRScissors.end());
        sort(s_TimingsSMSRColor.begin(), s_TimingsSMSRColor.end());
        sort(s_TimingsSMSRDepth.begin(), s_TimingsSMSRDepth.end());

        int medianIndex = s_StatCount >> 1;
        LOGI("O = %d | SMSR = %d | SMDR = %d | DM = %d | SMSRScissors = %d | SMSRColor = %d | SMSRDepth = %d",
            s_TimingsO[medianIndex],
            s_TimingsSMSR[medianIndex],
            s_TimingsSMDR[medianIndex],
            s_TimingsDM[medianIndex],
            s_TimingsSMSRScissors[medianIndex],
            s_TimingsSMSRColor[medianIndex],
            s_TimingsSMSRDepth[medianIndex]);

        s_TimingsO.clear();
        s_TimingsSMSR.clear();
        s_TimingsSMDR.clear();
        s_TimingsDM.clear();
        s_TimingsSMSRScissors.clear();
        s_TimingsSMSRColor.clear();
        s_TimingsSMSRDepth.clear();
        s_StatCount = 0;
    }
    
    if(++s_Step == 8)
        s_Step = 0;
}
/* [renderFrame] */

extern "C"
{
    JNIEXPORT void JNICALL Java_com_arm_malideveloper_openglessdk_simpletriangle_NativeLibrary_init(
                           JNIEnv * env, jobject obj, jint width, jint height);
    JNIEXPORT void JNICALL Java_com_arm_malideveloper_openglessdk_simpletriangle_NativeLibrary_step(
                           JNIEnv * env, jobject obj);
};

/* [Native functions] */
JNIEXPORT void JNICALL Java_com_arm_malideveloper_openglessdk_simpletriangle_NativeLibrary_init(
                       JNIEnv * env, jobject obj, jint width, jint height)
{
    setupGraphics(width, height);
}

JNIEXPORT void JNICALL Java_com_arm_malideveloper_openglessdk_simpletriangle_NativeLibrary_step(
                       JNIEnv * env, jobject obj)
{
    renderFrame();
}
/* [Native functions] */
