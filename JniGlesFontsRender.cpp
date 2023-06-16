#include <jni.h>
#include <android/log.h>
#include <list>
#include <mutex>
#include <condition_variable>
#include <unistd.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES/gl.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include "ft2build.h"
#include FT_FREETYPE_H
#include <android/native_window_jni.h>
#include <stdint.h>
#include <chrono>

#ifdef __cplusplus
extern "C"{
#endif

#define     FONTS_TEXT              "hello,yuhaoran"
#define     FONTS_WIDTH             50
#define     FONTS_HEIGHT            50
#define     FONTS_TTF               "/sdcard/DejaVuSans.ttf"
#define     FONTS_LAYER             15
#define     PICTURE_PATH            "/sdcard/720p.rgba"
#define     PICTURE_WIDTH           1280
#define     PICTURE_HEIGHT          720
#define     X_OFFSET                600
#define     Y_OFFSET                100

class NativeRender
{
public:
    void        Open(JNIEnv *env, jobject objView);
    void        Close();
private:
    void        InitNativeWindow(JNIEnv *env, jobject objView);
    void        ReleaseNativeWindow();
    void        InitFreeType();
    void        ReleaseFreeType();
    void        InitGLES();
    void        ReleaseGLES();

    void        CreateEGLWindow();
    void        DestroyEGLWindow();
    void        InstallGLESProgram();
    void        UninstallGLESProgram();
    void        CreateGLESTexture();
    void        DestroyGLESTexture();
    void        GLESDraw();
    void        EGLLock();
    void        EGLUnlock();
    void        GLESDrawFonts();
    GLuint      GLESLoadShader(GLenum type, const char *source);
    void        GLESDrawOneFont(int idx, int layer);
    void        GLESDrawPicture();
    uint32_t    GetTimeStamp();
private:
    ANativeWindow       *m_pWnd{ NULL };
    FT_Library          m_ftLib;
    int                 m_nFtFacesCounts{ 0 };
    FT_Face             *m_pFtFaces{ NULL };
    EGLDisplay              m_eglDisplay;
    EGLConfig               m_eglConfig;
    EGLContext              m_eglContext;
    EGLSurface              m_eglSurface;
    GLuint                  m_glProgram;
    GLuint                  m_aPositionHd;
    GLuint                  m_aTexCoordHd;
    GLuint                  m_textureFontsHd;
    GLuint                  m_drawTypeHd;
    GLuint                  m_textureFonts;
};

void NativeRender::Open(JNIEnv *env, jobject objView)
{
    InitNativeWindow(env, objView);
    InitFreeType();
    InitGLES();
    __android_log_print(ANDROID_LOG_INFO, "RENDER", "open success");
}

void NativeRender::Close()
{
    ReleaseGLES();
    ReleaseFreeType();
    ReleaseNativeWindow();
    __android_log_print(ANDROID_LOG_INFO, "RENDER", "close");
}

void NativeRender::InitNativeWindow(JNIEnv *env, jobject objView)
{
    jclass clsSurfaceView = env->FindClass("android/view/SurfaceView");
    jclass clsSurfaceHolder = env->FindClass("android/view/SurfaceHolder");
    jclass clsSurface = env->FindClass("android/view/Surface");
    jmethodID mthGetHolder = env->GetMethodID(clsSurfaceView, "getHolder", "()Landroid/view/SurfaceHolder;");
    jmethodID mthGetSurface = env->GetMethodID(clsSurfaceHolder, "getSurface", "()Landroid/view/Surface;");
    jobject objHolder = env->CallObjectMethod(objView, mthGetHolder);
    jobject objSurface = env->CallObjectMethod(objHolder, mthGetSurface);
    m_pWnd = ANativeWindow_fromSurface(env, objSurface);
    env->DeleteLocalRef(objHolder);
    env->DeleteLocalRef(objSurface);
    env->DeleteLocalRef(clsSurfaceView);
    env->DeleteLocalRef(clsSurfaceHolder);
    env->DeleteLocalRef(clsSurface);
}

void NativeRender::ReleaseNativeWindow()
{
    ANativeWindow_release(m_pWnd);
}

void NativeRender::InitFreeType()
{
    FT_Init_FreeType(&m_ftLib);
    const char *pText = FONTS_TEXT;
    m_nFtFacesCounts = strlen(pText);
    m_pFtFaces = (FT_Face*)malloc(sizeof(FT_Face) * m_nFtFacesCounts);
    for(int i=0; i<m_nFtFacesCounts; i++){
        FT_New_Face(m_ftLib, FONTS_TTF, 0, &m_pFtFaces[i]);
        FT_UInt glyph_idx = FT_Get_Char_Index(m_pFtFaces[i], pText[i]);
        FT_Set_Pixel_Sizes(m_pFtFaces[i], FONTS_WIDTH, FONTS_HEIGHT);
        FT_Load_Glyph(m_pFtFaces[i], glyph_idx, FT_RENDER_MODE_NORMAL);
        FT_Render_Glyph(m_pFtFaces[i]->glyph, FT_RENDER_MODE_NORMAL);
    }
}

void NativeRender::ReleaseFreeType()
{
    for(int i=0; i<m_nFtFacesCounts; i++){
        FT_Done_Face(m_pFtFaces[i]);
    }
    free(m_pFtFaces);
    FT_Done_FreeType(m_ftLib);
}

void NativeRender::InitGLES()
{
    CreateEGLWindow();
    EGLLock();
    InstallGLESProgram();
    CreateGLESTexture();
    GLESDraw();
    EGLUnlock();
}

void NativeRender::ReleaseGLES()
{
    EGLLock();
    DestroyGLESTexture();
    UninstallGLESProgram();
    EGLUnlock();
    DestroyEGLWindow();
}

void NativeRender::CreateEGLWindow()
{
    EGLint configSpec[] = {
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_NONE
    };

    m_eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    EGLint eglMajVers, eglMinVers;
    EGLint numConfigs;
    eglInitialize(m_eglDisplay, &eglMajVers, &eglMinVers);
    eglChooseConfig(m_eglDisplay, configSpec, &m_eglConfig, 1, &numConfigs);
    const EGLint ctxAttr[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };
    m_eglContext = eglCreateContext(m_eglDisplay, m_eglConfig, EGL_NO_CONTEXT, ctxAttr);
    m_eglSurface = eglCreateWindowSurface(m_eglDisplay, m_eglConfig, (EGLNativeWindowType)m_pWnd, NULL);
}

void NativeRender::DestroyEGLWindow()
{
    eglDestroySurface(m_eglDisplay, m_eglSurface);
    eglDestroyContext(m_eglDisplay, m_eglContext);
    eglTerminate(m_eglDisplay);
}

void NativeRender::InstallGLESProgram()
{
    const char *vShaderSource = 
        "attribute vec4 aPosition;      \n"
        "attribute vec2 aTexCoord;      \n"
        "varying vec2 vTexCoord;        \n"
        "void main(){                   \n"
        "  vTexCoord = aTexCoord;       \n"
        "  gl_Position = aPosition;     \n"
        "}                              \n";

    const char *fShaderSource = 
        "precision mediump float;                                   \n"
        "varying vec2 vTexCoord;                                    \n"
        "uniform sampler2D textureFonts;                            \n"
        "uniform int drawType;                                      \n"
        "void main(){                                               \n"
        "  vec4 color;                                              \n"
        "  if(drawType == 1){                                       \n"
        "       float alpha = texture2D(textureFonts, vTexCoord).r; \n"
        "       color = vec4(0.0, 1.0, 0.0, alpha);                 \n"
        "  }else{                                                   \n"
        "       color = texture2D(textureFonts, vTexCoord);         \n"
        "  }                                                        \n"
        "  gl_FragColor = color;                                    \n"
        "}                                                          \n";
    GLuint vShader = GLESLoadShader(GL_VERTEX_SHADER, vShaderSource);
    GLuint fShader = GLESLoadShader(GL_FRAGMENT_SHADER, fShaderSource);
    m_glProgram = glCreateProgram();
    glAttachShader(m_glProgram, vShader);
    glAttachShader(m_glProgram, fShader);
    glLinkProgram(m_glProgram);
    m_aPositionHd = (GLuint)glGetAttribLocation(m_glProgram, "aPosition");
    m_aTexCoordHd = (GLuint)glGetAttribLocation(m_glProgram, "aTexCoord");
    m_textureFontsHd = (GLuint) glGetUniformLocation(m_glProgram, "textureFonts");
    m_drawTypeHd = (GLuint)glGetUniformLocation(m_glProgram, "drawType");
}

void NativeRender::UninstallGLESProgram()
{
    glDeleteProgram(m_glProgram);
}

void NativeRender::CreateGLESTexture()
{
    glGenTextures(1, &m_textureFonts);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_textureFonts);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void NativeRender::DestroyGLESTexture()
{
    glDeleteTextures(1, &m_textureFonts);
}

void NativeRender::GLESDraw()
{
    glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
    glUseProgram(m_glProgram);
    float vVecs[12] = {
        -1.0f, 1.0f, 0.0f,
        -1.0f, -1.0f, 0.0f,
        1.0f, -1.0f, 0.0f,
        1.0f, 1.0f, 0.0f
    };
    glEnableVertexAttribArray(m_aPositionHd);
    glVertexAttribPointer(m_aPositionHd, 3, GL_FLOAT, GL_FALSE, 12, vVecs);
    float fVecs[8] = {
        0.0f, 0.0f,
        0.0f, 1.0f,
        1.0f, 1.0f,
        1.0f, 0.0f
    };
    glEnableVertexAttribArray(m_aTexCoordHd);
    glVertexAttribPointer(m_aTexCoordHd, 2, GL_FLOAT, GL_FALSE, 8, fVecs);
    GLESDrawPicture();
    GLESDrawFonts();
    glDisableVertexAttribArray(m_aPositionHd);
    glDisableVertexAttribArray(m_aTexCoordHd);
    eglSwapBuffers(m_eglDisplay, m_eglSurface);
}

void NativeRender::GLESDrawFonts()
{
    uint32_t nStart = GetTimeStamp();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glPixelStorei(GL_UNPACK_ALIGNMENT,1);
    glUniform1i(m_drawTypeHd, 1);
    for(int j=0; j<FONTS_LAYER; j++){
        for(int i=0; i<m_nFtFacesCounts; i++){
            GLESDrawOneFont(i, j);
        }
    }

    uint32_t nStop = GetTimeStamp();
    __android_log_print(ANDROID_LOG_INFO, "RENDER", "draw fonts duration: %d", nStop - nStart);
}

void NativeRender::EGLLock()
{
    eglMakeCurrent(m_eglDisplay, m_eglSurface, m_eglSurface, m_eglContext);
}

void NativeRender::EGLUnlock()
{
    eglMakeCurrent(m_eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
}

GLuint NativeRender::GLESLoadShader(GLenum type, const char *source)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    return shader;
}

void NativeRender::GLESDrawOneFont(int idx, int layer)
{
    glViewport(idx * FONTS_WIDTH + X_OFFSET, Y_OFFSET + layer * FONTS_HEIGHT, m_pFtFaces[idx]->glyph->bitmap.width, m_pFtFaces[idx]->glyph->bitmap.rows);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_textureFonts);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, m_pFtFaces[idx]->glyph->bitmap.width, m_pFtFaces[idx]->glyph->bitmap.rows,
                    0, GL_LUMINANCE, GL_UNSIGNED_BYTE, m_pFtFaces[idx]->glyph->bitmap.buffer);
    glUniform1i(m_textureFontsHd, 0);
    short iVecs[6] = {0, 1, 2, 0, 2, 3};
    glDrawElements(GL_TRIANGLE_STRIP, 6, GL_UNSIGNED_SHORT, iVecs);
}

