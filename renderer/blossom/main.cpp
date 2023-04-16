// system
#include <cstdio>
#include <cstdint>
#include <Windows.h>

// gl
#include <gl/GL.h>
#include "glext.h"
#include "gldefs.h"

// config
#include "config.h"

// shaders
#include "frag_draw.h"
#undef VAR_IRESOLUTION
#undef VAR_FRAGCOLOR
#include "frag_present.h"

// requirements for capture mode
#if CAPTURE || LIVE_RELOAD
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include <ctime>
#endif

#if LIVE_RELOAD
#include <cassert>
#include <algorithm>
#endif

// shaders
GLuint gShaderDraw;
GLuint gShaderPresent;

// framebuffers
struct {
    GLuint fb[2];
    int drawingTo = 0;
} gPingPong;

// uniform bindings
int const kUniformResolution = 0;
int const kUniformFrame = 1;
int const kUniformMode = 2;
int const kUniformPhase = 3;
int const kSamplerAccumulatorTex = 0;

// === resolutions ===
#if WINDOW_AUTO_SIZE
// the ugliest comma operator hack I will ever write
int const kCanvasWidth = (SetProcessDPIAware(), GetSystemMetrics(SM_CXSCREEN));
int const kCanvasHeight = GetSystemMetrics(SM_CYSCREEN);
#else
#define kCanvasWidth CANVAS_WIDTH
#define kCanvasHeight CANVAS_HEIGHT
#endif

#define kWindowWidth kCanvasWidth
#define kWindowHeight kCanvasHeight
// =====================

// cellular automaton configuration
int const kItersPhase1 = 32;


// capture GL errors
#if _DEBUG
void __stdcall
MessageCallback(GLenum source,
	GLenum type,
	GLuint id,
	GLenum severity,
	GLsizei length,
	const GLchar* message,
	const void* userParam)
{
    const char* pixelpath = "Pixel-path performance warning: Pixel transfer is synchronized with 3D rendering.";
    if (strcmp(message, pixelpath) == 0) {
        return;
    }
	fprintf(stderr, "GL CALLBACK: %s type = 0x%x, severity = 0x%x, message = %s\n",
		(type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : ""),
		type, severity, message);

    if (type != GL_DEBUG_TYPE_PERFORMANCE) {
        __debugbreak();
    }
}
#endif

static GLuint makeFramebuffer()
{
	GLuint name, backing;
	glGenFramebuffers(1, &name);
	glBindFramebuffer(GL_FRAMEBUFFER, name);
	glGenTextures(1, &backing);
	glBindTexture(GL_TEXTURE_2D, backing);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, kWindowWidth, kWindowHeight, 0, GL_RGBA, GL_FLOAT, 0);

	// don't remove these!
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT); // mirrored repeat worked best for cellular automata
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);

	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, backing, 0);
	GLenum drawBuffers[] = { GL_COLOR_ATTACHMENT0 };
	glDrawBuffers(1, drawBuffers);
    return name;
}

GLuint makeShader(const char* source)
{
#if _DEBUG
	GLuint shader = glCreateShader(GL_FRAGMENT_SHADER);

	glShaderSource(shader, 1, &source, 0);
	glCompileShader(shader);

	// shader compiler errors
	GLint isCompiled = 0;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &isCompiled);
	if (isCompiled == GL_FALSE)
	{
		const int maxLength = 1024;
		GLchar errorLog[maxLength];
		glGetShaderInfoLog(shader, maxLength, 0, errorLog);
		puts(errorLog);
		glDeleteShader(shader);
		__debugbreak();
	}

	// link shader
	GLuint m_program = glCreateProgram();
	glAttachShader(m_program, shader);
	glLinkProgram(m_program);

	GLint isLinked = 0;
	glGetProgramiv(m_program, GL_LINK_STATUS, &isLinked);
	if (isLinked == GL_FALSE)
	{
		const int maxLength = 1024;
		GLchar errorLog[maxLength];
		glGetProgramInfoLog(m_program, maxLength, 0, errorLog);
		puts(errorLog);
		glDeleteProgram(m_program);
		__debugbreak();
	}
	
	return m_program;
#else
	return glCreateShaderProgramv(GL_FRAGMENT_SHADER, 1, &source);
#endif

}

