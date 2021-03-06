#include <iostream>
#include <dlfcn.h>
#include <stdarg.h>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#include <atomic>
#include <sstream>
#include <codecvt>
#include <locale>
#include <dirent.h>
#include <fstream>
#ifndef USE_GLFW
#include <X11/Xlib.h>
#endif
#include <functional>
#include <sys/mman.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "../common/symbols/android_symbols.h"
#include "../common/symbols/egl_symbols.h"
#include "../common/symbols/fmod_symbols.h"
#include "../common/symbols/libm_symbols.h"
#include "../minecraft/symbols.h"
#include "../minecraft/gl.h"
#include "../minecraft/AppPlatform.h"
#include "../minecraft/MinecraftGame.h"
#include "../minecraft/Mouse.h"
#include "../minecraft/Keyboard.h"
#include "../minecraft/Options.h"
#include "../minecraft/Common.h"
#include "../minecraft/Xbox.h"
#include "../minecraft/MultiplayerService.h"
#include "appplatform.h"
#include "store.h"
#include "fake_jni.h"
#ifndef __APPLE__
#include "gamepad.h"
#include "../common/openssl_multithread.h"
#endif
#include "../common/common.h"
#include "../common/hook.h"
#include "../common/modloader.h"
#include "xboxlive.h"
#include "../common/extract.h"
#ifndef DISABLE_CEF
#include "../ui/browser/browser.h"
#include "../ui/browser/xbox_login_browser.h"
#include "../ui/browser/initial_setup_browser.h"
#include "../ui/browser/gamepad_mapper_browser.h"
#include "../common/path_helper.h"
#endif
#ifndef DISABLE_PLAYAPI
#include "../ui/browser/google_login_browser.h"
#endif
#ifdef USE_EGLUT
#include <EGL/egl.h>
#include <GLES2/gl2.h>
extern "C" {
#include <eglut.h>
}
#include "../ui/game_window/window_eglut.h"
#endif
#ifdef USE_GLFW
#include <GLFW/glfw3.h>
#include "../ui/game_window/window_glfw.h"
#endif

extern "C" {

#include <hybris/dlfcn.h>
#include <hybris/hook.h>
#include "../../libs/hybris/src/jb/linker.h"

}

void androidStub() {
    Log::warn("Launcher", "Android function stub call");
}

void eglStub() {
    Log::warn("Launcher", "EGL function stub call");
}

std::unique_ptr<LinuxStore> createStoreHookFunc(const mcpe::string& idk, StoreListener& listener) {
    Log::trace("Launcher", "Creating fake store (%s)", idk.c_str());
    return std::unique_ptr<LinuxStore>(new LinuxStore());
}

class HTTPRequest;

class LinuxHttpRequestInternal {
public:
    void* vtable;
    int filler1;
    HTTPRequest* request;

    void destroy() {
        Log::trace("Launcher", "LinuxHttpRequestInternal::~LinuxHttpRequestInternal");
    }
};
void** linuxHttpRequestInternalVtable;

void constructLinuxHttpRequestInternal(LinuxHttpRequestInternal* requestInternal, HTTPRequest* request) {
    requestInternal->vtable = linuxHttpRequestInternalVtable;
    requestInternal->request = request;
}

void sendLinuxHttpRequestInternal(LinuxHttpRequestInternal* requestInternal) {
    Log::trace("Launcher", "HTTPRequestInternalAndroid::send stub called");
    // TODO: Implement it
}

void abortLinuxHttpRequestInternal(LinuxHttpRequestInternal* requestInternal) {
    Log::trace("Launcher", "HTTPRequestInternalAndroid::abort stub called");
    // TODO: Implement it
}


static MinecraftGame* client;
static LinuxAppPlatform* platform;

