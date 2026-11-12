/**
 * @file Desk.cpp
 * @brief X11 抓屏与键鼠注入
 */

#include "Desk.h"

#include <X11/Xutil.h>
#include <X11/extensions/XTest.h>
#include <X11/keysym.h>

#include <jpeglib.h>

#include <cstdio>
#include <cstring>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>

namespace Vision {

int Desk::m_xError = 0;

int Desk::onXError(Display* display, XErrorEvent* event)
{
    (void)display;
    m_xError = event ? event->error_code : 1;
    return 0;
}

namespace {

int maskShift(unsigned long mask)
{
    int shift = 0;
    if (mask == 0) {
        return 0;
    }
    while ((mask & 1ul) == 0) {
        mask >>= 1;
        ++shift;
    }
    return shift;
}

void tryDisplay(const char* name, const char* authority, Display** out)
{
    if (authority && authority[0]) {
        setenv("XAUTHORITY", authority, 0);
    }
    *out = XOpenDisplay(name);
}

}

Desk::Desk()
    : m_dpy(0)
    , m_root(0)
    , m_rootW(0)
    , m_rootH(0)
    , m_image(0)
    , m_winImage(0)
    , m_pixels(0)
    , m_shmOn(false)
    , m_desktop(true)
    , m_srcW(0)
    , m_srcH(0)
    , m_capX(0)
    , m_capY(0)
{
    std::memset(&m_shm, 0, sizeof(m_shm));
    m_shm.shmid = -1;
}

Desk::~Desk()
{
    if (m_dpy && m_winImage) {
        XDestroyImage(m_winImage);
        m_winImage = 0;
    }
    releaseShm();
    if (m_dpy) {
        XCloseDisplay(m_dpy);
        m_dpy = 0;
    }
}

bool Desk::ready() const
{
    return m_dpy != 0;
}

std::string Desk::summary() const
{
    if (!m_dpy) {
        return std::string();
    }
    char text[80];
    std::snprintf(text, sizeof(text), "%s %dx%d", DisplayString(m_dpy), m_rootW, m_rootH);
    return text;
}

bool Desk::open(std::string* err)
{
    const char* env = getenv("DISPLAY");
    if (env && env[0]) {
        m_dpy = XOpenDisplay(env);
    }
    const char* auth = "/run/user/1000/gdm/Xauthority";
    if (!m_dpy && access(auth, R_OK) == 0) {
        tryDisplay(":1", auth, &m_dpy);
        if (!m_dpy) {
            tryDisplay(":0", auth, &m_dpy);
        }
    }
    if (!m_dpy) {
        tryDisplay(":1", 0, &m_dpy);
    }
    if (!m_dpy) {
        tryDisplay(":0", 0, &m_dpy);
    }
    if (!m_dpy) {
        if (err) {
            *err = "打不开 X11 显示";
        }
        return false;
    }
    const int screen = DefaultScreen(m_dpy);
    m_root = RootWindow(m_dpy, screen);
    XWindowAttributes attr;
    if (!XGetWindowAttributes(m_dpy, m_root, &attr)) {
        if (err) {
            *err = "读不到桌面尺寸";
        }
        return false;
    }
    m_rootW = attr.width;
    m_rootH = attr.height;
    std::fprintf(stderr, "桌面 %s %dx%d\n", DisplayString(m_dpy), m_rootW, m_rootH);
    return attachShm(err);
}

bool Desk::attachShm(std::string* err)
{
    releaseShm();
    XWindowAttributes attr;
    if (!XGetWindowAttributes(m_dpy, m_root, &attr)) {
        if (err) {
            *err = "读不到桌面尺寸";
        }
        return false;
    }
    m_rootW = attr.width;
    m_rootH = attr.height;
    // 共享内存抓屏失败时留着普通 XGetImage，后面 readRoot 会走那条
    if (!XShmQueryExtension(m_dpy)) {
        return true;
    }
    m_image = XShmCreateImage(m_dpy, attr.visual, static_cast<unsigned int>(attr.depth), ZPixmap, 0, &m_shm, m_rootW, m_rootH);
    if (!m_image) {
        return true;
    }
    m_shm.shmid = shmget(IPC_PRIVATE, static_cast<size_t>(m_image->bytes_per_line) * static_cast<size_t>(m_image->height), IPC_CREAT | 0600);
    if (m_shm.shmid < 0) {
        XDestroyImage(m_image);
        m_image = 0;
        return true;
    }
    m_shm.shmaddr = static_cast<char*>(shmat(m_shm.shmid, 0, 0));
    m_shm.readOnly = False;
    if (m_shm.shmaddr == reinterpret_cast<char*>(-1)) {
        shmctl(m_shm.shmid, IPC_RMID, 0);
        XDestroyImage(m_image);
        m_image = 0;
        m_shm.shmid = -1;
        return true;
    }
    m_image->data = m_shm.shmaddr;
    // 终端里的 X 授权有时不允许共享内存。错误是异步的，不接住就会在下一次请求时把进程杀掉
    m_xError = 0;
    XErrorHandler previous = XSetErrorHandler(&Desk::onXError);
    const Bool attached = XShmAttach(m_dpy, &m_shm);
    XSync(m_dpy, False);
    XSetErrorHandler(previous);
    if (!attached || m_xError != 0) {
        std::fprintf(stderr, "共享内存抓屏不可用，改用普通抓屏\n");
        shmdt(m_shm.shmaddr);
        shmctl(m_shm.shmid, IPC_RMID, 0);
        m_image->data = 0;
        XDestroyImage(m_image);
        m_image = 0;
        m_shm.shmid = -1;
        m_shm.shmaddr = 0;
        m_shmOn = false;
        return true;
    }
    m_shmOn = true;
    return true;
}

void Desk::releaseShm()
{
    if (!m_dpy || !m_image) {
        m_shmOn = false;
        return;
    }
    if (m_shmOn) {
        XShmDetach(m_dpy, &m_shm);
    }
    XDestroyImage(m_image);
    m_image = 0;
    if (m_shmOn && m_shm.shmaddr) {
        shmdt(m_shm.shmaddr);
        shmctl(m_shm.shmid, IPC_RMID, 0);
    }
    m_shm.shmaddr = 0;
    m_shm.shmid = -1;
    m_shmOn = false;
}

bool Desk::readRoot(std::string* err)
{
    XWindowAttributes attr;
    if (!XGetWindowAttributes(m_dpy, m_root, &attr)) {
        if (err) {
            *err = "读不到桌面";
        }
        return false;
    }
    if (attr.width != m_rootW || attr.height != m_rootH || !m_image) {
        m_rootW = attr.width;
        m_rootH = attr.height;
        if (!attachShm(err)) {
            return false;
        }
    }
    if (m_shmOn) {
        if (!XShmGetImage(m_dpy, m_root, m_image, 0, 0, AllPlanes)) {
            if (err) {
                *err = "抓屏失败";
            }
            return false;
        }
        return true;
    }
    if (m_image) {
        XDestroyImage(m_image);
        m_image = 0;
    }
    m_image = XGetImage(m_dpy, m_root, 0, 0, static_cast<unsigned int>(m_rootW), static_cast<unsigned int>(m_rootH), AllPlanes, ZPixmap);
    if (!m_image) {
        if (err) {
            *err = "抓屏失败";
        }
        return false;
    }
    return true;
}

void Desk::setShare(bool desktop, const std::string& process, const std::string& title)
{
    m_desktop = desktop;
    m_process = desktop ? std::string() : process;
    m_title = desktop ? std::string() : title;
}

bool Desk::desktop() const
{
    return m_desktop;
}

std::string Desk::source() const
{
    if (m_desktop) {
        return "桌面";
    }
    if (!m_process.empty()) {
        return m_process;
    }
    if (!m_title.empty()) {
        return m_title;
    }
    return "进程画面";
}

std::string Desk::process() const
{
    return m_process;
}

std::string Desk::title() const
{
    return m_title;
}

std::string Desk::lower(const std::string& text)
{
    std::string out = text;
    for (size_t i = 0; i < out.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(out[i]);
        if (c >= 'A' && c <= 'Z') {
            out[i] = static_cast<char>(c - 'A' + 'a');
        }
    }
    return out;
}

bool Desk::hasText(const std::string& hay, const std::string& needle)
{
    if (needle.empty()) {
        return true;
    }
    return lower(hay).find(lower(needle)) != std::string::npos;
}

void Desk::windowName(Window window, std::string* process, std::string* title) const
{
    process->clear();
    title->clear();
    XClassHint hint;
    std::memset(&hint, 0, sizeof(hint));
    if (XGetClassHint(m_dpy, window, &hint)) {
        if (hint.res_class && hint.res_class[0]) {
            *process = hint.res_class;
        } else if (hint.res_name && hint.res_name[0]) {
            *process = hint.res_name;
        }
        if (hint.res_name) {
            XFree(hint.res_name);
        }
        if (hint.res_class) {
            XFree(hint.res_class);
        }
    }
    XTextProperty prop;
    std::memset(&prop, 0, sizeof(prop));
    if (!XGetWMName(m_dpy, window, &prop) || !prop.value) {
        return;
    }
    char** list = 0;
    int count = 0;
    if (Xutf8TextPropertyToTextList(m_dpy, &prop, &list, &count) == Success && count > 0 && list && list[0]) {
        *title = list[0];
        XFreeStringList(list);
    } else if (prop.encoding == XInternAtom(m_dpy, "STRING", False)) {
        *title = reinterpret_cast<char*>(prop.value);
    }
    XFree(prop.value);
}

void Desk::collect(Window window, int depth, std::vector<std::pair<std::string, std::string>>* out)
{
    if (depth > 6 || out->size() >= 40) {
        return;
    }
    Window root = 0;
    Window parent = 0;
    Window* kids = 0;
    unsigned int count = 0;
    if (!XQueryTree(m_dpy, window, &root, &parent, &kids, &count)) {
        return;
    }
    for (unsigned int i = 0; i < count; ++i) {
        XWindowAttributes attr;
        std::memset(&attr, 0, sizeof(attr));
        if (!XGetWindowAttributes(m_dpy, kids[i], &attr)) {
            continue;
        }
        if (attr.map_state == IsViewable && attr.c_class == InputOutput && attr.width >= 40 && attr.height >= 40
            && !attr.override_redirect) {
            std::string process;
            std::string title;
            windowName(kids[i], &process, &title);
            if (!process.empty() || !title.empty()) {
                out->push_back(std::make_pair(process, title));
            }
        }
        if (out->size() < 40) {
            collect(kids[i], depth + 1, out);
        }
    }
    if (kids) {
        XFree(kids);
    }
}

std::vector<std::pair<std::string, std::string>> Desk::windows()
{
    std::vector<std::pair<std::string, std::string>> out;
    if (!m_dpy) {
        return out;
    }
    m_xError = 0;
    XErrorHandler previous = XSetErrorHandler(&Desk::onXError);
    collect(m_root, 0, &out);
    XSync(m_dpy, False);
    XSetErrorHandler(previous);
    return out;
}

Window Desk::findWindow() const
{
    if (!m_dpy || (m_process.empty() && m_title.empty())) {
        return 0;
    }
    Window best = 0;
    int bestArea = 0;
    m_xError = 0;
    XErrorHandler previous = XSetErrorHandler(&Desk::onXError);
    std::vector<Window> pending;
    pending.push_back(m_root);
    for (size_t n = 0; n < pending.size() && n < 400; ++n) {
        Window root = 0;
        Window parent = 0;
        Window* kids = 0;
        unsigned int count = 0;
        if (!XQueryTree(m_dpy, pending[n], &root, &parent, &kids, &count)) {
            continue;
        }
        for (unsigned int i = 0; i < count; ++i) {
            pending.push_back(kids[i]);
            XWindowAttributes attr;
            std::memset(&attr, 0, sizeof(attr));
            if (!XGetWindowAttributes(m_dpy, kids[i], &attr)) {
                continue;
            }
            if (attr.map_state != IsViewable || attr.c_class != InputOutput || attr.width < 40 || attr.height < 40) {
                continue;
            }
            std::string process;
            std::string title;
            windowName(kids[i], &process, &title);
            if (!hasText(process, m_process) || !hasText(title, m_title)) {
                continue;
            }
            const int area = attr.width * attr.height;
            if (area > bestArea) {
                bestArea = area;
                best = kids[i];
            }
        }
        if (kids) {
            XFree(kids);
        }
    }
    XSync(m_dpy, False);
    XSetErrorHandler(previous);
    return best;
}

bool Desk::readFrame(std::string* err)
{
    if (m_desktop) {
        if (m_winImage) {
            XDestroyImage(m_winImage);
            m_winImage = 0;
        }
        if (!readRoot(err)) {
            return false;
        }
        m_pixels = m_image;
        m_srcW = m_rootW;
        m_srcH = m_rootH;
        m_capX = 0;
        m_capY = 0;
        return m_pixels != 0;
    }
    const Window target = findWindow();
    if (!target) {
        if (err) {
            *err = "没有匹配的窗口";
        }
        return false;
    }
    XWindowAttributes attr;
    std::memset(&attr, 0, sizeof(attr));
    if (!XGetWindowAttributes(m_dpy, target, &attr) || attr.width < 2 || attr.height < 2) {
        if (err) {
            *err = "读不到窗口";
        }
        return false;
    }
    if (m_winImage) {
        XDestroyImage(m_winImage);
        m_winImage = 0;
    }
    m_xError = 0;
    XErrorHandler previous = XSetErrorHandler(&Desk::onXError);
    m_winImage = XGetImage(m_dpy, target, 0, 0, static_cast<unsigned int>(attr.width), static_cast<unsigned int>(attr.height), AllPlanes, ZPixmap);
    XSync(m_dpy, False);
    XSetErrorHandler(previous);
    if (!m_winImage || m_xError != 0) {
        if (m_winImage) {
            XDestroyImage(m_winImage);
            m_winImage = 0;
        }
        if (err) {
            *err = "抓不到这个窗口";
        }
        return false;
    }
    int x = 0;
    int y = 0;
    Window child = 0;
    XTranslateCoordinates(m_dpy, target, m_root, 0, 0, &x, &y, &child);
    m_pixels = m_winImage;
    m_srcW = attr.width;
    m_srcH = attr.height;
    m_capX = x;
    m_capY = y;
    return true;
}

bool Desk::grabJpeg(int longEdge, int quality, CommBytes* jpeg, int* imageW, int* imageH, std::string* err)
{
    if (!m_dpy || !readFrame(err)) {
        return false;
    }
    if (m_pixels->bits_per_pixel != 32) {
        if (err) {
            *err = "桌面不是 32 位色";
        }
        return false;
    }
    int outW = m_srcW;
    int outH = m_srcH;
    const int edge = outW > outH ? outW : outH;
    if (longEdge > 0 && longEdge < 65535 && edge > longEdge) {
        const double scale = static_cast<double>(longEdge) / static_cast<double>(edge);
        outW = static_cast<int>(outW * scale);
        outH = static_cast<int>(outH * scale);
        if (outW < 2) {
            outW = 2;
        }
        if (outH < 2) {
            outH = 2;
        }
    }
    m_rgb.resize(static_cast<size_t>(outW) * static_cast<size_t>(outH) * 3);
    const int rShift = maskShift(m_pixels->red_mask);
    const int gShift = maskShift(m_pixels->green_mask);
    const int bShift = maskShift(m_pixels->blue_mask);
    for (int y = 0; y < outH; ++y) {
        const int sy = y * m_srcH / outH;
        const unsigned char* row = reinterpret_cast<const unsigned char*>(m_pixels->data) + static_cast<size_t>(sy) * static_cast<size_t>(m_pixels->bytes_per_line);
        unsigned char* dst = m_rgb.data() + static_cast<size_t>(y) * static_cast<size_t>(outW) * 3;
        for (int x = 0; x < outW; ++x) {
            const int sx = x * m_srcW / outW;
            const uint32_t pix = *reinterpret_cast<const uint32_t*>(row + static_cast<size_t>(sx) * 4);
            dst[0] = static_cast<unsigned char>((pix >> rShift) & 0xff);
            dst[1] = static_cast<unsigned char>((pix >> gShift) & 0xff);
            dst[2] = static_cast<unsigned char>((pix >> bShift) & 0xff);
            dst += 3;
        }
    }

    if (quality < 1) {
        quality = 60;
    }
    if (quality > 100) {
        quality = 100;
    }
    unsigned char* mem = 0;
    unsigned long memSize = 0;
    jpeg_compress_struct cinfo;
    jpeg_error_mgr jerr;
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    jpeg_mem_dest(&cinfo, &mem, &memSize);
    cinfo.image_width = static_cast<JDIMENSION>(outW);
    cinfo.image_height = static_cast<JDIMENSION>(outH);
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;
    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, quality, TRUE);
    jpeg_start_compress(&cinfo, TRUE);
    while (cinfo.next_scanline < cinfo.image_height) {
        JSAMPROW row = m_rgb.data() + static_cast<size_t>(cinfo.next_scanline) * static_cast<size_t>(outW) * 3;
        jpeg_write_scanlines(&cinfo, &row, 1);
    }
    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);
    jpeg->assign(mem, mem + memSize);
    free(mem);
    *imageW = outW;
    *imageH = outH;
    return true;
}