void NativeRender::GLESDrawPicture()
{
    uint32_t nStart = GetTimeStamp();

    int nPicSize = PICTURE_WIDTH * PICTURE_HEIGHT * 4;
    uint8_t *pPicBuf = (uint8_t*)malloc(nPicSize);
    FILE *fp = fopen(PICTURE_PATH, "rb");
    fread(pPicBuf, 1, nPicSize, fp);
    fclose(fp);

    glUniform1i(m_drawTypeHd, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_textureFonts);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, PICTURE_WIDTH, PICTURE_HEIGHT,
                    0, GL_RGBA, GL_UNSIGNED_BYTE, pPicBuf);
    glUniform1i(m_textureFontsHd, 0);
    short iVecs[6] = {0, 1, 2, 0, 2, 3};
    glDrawElements(GL_TRIANGLE_STRIP, 6, GL_UNSIGNED_SHORT, iVecs);

    free(pPicBuf);

    uint32_t nStop = GetTimeStamp();
    __android_log_print(ANDROID_LOG_INFO, "RENDER", "draw picture duration: %d", nStop - nStart);
}

uint32_t NativeRender::GetTimeStamp()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>( std::chrono::system_clock::now().time_since_epoch()).count();
}

JNIEXPORT jlong JNICALL
Java_com_example_testjniglesfontsrender_JniGlesFontsRender_Open(JNIEnv* env, jobject instance, jobject objView)
{
    NativeRender *pRender = new NativeRender;
    pRender->Open(env, objView);
    return (jlong)pRender;
}

JNIEXPORT jint JNICALL
Java_com_example_testjniglesfontsrender_JniGlesFontsRender_Close(JNIEnv* env, jobject instance, jlong nHdRender)
{
    NativeRender *pRender = (NativeRender*)nHdRender;
    pRender->Close();
    delete pRender;
    return 0;
}

#ifdef __cplusplus
}
#endif