void bindSharedUniforms()
{
	glUniform4f(
		kUniformResolution,
		(float)kCanvasWidth,
		(float)kCanvasHeight,
		(float)kCanvasWidth / (float)kCanvasHeight,
		(float)kCanvasHeight / (float)kCanvasWidth);
}

void accumulatorSetup()
{
	glUseProgram(gShaderDraw);
	glBindFramebuffer(GL_FRAMEBUFFER, gPingPong.fb[gPingPong.drawingTo]);

	bindSharedUniforms();
	glBindTexture(GL_TEXTURE_2D, gPingPong.fb[1 - gPingPong.drawingTo]); // It's OK to bind FBO instead of its Texture here (!)
}

enum {
    MODE_PRESENT = 0,
    MODE_SCALE = 1,
};

static inline void presentSetup(int destFb, int sourceFb, int mode, int frame)
{
	glUseProgram(gShaderPresent);
	glBindFramebuffer(GL_FRAMEBUFFER, destFb);

	bindSharedUniforms();
    glUniform1i(kUniformFrame, frame);
    glUniform1i(kUniformMode, mode);
	glBindTexture(GL_TEXTURE_2D, sourceFb);
}

static inline void accumulatorRender(int sampleCount)
{
	glUniform1i(kUniformFrame, sampleCount);
	glUniform1i(kUniformPhase, sampleCount >= kItersPhase1);
	glRecti(-1, -1, 1, 1);

#ifndef RENDER_EXACT_SAMPLES
	// deliberately block so we don't queue up more work than we have time for
	glFinish();
#endif
}

static inline void presentRender(HDC hDC)
{
	glRecti(-1, -1, 1, 1);
	SwapBuffers(hDC);
}

static inline void clearFramebuffer()
{
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
}


#if LIVE_RELOAD
#define _CRT_SECURE_NO_DEPRECATE
#include <cstdio>

static FILE* tryfopen(const char* path, const char* mode) {
    FILE* fp = NULL;

    int attempts = 10; // sometimes file is still locked during text editor writing so we retry
    while (!fp && attempts--) {
        fp = fopen(path, mode);
        if (!fp) Sleep(1);
    }
    return fp;
}

const char* loadTempString(const char* path)
{
    static char* tempString;
    FILE* fp = tryfopen(path, "rb");

    if (!fp) {
        printf("Couldn't open path '%s'!\n", path);
        return nullptr;
    }
    fseek(fp, 0L, SEEK_END);
    long size = ftell(fp);
    free(tempString);
    tempString = (char*)calloc(size + 1, 1);
    fseek(fp, 0L, SEEK_SET);
    size_t numRead = fread(tempString, 1, size, fp);
    fclose(fp);

    if (numRead != size) {
        printf("Invalid size %zu\n", numRead);
        return nullptr;
    }
    tempString[size] = '\0';

    return tempString;
}

void makeShaderOrBail(const char* source, bool* ok, GLuint* outProgram)
{
    if (!*ok) {
        return; // early out if we already failed. it's literally a monad, bro
    }

	GLuint shader = glCreateShader(GL_FRAGMENT_SHADER);

	glShaderSource(shader, 1, &source, 0);
	glCompileShader(shader);

	// shader compiler errors
	GLint isCompiled = 0;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &isCompiled);
	if (isCompiled == GL_FALSE)
	{
		const int maxLength = 1024;
		GLchar errorLog[maxLength];
		glGetShaderInfoLog(shader, maxLength, 0, errorLog);
		puts(errorLog);
		glDeleteShader(shader);
        *ok = false;
        return;
	}

	// link shader
	GLuint program = glCreateProgram();
	glAttachShader(program, shader);
	glLinkProgram(program);

	GLint isLinked = 0;
	glGetProgramiv(program, GL_LINK_STATUS, &isLinked);
	if (isLinked == GL_FALSE)
	{
		const int maxLength = 1024;
		GLchar errorLog[maxLength];
		glGetProgramInfoLog(program, maxLength, 0, errorLog);
		puts(errorLog);
		glDeleteProgram(program);
        *ok = false;
        return;
	}

    if (outProgram) {
        glDeleteProgram(*outProgram);
    }

    *ok = true;
    *outProgram = program;
}

bool reloadShaders()
{
    bool ok = true;
    makeShaderOrBail(loadTempString("draw.frag"), &ok, &gShaderDraw);
    makeShaderOrBail(loadTempString("present.frag"), &ok, &gShaderPresent);

    return ok;
}
#endif