unsigned long Desk::keySym(uint16_t vk)
{
    if (vk >= 0x41 && vk <= 0x5A) {
        return static_cast<unsigned long>(XK_a + (vk - 0x41));
    }
    if (vk >= 0x30 && vk <= 0x39) {
        return static_cast<unsigned long>(XK_0 + (vk - 0x30));
    }
    if (vk >= 0x70 && vk <= 0x7B) {
        return static_cast<unsigned long>(XK_F1 + (vk - 0x70));
    }
    switch (vk) {
    case 0x08: return XK_BackSpace;
    case 0x09: return XK_Tab;
    case 0x0D: return XK_Return;
    case 0x1B: return XK_Escape;
    case 0x20: return XK_space;
    case 0x10: return XK_Shift_L;
    case 0x11: return XK_Control_L;
    case 0x12: return XK_Alt_L;
    case 0x14: return XK_Caps_Lock;
    case 0x25: return XK_Left;
    case 0x26: return XK_Up;
    case 0x27: return XK_Right;
    case 0x28: return XK_Down;
    case 0x2E: return XK_Delete;
    case 0x21: return XK_Page_Up;
    case 0x22: return XK_Page_Down;
    case 0x23: return XK_End;
    case 0x24: return XK_Home;
    case 0x2D: return XK_Insert;
    case 0x5B: return XK_Super_L;
    case 0xA0: return XK_Shift_L;
    case 0xA1: return XK_Shift_R;
    case 0xA2: return XK_Control_L;
    case 0xA3: return XK_Control_R;
    case 0xA4: return XK_Alt_L;
    case 0xA5: return XK_Alt_R;
    case 0xBA: return XK_semicolon;
    case 0xBB: return XK_equal;
    case 0xBC: return XK_comma;
    case 0xBD: return XK_minus;
    case 0xBE: return XK_period;
    case 0xBF: return XK_slash;
    case 0xC0: return XK_grave;
    case 0xDB: return XK_bracketleft;
    case 0xDC: return XK_backslash;
    case 0xDD: return XK_bracketright;
    case 0xDE: return XK_apostrophe;
    default: return 0;
    }
}