bool verifyCertChainStub() {
    Log::trace("Launcher", "verify_cert_chain_platform_specific stub called");
    return true;
}
mcpe::string xboxReadConfigFile(void* th) {
    Log::trace("Launcher", "Reading xbox config file");
    std::ifstream f(PathHelper::findDataFile("assets/xboxservices.config"));
    std::stringstream s;
    s << f.rdbuf();
    return s.str();
}
xbox::services::xbox_live_result<void> xboxLogTelemetrySignin(void* th, bool b, mcpe::string const& s) {
    Log::trace("Launcher", "log_telemetry_signin %i %s", (int) b, s.c_str());
    xbox::services::xbox_live_result<void> ret;
    ret.code = 0;
    ret.error_code_category = xbox::services::xbox_services_error_code_category();
    ret.message = " ";
    return ret;
}
mcpe::string xboxGetLocalStoragePath() {
    return PathHelper::getPrimaryDataDirectory();
}
xbox::services::xbox_live_result<void> xboxInitSignInActivity(xbox::services::system::user_auth_android* th,
                                                              int requestCode) {
    Log::trace("Launcher", "init_sign_in_activity %i", requestCode);
    xbox::services::xbox_live_result<void> ret;
    ret.code = 0;
    ret.error_code_category = xbox::services::xbox_services_error_code_category();

    auto local_conf = xbox::services::local_config::get_local_config_singleton();
    th->cid = local_conf->get_value_from_local_storage("cid").std();

    if (requestCode == 1) { // silent signin
        auto account = XboxLiveHelper::getMSAStorageManager()->getAccount();
        xbox::services::system::java_rps_ticket ticket;
        if (account) {
            std::unordered_map<MSASecurityScope, MSATokenResponse> tokens;
            try {
                tokens = account->requestTokens({{"user.auth.xboxlive.com", "mbi_ssl"}});
            } catch (std::exception& e) {
                std::cerr << "Xbox Live sign in failed: " << e.what() << "\n";
                ticket.error_code = 0x800704CF;
                ret.error_code_category = xbox::services::xbox_services_error_code_category();
                xbox::services::system::user_auth_android::s_rpsTicketCompletionEvent->set(ticket);
                return ret;
            }
            auto xboxLiveToken = tokens[{"user.auth.xboxlive.com"}];
            if (!xboxLiveToken.hasError()) {
                ticket.token = std::static_pointer_cast<MSACompactToken>(xboxLiveToken.getToken())->getBinaryToken();
                ticket.error_code = 0;
                ticket.error_text = "Got ticket";
                xbox::services::system::user_auth_android::s_rpsTicketCompletionEvent->set(ticket);
                return ret;
            }
        }
        ticket.error_code = 1;
        ticket.error_text = "Must show UI to acquire an account.";
        xbox::services::system::user_auth_android::s_rpsTicketCompletionEvent->set(ticket);
    } else if (requestCode == 6) { // sign out
        XboxLiveHelper::getMSAStorageManager()->setAccount(std::shared_ptr<MSAAccount>());

        xbox::services::xbox_live_result<void> arg;
        arg.code = 0;
        arg.error_code_category = xbox::services::xbox_services_error_code_category();
        xbox::services::system::user_auth_android::s_signOutCompleteEvent->set(arg);
    }

    return ret;
}
void xboxInvokeAuthFlow(xbox::services::system::user_auth_android* ret) {
    Log::trace("Launcher", "invoke_auth_flow");

#ifdef DISABLE_CEF
    std::cerr << "This build does not support Xbox Live login.\n";
    std::cerr << "To log in please build the launcher with CEF support.\n";
    ret->auth_flow_result.code = 2;
    ret->auth_flow_event.set(ret->auth_flow_result);
#else
    XboxLiveHelper::openLoginBrowser(ret);
#endif
}
std::vector<mcpe::string> xblGetLocaleList() {
    std::vector<mcpe::string> ret;
    ret.push_back("en-US");
    return ret;
}
void xblRegisterNatives() {
    Log::trace("Launcher", "register_natives stub");
}
xbox::services::xbox_live_result<void> xblLogCLL(void* th, mcpe::string const& a, mcpe::string const& b, mcpe::string const& c) {
    Log::trace("Launcher", "log_cll %s %s %s", a.c_str(), b.c_str(), c.c_str());
    XboxLiveHelper::getCLL()->addEvent(a.std(), b.std(), c.std());
    xbox::services::xbox_live_result<void> ret;
    ret.code = 0;
    ret.error_code_category = xbox::services::xbox_services_error_code_category();
    ret.message = " ";
    return ret;
}