#if defined(RELEASE)
int WinMainCRTStartup()
#else
int main()
#endif
{
#ifndef RENDER_EXACT_SAMPLES
	unsigned int startTime = timeGetTime();
#endif

	DEVMODE screenSettings = {
		{0}, 0, 0, sizeof(screenSettings), 0, DM_PELSWIDTH | DM_PELSHEIGHT,
		{0}, 0, 0, 0, 0, 0, {0}, 0, 0, (DWORD)kWindowWidth, (DWORD)kWindowHeight, 0, 0,
		#if(WINVER >= 0x0400)
			0, 0, 0, 0, 0, 0,
			#if (WINVER >= 0x0500) || (_WIN32_WINNT >= 0x0400)
				0, 0
			#endif
		#endif
	};

	const PIXELFORMATDESCRIPTOR pfd = {
		sizeof(pfd), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, PFD_TYPE_RGBA,
		32, 0, 0, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 32, 0, 0, PFD_MAIN_PLANE, 0, 0, 0, 0
	};
	
	#if WINDOW_FULLSCREEN
		ChangeDisplaySettings(&screenSettings, CDS_FULLSCREEN);
		ShowCursor(0);
		HDC hDC = GetDC(CreateWindow((LPCSTR)0xC018, 0, WS_POPUP | WS_VISIBLE | WS_MAXIMIZE, 0, 0, 0, 0, 0, 0, 0, 0));
	#else
        #if _DEBUG
        int windowX = 200;
        int windowY = 10;
        HWND mainWindow = CreateWindow((LPCSTR)0xC018, 0, WS_POPUP | WS_VISIBLE, 200, 10, kWindowWidth, kWindowHeight, 0, 0, 0, 0);
		HDC hDC = GetDC(mainWindow);

        HWND consoleWindow = GetConsoleWindow();
        WINDOWINFO windowInfo;
        GetWindowInfo(consoleWindow, &windowInfo);
        int consoleW = windowInfo.rcWindow.right - windowInfo.rcWindow.left;
        int consoleH = windowInfo.rcWindow.bottom - windowInfo.rcWindow.top;
        consoleH = min(consoleH, 1440 - (windowY + kWindowHeight));
        int consoleY = min(windowY + kWindowHeight + 4, 1440 - consoleH + 4);

        SetWindowPos( consoleWindow, 0, windowX, consoleY, consoleW, consoleH, SWP_NOACTIVATE );	

        #else
		HDC hDC = GetDC(CreateWindow((LPCSTR)0xC018, 0, WS_POPUP | WS_VISIBLE, 0, 0, kWindowWidth, kWindowHeight, 0, 0, 0, 0));
        #endif
	#endif

	// set pixel format and make opengl context
	SetPixelFormat(hDC, ChoosePixelFormat(hDC, &pfd), &pfd);
	wglMakeCurrent(hDC, wglCreateContext(hDC));
	SwapBuffers(hDC);

	// enable opengl debug messages
#if _DEBUG
	glEnable(GL_DEBUG_OUTPUT);
	glDebugMessageCallback(MessageCallback, 0);
#endif

	// make framebuffer
	gPingPong.fb[0] = makeFramebuffer();
	gPingPong.fb[1] = makeFramebuffer();

	// optional extra buffers/textures
#if CAPTURE || LIVE_RELOAD
	GLuint const fbCapture = makeFramebuffer();
	float* tempFramebufferFloat = new float[kCanvasWidth * kCanvasHeight * 4];
	float* cpuFramebufferFloat = new float[kCanvasWidth * kCanvasHeight * 4];
	uint8_t* cpuFramebufferU8 = new uint8_t[kCanvasWidth * kCanvasHeight * 4];
	uint8_t* cpuFramebufferDepth = new uint8_t[kCanvasWidth * kCanvasHeight];
#endif

#if LIVE_RELOAD
    reloadShaders();
    restart:
#else
	// make shaders
	gShaderDraw = makeShader(draw_frag);
	gShaderPresent = makeShader(present_frag);
#endif

#if !DESPERATE
    clearFramebuffer();
#endif
    int sampleCount = 0;
	for (
		;
#ifdef RENDER_EXACT_SAMPLES
		sampleCount < RENDER_EXACT_SAMPLES;
#else
	#ifdef RENDER_MIN_SAMPLES
			(sampleCount < RENDER_MIN_SAMPLES) ||
	#endif
	#ifdef RENDER_MAX_SAMPLES
			(sampleCount < RENDER_MAX_SAMPLES) &&
	#endif
			(timeGetTime() < startTime + RENDER_MAX_TIME_MS);
#endif
		++sampleCount
	)
	{
#if _DEBUG
		//printf("accumulate sample %d, drawing to = %d\n", sampleCount, gPingPong.drawingTo);
#endif

		#if !DESPERATE
		PeekMessage(0, 0, 0, 0, PM_REMOVE);
		#endif

        #if LIVE_RELOAD
        if (sampleCount % 5 == 0) {
            if (reloadShaders()) {
                accumulatorSetup();
            }
        }

        if (GetAsyncKeyState(VK_RCONTROL)) {
            printf("Clearing\n");
            glBindFramebuffer(GL_FRAMEBUFFER, gPingPong.fb[0]);
            clearFramebuffer();
            glBindFramebuffer(GL_FRAMEBUFFER, gPingPong.fb[1]);
            clearFramebuffer();
            accumulatorSetup();
            sampleCount = 0;
        }

        //if (true && sampleCount == 32) {
        //    printf("HACK\n");
        //    sampleCount = 31;
        //    continue;
        //}
        #endif

        if (sampleCount == kItersPhase1) {
            #if _DEBUG
            printf("Upscaling at %d\n", sampleCount);
            #endif
            // Read from 'drawingTo', write to the other. So we read from last frame's output which is correct.
            presentSetup(
                gPingPong.fb[gPingPong.drawingTo],
                gPingPong.fb[1 - gPingPong.drawingTo],
                MODE_SCALE, sampleCount);
            glRecti(-1, -1, 1, 1);
            // // swap ping pong afterwards
            gPingPong.drawingTo = 1 - gPingPong.drawingTo;
        }

        #if LIVE_RELOAD
        // on the first frame render offline data first
        if (sampleCount == 0 && GetAsyncKeyState(VK_RSHIFT)) {
            puts("Saving first frame");
            const char* types[] = { "shaded", "raw" };

            for (int idx = 0; idx < 2; idx++) {
                accumulatorSetup();
                accumulatorRender(-2 + idx);

                glFinish();

                glBindFramebuffer(GL_READ_FRAMEBUFFER, gPingPong.fb[gPingPong.drawingTo]);
                glReadPixels(0, 0, kCanvasWidth, kCanvasHeight, GL_RGBA, GL_FLOAT, tempFramebufferFloat);

                for (int i = 0; i < kCanvasHeight; i++) {
                    memcpy(
                        &cpuFramebufferFloat[i * kCanvasWidth * 4],
                        &tempFramebufferFloat[(kCanvasHeight - i - 1) * kCanvasWidth * 4],
                        sizeof(float) * kCanvasWidth * 4);
                }

                for (int y = 0; y < kCanvasHeight; ++y) {
                    for (int x = 0; x < kCanvasWidth; ++x) {
                        for (int i = 0; i < 3; ++i) {
                            float fval = cpuFramebufferFloat[(y*kCanvasWidth + x) * 4 + i];
                            // bias and clip RGB the same way as present.frag does
                            if (i < 3) {
                                fval = fval + 0.5f;
                            }
                            if (i == 3) {
                                fval += (0.5f / 255.0f) * ((rand() % 3) - 1);
                            }
                            uint8_t const u8val = fval < 0 ? 0 : (fval > 1 ? 255 : (uint8_t)(fval * 255));
                            cpuFramebufferU8[(y*kCanvasWidth + x) * 3 + i] = u8val;
                        }
                    }
                }

                for (int y = 0; y < kCanvasHeight; ++y) {
                    for (int x = 0; x < kCanvasWidth; ++x) {
                        for (int i = 0; i < 4; ++i) {
                            float fval = cpuFramebufferFloat[(y*kCanvasWidth + x) * 4 + i];
                            if (i == 3) {
                                fval += (0.5f / 255.0f) * ((rand() % 3) - 1);
                                uint8_t const u8val = fval < 0 ? 0 : (fval > 1 ? 255 : (uint8_t)(fval * 255));
                                cpuFramebufferDepth[(y*kCanvasWidth + x) * 1 + i] = u8val;
                            }
                        }
                    }
                }

                int cropWidth = int(CANVAS_WIDTH/2);
                int cropHeight = int(CANVAS_HEIGHT/2);

                if (idx == 0) {
                    // Save lower left corner as a PNG
                    int strideBytes = kCanvasWidth * 3;
                    int ystart = kCanvasHeight - cropHeight;
                    stbi_write_png_compression_level = 1;
                    stbi_write_png("rendering.png", cropWidth, cropHeight, 3, cpuFramebufferU8 + strideBytes * ystart, strideBytes);
                    stbi_write_png("rendering_depth.png", cropWidth, cropHeight, 1, cpuFramebufferDepth + kCanvasWidth * ystart, kCanvasWidth);
                }

                {
                    char binname[256];
                    sprintf(binname, "%s.f32.%d.%d.data", types[idx], cropWidth, cropHeight);
                    FILE* fh = tryfopen(binname, "wb");
                    assert(fh);
                    int strideFloats = kCanvasWidth * 4;
                    int ystart = kCanvasHeight - cropHeight;
                    for (int y = 0; y < cropHeight; y++) {
                        fwrite(&cpuFramebufferFloat[(ystart+y)*strideFloats], 4, cropWidth * 4, fh);
                    }
                    fclose(fh);
                }

                // {
                //     TODO should write crop only
                //     char binname[256];
                //     sprintf(binname, "%s.u8.%d.%d.data", types[idx], cropWidth, cropHeight);
                //     FILE* fh = tryfopen(binname, "wb");
                //     assert(fh);
                //     //fwrite(cpuFramebufferU8, 1, cropHeight * cropWidth * 3, fh);
                //     fwrite(cpuFramebufferU8, 1, kCanvasHeight * kCanvasWidth * 3, fh);
                //     fclose(fh);
                // }
            }
        }
        #endif

        #if LIVE_RELOAD
        static bool actuallyAccumulate;
        // if (sampleCount == 0) actuallyAccumulate = true;
        accumulatorSetup();
		accumulatorRender(actuallyAccumulate ? sampleCount : -2);
        if (GetAsyncKeyState(VK_RETURN) && (GetForegroundWindow() == consoleWindow || GetForegroundWindow() == mainWindow)) {
            actuallyAccumulate = !actuallyAccumulate;
            printf("Accumulate: %d\n", actuallyAccumulate);
        }

        #else
        accumulatorSetup();
		accumulatorRender(sampleCount);
        #endif

		// To prevent accidentally hitting esc during long renders. Use Alt+F4 instead.
		#if !CAPTURE
        #if LIVE_RELOAD
		if (GetAsyncKeyState(VK_ESCAPE) && (GetForegroundWindow() == consoleWindow || GetForegroundWindow() == mainWindow))
			goto abort;
        #else
		if (GetAsyncKeyState(VK_ESCAPE))
			goto abort;
        #endif
		#endif


		#if RENDER_PROGRESSIVE
			#if CAPTURE
			if ((sampleCount&(sampleCount-1))==0)
            #elif LIVE_RELOAD
            if (!(GetAsyncKeyState(VK_SPACE) && GetForegroundWindow() == mainWindow))
			#endif
        
			{
				presentSetup(0,  gPingPong.fb[gPingPong.drawingTo], MODE_PRESENT, sampleCount);
				presentRender(hDC);
			}
        #else
				presentSetup(0,  gPingPong.fb[gPingPong.drawingTo], MODE_PRESENT, sampleCount);
	            glRecti(-1, -1, 1, 1);
		#endif


        gPingPong.drawingTo = 1 - gPingPong.drawingTo;
	}

    #if LIVE_RELOAD
    while (!(GetAsyncKeyState(VK_ESCAPE) && (GetForegroundWindow() == consoleWindow || GetForegroundWindow() == mainWindow)))
    #else
	while (!GetAsyncKeyState(VK_ESCAPE))
    #endif
	{
		PeekMessage(0, 0, 0, 0, PM_REMOVE);

        presentSetup(0,  gPingPong.fb[1 - gPingPong.drawingTo], MODE_PRESENT, sampleCount);
        presentRender(hDC);

        #if LIVE_RELOAD
        if (GetAsyncKeyState(VK_RCONTROL)) {
            goto restart;
        }
        #endif
	}

abort:
	ExitProcess(0);
	return 0;
}