void Desk::inject(const InputMsg& msg, int imageW, int imageH)
{
    if (!m_dpy || imageW < 1 || imageH < 1) {
        return;
    }
    if (msg.vk != 0) {
        const unsigned long sym = keySym(msg.vk);
        const KeyCode code = sym ? XKeysymToKeycode(m_dpy, sym) : 0;
        if (code) {
            XTestFakeKeyEvent(m_dpy, code, msg.down ? True : False, CurrentTime);
            XFlush(m_dpy);
        }
        return;
    }
    if (m_srcW < 1 || m_srcH < 1) {
        return;
    }
    const int sx = m_capX + static_cast<int>(msg.x) * m_srcW / imageW;
    const int sy = m_capY + static_cast<int>(msg.y) * m_srcH / imageH;
    XTestFakeMotionEvent(m_dpy, DefaultScreen(m_dpy), sx, sy, CurrentTime);
    if (msg.wheel > 0) {
        for (int i = 0; i < msg.wheel; ++i) {
            XTestFakeButtonEvent(m_dpy, 4, True, CurrentTime);
            XTestFakeButtonEvent(m_dpy, 4, False, CurrentTime);
        }
    } else if (msg.wheel < 0) {
        for (int i = 0; i < -msg.wheel; ++i) {
            XTestFakeButtonEvent(m_dpy, 5, True, CurrentTime);
            XTestFakeButtonEvent(m_dpy, 5, False, CurrentTime);
        }
    }
    const int buttons[3] = { 1, 3, 2 };
    const uint8_t bits[3] = { 1, 2, 4 };
    const bool press = msg.action == InputMsg::ActionPress || (msg.action == InputMsg::ActionLegacy && msg.down);
    const bool release = msg.action == InputMsg::ActionRelease || (msg.action == InputMsg::ActionLegacy && !msg.down && msg.buttons);
    if (press || release) {
        for (int i = 0; i < 3; ++i) {
            if (msg.buttons & bits[i]) {
                XTestFakeButtonEvent(m_dpy, static_cast<unsigned int>(buttons[i]), press ? True : False, CurrentTime);
            }
        }
    }
    XFlush(m_dpy);
}

bool Desk::pointer(int imageW, int imageH, int* x, int* y) const
{
    if (!m_dpy || imageW < 1 || imageH < 1 || m_srcW < 1 || m_srcH < 1) {
        return false;
    }
    Window root = 0;
    Window child = 0;
    int rx = 0;
    int ry = 0;
    int wx = 0;
    int wy = 0;
    unsigned int mask = 0;
    if (!XQueryPointer(m_dpy, m_root, &root, &child, &rx, &ry, &wx, &wy, &mask)) {
        return false;
    }
    int lx = rx - m_capX;
    int ly = ry - m_capY;
    if (lx < 0) {
        lx = 0;
    }
    if (ly < 0) {
        ly = 0;
    }
    if (lx >= m_srcW) {
        lx = m_srcW - 1;
    }
    if (ly >= m_srcH) {
        ly = m_srcH - 1;
    }
    *x = lx * imageW / m_srcW;
    *y = ly * imageH / m_srcH;
    return true;
}

}