static void (*glGenVertexArrays)(GLsizei n, GLuint *arrays);
static void (*glBindVertexArray)(GLuint array);

static void (*reflectShaderUniformsOriginal)(void*);
void reflectShaderUniformsHook(void* th) {
    GLuint vertexArr;
    glGenVertexArrays(1, &vertexArr);
    glBindVertexArray(vertexArr);
    *((GLuint*) ((unsigned int) th + 0xA0)) = vertexArr;
    reflectShaderUniformsOriginal(th);
}
static void (*bindVertexArrayOriginal)(void*, void*, void*);
void bindVertexArrayHook(void* th, void* a, void* b) {
    unsigned int vertexArr = *((GLuint*) ((unsigned int) th + 0xA0));
    glBindVertexArray(vertexArr);
    bindVertexArrayOriginal(th, a, b);
}

bool supportsImmediateModeHook() {
    return false;
}

#ifndef USE_GLFW
static int XErrorHandlerImpl(Display* display, XErrorEvent* event) {
    std::cerr << "X error received: "
              << "type " << event->type << ", "
              << "serial " << event->serial << ", "
              << "error_code " << static_cast<int>(event->error_code) << ", "
              << "request_code " << static_cast<int>(event->request_code) << ", "
              << "minor_code " << static_cast<int>(event->minor_code);
    return 0;
}

static int XIOErrorHandlerImpl(Display* display) {
    return 0;
}
#endif


static void setScissorRect(void* a, int x, int y, unsigned int w, unsigned int h) {
    glScissor(x, y, w, h);
}

extern "C"
void pshufb(char* dest, char* src) {
    char new_dest[16];
    for (int i = 0; i < 16; i++)
        new_dest[i] = (src[i] & 0x80) ? 0 : dest[src[i] & 15];
    memcpy(dest, new_dest, 16);
}
extern "C"
void pshufb_xmm4_xmm0();

void destroyXsapiSingleton(void* handle) {
    unsigned int off = (unsigned int) hybris_dlsym(handle, "_ZN4xbox8services19get_xsapi_singletonEb");
    unsigned int ebx = off + 0xb;
    ebx += *((unsigned int*) (off + 0xc + 2));
    unsigned int ptr = ebx + *((unsigned int*) (off + (0x661 - 0x4F0) + 2));
    ((std::shared_ptr<xbox::services::xsapi_singleton>*) ptr)->reset();
}

using namespace std;
int main(int argc, char *argv[]) {
    if (argc == 3 && strcmp(argv[1], "extract") == 0) {
        ExtractHelper::extractApk(argv[2]);
        return 0;
    }

#ifndef USE_GLFW
    XSetErrorHandler(XErrorHandlerImpl);
    XSetIOErrorHandler(XIOErrorHandlerImpl);
#endif

#ifndef __APPLE__
    OpenSSLMultithreadHelper::init();
#endif

#ifndef DISABLE_CEF
    BrowserApp::RegisterRenderProcessHandler<InitialSetupRenderHandler>();
    BrowserApp::RegisterRenderProcessHandler<XboxLoginRenderHandler>();
#ifdef GAMEPAD_SUPPORT
    BrowserApp::RegisterRenderProcessHandler<GamepadMapperRenderHandler>();
#endif
#ifndef DISABLE_PLAYAPI
    BrowserApp::RegisterRenderProcessHandler<GoogleLoginRenderHandler>();
#endif
    CefMainArgs cefArgs(argc, argv);
    int exit_code = CefExecuteProcess(cefArgs, BrowserApp::singleton.get(), NULL);
    if (exit_code >= 0)
        return exit_code;

#ifdef GAMEPAD_SUPPORT
    if (argc > 1 && strcmp(argv[1], "mapGamepad") == 0) {
        LinuxGamepadManager::instance.init();
        std::atomic<bool> shouldStop (false);
        std::thread t([&shouldStop] {
            while (!shouldStop) {
                LinuxGamepadManager::instance.pool();
                usleep(50000L);
            }
        });
        GamepadMapperBrowserClient::OpenBrowser();
        shouldStop = true;
        t.join();
        return 0;
    }
#endif
    {
        bool found = true;
        try {
            PathHelper::findDataFile("libs/libminecraftpe.so");
        } catch (std::exception& e) {
            found = false;
        }
        if (!found || (argc > 1 && strcmp(argv[1], "setup") == 0)) {
            if (!InitialSetupBrowserClient::OpenBrowser()) {
                BrowserApp::Shutdown();
                return 1;
            }
        }
    }
#endif

    bool enableStackTracePrinting = true;
    bool workaroundAMD = false;

    int windowWidth = 720;
    int windowHeight = 480;
    float pixelSize = 2.f;
#ifdef __APPLE__
    GraphicsApi graphicsApi = GraphicsApi::OPENGL;
#else
    GraphicsApi graphicsApi = GraphicsApi::OPENGL_ES2;
#endif

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--scale") == 0) {
            i++;
            pixelSize = std::stof(argv[i]);
        } else if (strcmp(argv[i], "-sw") == 0 || strcmp(argv[i], "--width") == 0) {
            i++;
            windowWidth = std::stoi(argv[i]);
        } else if (strcmp(argv[i], "-sh") == 0 || strcmp(argv[i], "--height") == 0) {
            i++;
            windowHeight = std::stoi(argv[i]);
        } else if (strcmp(argv[i], "-ns") == 0 || strcmp(argv[i], "--no-stacktrace") == 0) {
            enableStackTracePrinting = false;
        } else if (strcmp(argv[i], "--pocket-guis") == 0) {
            enablePocketGuis = true;
        } else if (strcmp(argv[i], "--amd-fix") == 0) {
            std::cout << "--amd-fix: Enabling AMD Workaround.\n";
            workaroundAMD = true;
#ifdef USE_GLFW
        } else if (strcmp(argv[i], "--gl-core") == 0) {
            std::cout << "--gl-core: Using OpenGL Core profile.\n";
            graphicsApi = GraphicsApi::OPENGL;
#endif
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            std::cout << "Help\n";
            std::cout << "--help               Shows this help information\n";
            std::cout << "--scale <scale>      Sets the pixel scale\n";
            std::cout << "--width <width>      Sets the window width\n";
            std::cout << "--height <height>    Sets the window height\n";
            std::cout << "--pocket-guis        Switches to Pocket Edition GUIs\n";
            std::cout << "--no-stacktrace      Disables stack trace printing\n";
            std::cout << "--splitscreen-fix    Fixes the splitscreen bug\n\n";
            std::cout << "--amd-workaround     Fixes crashes on pre-i686 and AMD CPUs\n\n";
#ifdef USE_GLFW
            std::cout << "--gl-core            Uses OpenGL 3.2+ core profile instead of GLES\n\n";
#endif
            std::cout << "EGL Options\n";
            std::cout << "-display <display>  Sets the display\n";
            std::cout << "-info               Shows info about the display\n\n";
            std::cout << "MCPE arguments:\n";
            std::cout << "edu <true|false>\n";
            std::cout << "mcworld <world>\n";
            return 0;
        }
    }

    if (enableStackTracePrinting) {
        registerCrashHandler();
    }

    setenv("LC_ALL", "C", 1); // HACK: Force set locale to one recognized by MCPE so that the outdated C++ standard library MCPE uses doesn't fail to find one

    Log::trace("Launcher", "Loading native libraries");
#ifdef __APPLE__
    void* fmodLib = loadLibraryOS(PathHelper::findDataFile("libs/native/libfmod.dylib"), fmod_symbols);
    void* libmLib = loadLibraryOS("libm.dylib", libm_symbols);
#else
    void* fmodLib = loadLibraryOS(PathHelper::findDataFile("libs/native/libfmod.so.9.6"), fmod_symbols);
    void* libmLib = loadLibraryOS("libm.so.6", libm_symbols);
#endif

    if (fmodLib == nullptr || libmLib == nullptr)
        return -1;
    Log::trace("Launcher", "Loading hybris libraries");
    stubSymbols(android_symbols, (void*) androidStub);
    stubSymbols(egl_symbols, (void*) eglStub);
#ifdef USE_EGLUT
    hybris_hook("eglGetProcAddress", (void*) eglGetProcAddress);
#endif
#ifdef USE_GLFW
    hybris_hook("eglGetProcAddress", (void*) glfwGetProcAddress);
#endif
    hybris_hook("mcpelauncher_hook", (void*) hookFunction);
    hookAndroidLog();
    if (!load_empty_library("libc.so") || !load_empty_library("libm.so"))
        return -1;
    // load stub libraries
    if (!load_empty_library("libandroid.so") || !load_empty_library("liblog.so") || !load_empty_library("libEGL.so") || !load_empty_library("libGLESv2.so") || !load_empty_library("libOpenSLES.so") || !load_empty_library("libfmod.so") || !load_empty_library("libGLESv1_CM.so"))
        return -1;
    if (!load_empty_library("libmcpelauncher_mod.so"))
        return -1;
    load_empty_library("libstdc++.so");
    Log::trace("Launcher", "Loading Minecraft library");
    std::string mcpePath = PathHelper::findDataFile("libs/libminecraftpe.so");
    void* handle = hybris_dlopen(mcpePath.c_str(), RTLD_LAZY);
    if (handle == nullptr) {
        Log::error("Launcher", "Failed to load Minecraft: %s", hybris_dlerror());
        return -1;
    }
    addHookLibrary(handle, mcpePath);

    unsigned int libBase = ((soinfo*) handle)->base;
    Log::info("Launcher", "Loaded Minecraft library");
    Log::debug("Launcher", "Minecraft is at offset 0x%x", libBase);

    ModLoader modLoader;
    modLoader.loadModsFromDirectory(PathHelper::getPrimaryDataDirectory() + "mods/");

    Log::info("Launcher", "Applying patches");

    unsigned int patchOff = (unsigned int) hybris_dlsym(handle, "_ZN12AndroidStore21createGooglePlayStoreERKSsR13StoreListener");
    patchCallInstruction((void*) patchOff, (void*) &createStoreHookFunc, true);

    patchOff = (unsigned int) hybris_dlsym(handle, "_ZN26HTTPRequestInternalAndroidC2ER11HTTPRequest");
    patchCallInstruction((void*) patchOff, (void*) &constructLinuxHttpRequestInternal, true);

    patchOff = (unsigned int) hybris_dlsym(handle, "_ZN26HTTPRequestInternalAndroid4sendEv");
    patchCallInstruction((void*) patchOff, (void*) &sendLinuxHttpRequestInternal, true);

    patchOff = (unsigned int) hybris_dlsym(handle, "_ZN26HTTPRequestInternalAndroid5abortEv");
    patchCallInstruction((void*) patchOff, (void*) &abortLinuxHttpRequestInternal, true);

    patchOff = (unsigned int) hybris_dlsym(handle, "_ZN3web4http6client7details35verify_cert_chain_platform_specificERN5boost4asio3ssl14verify_contextERKSs");
    patchCallInstruction((void*) patchOff, (void*) &verifyCertChainStub, true);

    patchOff = (unsigned int) hybris_dlsym(handle, "_ZN4xbox8services12java_interop16read_config_fileEv");
    patchCallInstruction((void*) patchOff, (void*) &xboxReadConfigFile, true);

    patchOff = (unsigned int) hybris_dlsym(handle, "_ZN4xbox8services12java_interop20log_telemetry_signinEbRKSs");
    patchCallInstruction((void*) patchOff, (void*) &xboxLogTelemetrySignin, true);

    patchOff = (unsigned int) hybris_dlsym(handle, "_ZN4xbox8services12java_interop22get_local_storage_pathEv");
    patchCallInstruction((void*) patchOff, (void*) &xboxGetLocalStoragePath, true);

    patchOff = (unsigned int) hybris_dlsym(handle, "_ZN4xbox8services6system17user_auth_android21init_sign_in_activityEi");
    patchCallInstruction((void*) patchOff, (void*) &xboxInitSignInActivity, true);

    patchOff = (unsigned int) hybris_dlsym(handle, "_ZN4xbox8services6system17user_auth_android16invoke_auth_flowEv");
    patchCallInstruction((void*) patchOff, (void*) &xboxInvokeAuthFlow, true);

    patchOff = (unsigned int) hybris_dlsym(handle, "_ZN4xbox8services5utils15get_locale_listEv");
    patchCallInstruction((void*) patchOff, (void*) &xblGetLocaleList, true);

    patchOff = (unsigned int) hybris_dlsym(handle, "_ZN4xbox8services12java_interop16register_nativesEP15JNINativeMethod");
    patchCallInstruction((void*) patchOff, (void*) &xblRegisterNatives, true);

    patchOff = (unsigned int) hybris_dlsym(handle, "_ZN4xbox8services12java_interop7log_cllERKSsS3_S3_");
    patchCallInstruction((void*) patchOff, (void*) &xblLogCLL, true);

    patchOff = (unsigned int) hybris_dlsym(handle, "_ZN3mce13RenderContext26setViewportWithFullScissorERKNS_12ViewportInfoE") + (0x85E - 0x740);
    patchCallInstruction((void*) patchOff, (void*) &setScissorRect, false);

    if (graphicsApi == GraphicsApi::OPENGL) {
        patchOff = (unsigned int) hybris_dlsym(handle, "_ZN3mce11ShaderGroup10loadShaderERNS_12RenderDeviceERKSsS4_S4_S4_");
#ifdef __linux
    if (((unsigned char*) patchOff)[0x15a + 3] != 0xA0) {
#elif __APPLE__        
    if (((unsigned char*) patchOff)[0x90C - 0x7F0 + 1] != 0xA0) {
#endif
            Log::error("Launcher", "Graphics patch error: unexpected byte");
            return -1;
        }
#ifdef __linux
        ((unsigned char*) patchOff)[0x15a + 3] += 4;
#elif __APPLE__
        ((unsigned char*) patchOff)[0x90C - 0x7F0 + 1] += 4;
#endif

        reflectShaderUniformsOriginal = (void (*)(void*)) hybris_dlsym(handle, "_ZN3mce9ShaderOGL21reflectShaderUniformsEv");
        patchOff = (unsigned int) hybris_dlsym(handle, "_ZN3mce9ShaderOGLC2ERNS_11ShaderCacheERNS_13ShaderProgramES4_S4_") + (0x720 - 0x6A0);
        patchCallInstruction((void*) patchOff, (void*) &reflectShaderUniformsHook, false);

        bindVertexArrayOriginal = (void (*)(void*, void*, void*)) hybris_dlsym(handle, "_ZN3mce9ShaderOGL18bindVertexPointersERKNS_12VertexFormatEPv");
#ifdef __linux
        patchOff = (unsigned int) hybris_dlsym(handle, "_ZN3mce9ShaderOGL10bindShaderERNS_13RenderContextERKNS_12VertexFormatEPvj") + (0xEA5 - 0xE40);
#elif __APPLE__
        patchOff = (unsigned int) hybris_dlsym(handle, "_ZN3mce9ShaderOGL10bindShaderERNS_13RenderContextERKNS_12VertexFormatEPvj") + (0x72 - 0x10);
#endif
        patchCallInstruction((void*) patchOff, (void*) &bindVertexArrayHook, false);

        patchOff = (unsigned int) hybris_dlsym(handle, "_ZN2gl21supportsImmediateModeEv");
        patchCallInstruction((void*) patchOff, (void*) &supportsImmediateModeHook, true);

        patchOff = (unsigned int) hybris_dlsym(handle, "_ZNK3mce9BufferOGL10bindBufferERNS_13RenderContextE");
#ifdef __linux
        ((unsigned char*) patchOff)[0x2C] = 0x90;
        ((unsigned char*) patchOff)[0x2D] = 0x90;
#elif __APPLE__
        ((unsigned char*) patchOff)[0x29] = 0x90;
        ((unsigned char*) patchOff)[0x2A] = 0x90;
#endif

        patchOff = (unsigned int) hybris_dlsym(handle, "_ZN3mce16ShaderProgramOGL20compileShaderProgramERNS_11ShaderCacheE");
        const char* versionStr = "#version 410\n";
#ifdef __linux
        patchOff += 0xA2 - 0x30;
#elif __APPLE__
        patchOff += 0xB3 - 0x40;
#endif
        ((unsigned char*) patchOff)[0] = 0xB9;
        *((size_t*) (patchOff + 1)) = (size_t) versionStr;
        ((unsigned char*) patchOff)[5] = 0x90;
    }

    linuxHttpRequestInternalVtable = (void**) ::operator new(8);
    linuxHttpRequestInternalVtable[0] = memberFuncCast(&LinuxHttpRequestInternal::destroy);
    linuxHttpRequestInternalVtable[1] = memberFuncCast(&LinuxHttpRequestInternal::destroy);

    if (workaroundAMD) {/*
        patchOff = (unsigned int) hybris_dlsym(handle, "_ZN21BlockTessallatorCache5resetER11BlockSourceRK8BlockPos") +
                   (0x40AD97 - 0x40ACD0);
        for (unsigned int i = 0; i < 0x40ADA0 - 0x40AD97; i++)
            ((char *) (void *) patchOff)[i] = 0x90;*/
#ifndef __APPLE__
        patchOff = (unsigned int) hybris_dlsym(handle, "_ZN21BlockTessallatorCache5resetER11BlockSourceRK8BlockPos") + (0x40AD9B - 0x40ACD0);
        patchCallInstruction((void*) patchOff, (void*) &pshufb_xmm4_xmm0, false);
#endif
    }

    Log::info("Launcher", "Patches were successfully applied");

    mcpe::string::empty = (mcpe::string*) hybris_dlsym(handle, "_ZN4Util12EMPTY_STRINGE");

    minecraft_symbols_init(handle);

    Log::info("Launcher", "Game version: %s", Common::getGameVersionStringNet().c_str());
    XboxLiveHelper::getCLL()->setAppVersion(Common::getGameVersionStringNet().std());

    Log::info("Launcher", "Creating window");

#ifdef USE_EGLUT
    eglutInit(argc, argv);
    EGLUTWindow window ("Minecraft", windowWidth, windowHeight, graphicsApi);
#endif
#ifdef USE_GLFW
    if (!glfwInit())
        return -1;
    GLFWGameWindow window ("Minecraft", windowWidth, windowHeight, graphicsApi);
    glGenVertexArrays = (void (*)(GLsizei, GLuint*)) glfwGetProcAddress("glGenVertexArrays");
    glBindVertexArray = (void (*)(GLuint)) glfwGetProcAddress("glBindVertexArray");
#endif
#ifndef __APPLE__
    window.setIcon(PathHelper::getIconPath());
#endif
    window.show();

    Log::info("Launcher", "Starting game initialization");

    JavaVM** jvmPtr = (JavaVM**) hybris_dlsym(handle, "_ZN9crossplat3JVME");
    *jvmPtr = FakeJNI::instance.getVM();

    std::shared_ptr<xbox::services::java_interop> javaInterop = xbox::services::java_interop::get_java_interop_singleton();
    javaInterop->activity = (void*) 1; // this just needs not to be null

    Log::trace("Launcher", "Initializing AppPlatform (vtable)");
    LinuxAppPlatform::initVtable(handle);
    Log::trace("Launcher", "Initializing AppPlatform (create instance)");
    platform = new LinuxAppPlatform();
    platform->setWindow(&window);
    Log::trace("Launcher", "Initializing AppPlatform (initialize call)");
    platform->initialize();

    Log::trace("Launcher", "Initializing OpenGL bindings");
    mce::Platform::OGL::InitBindings();

    Log::trace("Launcher", "Initializing MinecraftGame (create instance)");
    client = new MinecraftGame(argc, argv);
    Log::trace("Launcher", "Initializing MinecraftGame (init call)");
    AppContext ctx;
    ctx.platform = platform;
    ctx.doRender = true;
    client->init(ctx);
    Log::info("Launcher", "Game initialized");

#ifdef GAMEPAD_SUPPORT
    LinuxGamepadManager::instance.init();
#endif

    if (client->getPrimaryUserOptions()->getFullscreen())
        window.setFullscreen(true);

    modLoader.onGameInitialized(client);

    window.setDrawCallback([&window]() {
        if (client->wantToQuit()) {
            window.close();
            return;
        }

        platform->runMainThreadTasks();

#ifdef GAMEPAD_SUPPORT
        LinuxGamepadManager::instance.pool();
#endif
        client->update();
    });

    window.setWindowSizeCallback([pixelSize](int w, int h) {
        client->setRenderingSize(w, h);
        client->setUISizeAndScale(w, h, pixelSize);
    });

    window.setMouseButtonCallback([](double x, double y, int btn, MouseButtonAction action) {
        Mouse::feed((char) btn, (char) (action == MouseButtonAction::PRESS ? 1 : 0), (short) x, (short) y, 0, 0);
    });
    window.setMousePositionCallback([](double x, double y) {
        Mouse::feed(0, 0, (short) x, (short) y, 0, 0);
    });
    window.setMouseRelativePositionCallback([](double x, double y) {
        Mouse::feed(0, 0, 0, 0, (short) x, (short) y);
    });
    window.setMouseScrollCallback([](double x, double y, double dx, double dy) {
        char cdy = (char) std::max(std::min(dy * 127.0, 127.0), -127.0);
        Mouse::feed(4, cdy, 0, 0, (short) x, (short) y);
    });
    window.setKeyboardCallback([](int key, KeyAction action) {
        if (key == 112 + 10 && action == KeyAction::PRESS)
            client->getPrimaryUserOptions()->setFullscreen(!client->getPrimaryUserOptions()->getFullscreen());
        if (action == KeyAction::PRESS)
            Keyboard::feed((unsigned char) key, 1);
        else if (action == KeyAction::RELEASE)
            Keyboard::feed((unsigned char) key, 0);

    });
    window.setKeyboardTextCallback([](std::string const& c) {
        Keyboard::feedText(c, false, 0);
    });
    window.setPasteCallback([](std::string const& str) {
        for (int i = 0; i < str.length(); ) {
            char c = str[i];
            int l = 1;
            if ((c & 0b11110000) == 0b11100000)
                l = 3;
            else if ((c & 0b11100000) == 0b11000000)
                l = 2;
            Keyboard::feedText(mcpe::string(&str[i], (size_t) l), false, 0);
            i += l;
        }
    });
    window.setCloseCallback([]() {
        client->quit();
    });
    Log::trace("Launcher", "Initialized display");

    window.getWindowSize(windowWidth, windowHeight);
    // (*AppPlatform::_singleton)->_fireAppFocusGained();
    client->setRenderingSize(windowWidth, windowHeight);
    client->setUISizeAndScale(windowWidth, windowHeight, pixelSize);
    Log::trace("Launcher", "Start loop");
    window.runLoop();

    workaroundShutdownCrash(handle);

#ifndef DISABLE_CEF
    BrowserApp::Shutdown();
#endif
    XboxLiveHelper::shutdown();

    delete client;

    auto userAndroidInstance = xbox::services::system::user_impl_android::get_instance();
    if (userAndroidInstance)
        userAndroidInstance->user_signed_out();
    destroyXsapiSingleton(handle);

    return 0;
}
