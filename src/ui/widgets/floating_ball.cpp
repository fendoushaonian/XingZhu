#include "floating_ball.h"
#include "ui/toast.h"
#include "utils/logger.h"
#include <wincodec.h>
#include <cmath>
#include <vector>
#include <algorithm>

#pragma comment(lib, "msimg32.lib")

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace sf {

static FloatingBall* g_floatingBall = nullptr;

FloatingBall& GetFloatingBall() {
    static FloatingBall instance;
    return instance;
}

FloatingBall::FloatingBall() {
    g_floatingBall = this;
}

FloatingBall::~FloatingBall() {
    Shutdown();
    g_floatingBall = nullptr;
}

bool FloatingBall::Init(HINSTANCE hInstance) {
    hInstance_ = hInstance;
    lastUpdateTime_ = GetTickCount();

    // 加载 Logo
    logoBitmap_ = LoadLogoBitmap();

    // 创建窗口
    CreateBallWindow(hInstance);
    CreateMenuWindow(hInstance);

    Log(LogLevel::Info, "FloatingBall initialized");
    return ballHwnd_ != nullptr;
}

void FloatingBall::Shutdown() {
    if (logoBitmap_) {
        DeleteObject(logoBitmap_);
        logoBitmap_ = nullptr;
    }
    if (menuHwnd_) {
        DestroyWindow(menuHwnd_);
        menuHwnd_ = nullptr;
    }
    if (ballHwnd_) {
        DestroyWindow(ballHwnd_);
        ballHwnd_ = nullptr;
    }
}

HBITMAP FloatingBall::LoadLogoBitmap() {
    // 使用 WIC 加载 PNG
    CoInitialize(nullptr);
    IWICImagingFactory* factory = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                   IID_PPV_ARGS(&factory));
    if (FAILED(hr) || !factory) return nullptr;

    IWICBitmapDecoder* decoder = nullptr;
    hr = factory->CreateDecoderFromFilename(L"resources/icons/login.png", nullptr,
                                             GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder);
    if (FAILED(hr) || !decoder) {
        factory->Release();
        return nullptr;
    }

    IWICBitmapFrameDecode* frame = nullptr;
    decoder->GetFrame(0, &frame);
    if (!frame) {
        decoder->Release();
        factory->Release();
        return nullptr;
    }

    // 缩放到球的大小
    IWICBitmapScaler* scaler = nullptr;
    factory->CreateBitmapScaler(&scaler);
    if (scaler) {
        scaler->Initialize(frame, BALL_SIZE - 8, BALL_SIZE - 8, WICBitmapInterpolationModeHighQualityCubic);
    }

    IWICFormatConverter* converter = nullptr;
    factory->CreateFormatConverter(&converter);
    if (converter) {
        converter->Initialize(scaler ? (IWICBitmapSource*)scaler : (IWICBitmapSource*)frame,
                              GUID_WICPixelFormat32bppPBGRA,
                              WICBitmapDitherTypeNone, nullptr, 0.0,
                              WICBitmapPaletteTypeCustom);

        UINT width, height;
        converter->GetSize(&width, &height);
        logoWidth_ = width;
        logoHeight_ = height;

        // 创建 DIB
        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = width;
        bmi.bmiHeader.biHeight = -(LONG)height;  // Top-down
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        void* bits = nullptr;
        HDC hdc = GetDC(nullptr);
        HBITMAP hBitmap = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
        ReleaseDC(nullptr, hdc);

        if (hBitmap && bits) {
            converter->CopyPixels(nullptr, width * 4, width * height * 4, (BYTE*)bits);
        }

        converter->Release();
        if (scaler) scaler->Release();
        frame->Release();
        decoder->Release();
        factory->Release();

        return hBitmap;
    }

    if (scaler) scaler->Release();
    frame->Release();
    decoder->Release();
    factory->Release();
    return nullptr;
}

void FloatingBall::CreateBallWindow(HINSTANCE hInstance) {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = BallWndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_HAND);
    wc.lpszClassName = L"SteamForgeBall";
    RegisterClassExW(&wc);

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);

    // 创建分层窗口
    ballHwnd_ = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        L"SteamForgeBall", L"",
        WS_POPUP,
        screenW - BALL_SIZE - 30,
        screenH - BALL_SIZE - 150,
        BALL_SIZE, BALL_SIZE,
        NULL, NULL, hInstance, NULL
    );

    // 初始绘制
    UpdateBallImage();
}

void FloatingBall::CreateMenuWindow(HINSTANCE hInstance) {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = MenuWndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = L"SteamForgeMenu";
    RegisterClassExW(&wc);

    int menuHeight = MENU_ITEM_HEIGHT * 3 + 20;

    menuHwnd_ = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        L"SteamForgeMenu", L"",
        WS_POPUP,
        0, 0, MENU_WIDTH, menuHeight,
        NULL, NULL, hInstance, NULL
    );
}

void FloatingBall::UpdateBallImage() {
    if (!ballHwnd_) return;

    // 计算实际绘制尺寸（包含阴影和特效区域）
    int drawSize = (int)(BALL_SIZE * hoverScale_ * squishX_) + 24;
    int ballSizeX = (int)(BALL_SIZE * hoverScale_ * squishX_);
    int ballSizeY = (int)(BALL_SIZE * hoverScale_ * squishY_);

    HDC screenDC = GetDC(nullptr);
    HDC memDC = CreateCompatibleDC(screenDC);

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = drawSize;
    bmi.bmiHeader.biHeight = -drawSize;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP hBitmap = CreateDIBSection(screenDC, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, hBitmap);

    memset(bits, 0, drawSize * drawSize * 4);

    BYTE* pixels = (BYTE*)bits;
    float cx = drawSize / 2.0f;
    float cy = drawSize / 2.0f;
    float radiusX = ballSizeX / 2.0f - 2;
    float radiusY = ballSizeY / 2.0f - 2;

    // 动画参数
    float bounce = sinf(bouncePhase_) * 3.0f;
    float wobble = sinf(wobblePhase_) * 2.0f;
    float breathe = 0.9f + sinf(pulsePhase_) * 0.1f;

    // 精灵颜色（渐变）
    BYTE bodyR1, bodyG1, bodyB1;  // 顶部颜色
    BYTE bodyR2, bodyG2, bodyB2;  // 底部颜色
    if (boosterEnabled_) {
        bodyR1 = 150; bodyG1 = 245; bodyB1 = 180;
        bodyR2 = 80;  bodyG2 = 180; bodyB2 = 120;
    } else {
        bodyR1 = 180; bodyG1 = 200; bodyB1 = 255;
        bodyR2 = 100; bodyG2 = 130; bodyB2 = 200;
    }

    // 绘制精灵
    for (int y = 0; y < drawSize; y++) {
        for (int x = 0; x < drawSize; x++) {
            float dx = (x - cx) + wobble;
            float dy = (y - cy) - bounce;

            // 椭圆距离（支持挤压变形）
            float normX = dx / radiusX;
            float normY = dy / radiusY;
            float dist = sqrtf(normX * normX + normY * normY);
            float actualDist = sqrtf(dx * dx + dy * dy);

            int idx = (y * drawSize + x) * 4;

            // 1. 柔和阴影
            float shadowDy = (y - cy) + 5;
            float shadowNormX = dx / radiusX;
            float shadowNormY = shadowDy / radiusY;
            float shadowDist = sqrtf(shadowNormX * shadowNormX + shadowNormY * shadowNormY);
            if (shadowDist > 1.0f && shadowDist < 1.4f) {
                float shadowAlpha = (1.4f - shadowDist) / 0.4f;
                shadowAlpha = shadowAlpha * shadowAlpha * 0.35f;
                pixels[idx + 0] = (BYTE)(20 * shadowAlpha);
                pixels[idx + 1] = (BYTE)(20 * shadowAlpha);
                pixels[idx + 2] = (BYTE)(30 * shadowAlpha);
                pixels[idx + 3] = (BYTE)(150 * shadowAlpha);
            }

            // 2. 主体（可爱的果冻身体）
            if (dist <= 1.0f) {
                float edgeFade = 1.0f;
                if (dist > 0.92f) {
                    edgeFade = (1.0f - dist) / 0.08f;
                }

                // 渐变（上亮下暗）
                float gradientY = (normY + 1.0f) / 2.0f;
                BYTE r = (BYTE)(bodyR1 * (1 - gradientY) + bodyR2 * gradientY);
                BYTE g = (BYTE)(bodyG1 * (1 - gradientY) + bodyG2 * gradientY);
                BYTE b = (BYTE)(bodyB1 * (1 - gradientY) + bodyB2 * gradientY);

                // 顶部高光
                if (normY < -0.3f && dist < 0.7f) {
                    float highlight = (0.7f - dist) / 0.7f * (-0.3f - normY) / 0.7f * 0.4f;
                    r = (BYTE)(std::min)(255.0f, r + highlight * 100);
                    g = (BYTE)(std::min)(255.0f, g + highlight * 100);
                    b = (BYTE)(std::min)(255.0f, b + highlight * 100);
                }

                // 边缘光
                if (dist > 0.75f) {
                    float rim = (dist - 0.75f) / 0.25f * 0.2f;
                    r = (BYTE)(std::min)(255.0f, r + rim * 60);
                    g = (BYTE)(std::min)(255.0f, g + rim * 60);
                    b = (BYTE)(std::min)(255.0f, b + rim * 60);
                }

                pixels[idx + 0] = (BYTE)(b * edgeFade);
                pixels[idx + 1] = (BYTE)(g * edgeFade);
                pixels[idx + 2] = (BYTE)(r * edgeFade);
                pixels[idx + 3] = (BYTE)(255 * edgeFade);
            }

            // 3. 眼睛（根据表情变化）
            float eyeY = cy + bounce - radiusY * 0.1f;
            float eyeSpacing = radiusX * 0.32f;
            float eyeRadiusBase = radiusX * 0.16f;

            // 眨眼：眼睛高度变化
            float eyeOpenness = 1.0f - blinkProgress_;
            if (eyeOpenness < 0.1f) eyeOpenness = 0.1f;

            float leftEyeX = cx - eyeSpacing + wobble;
            float rightEyeX = cx + eyeSpacing + wobble;

            // 根据表情调整眼睛
            float eyeScaleX = 1.0f, eyeScaleY = eyeOpenness;
            bool drawPupil = true;
            bool isStarEyes = false;

            switch (currentEmotion_) {
            case 1: // 惊讶 - 大眼睛
                eyeScaleX = 1.3f;
                eyeScaleY = eyeOpenness * 1.3f;
                break;
            case 2: // 眯眼笑 - 弯弯的眼睛
                eyeScaleY = 0.3f;
                drawPupil = false;
                break;
            case 3: // 星星眼
                isStarEyes = true;
                break;
            }

            float eyeRX = eyeRadiusBase * eyeScaleX;
            float eyeRY = eyeRadiusBase * eyeScaleY;

            // 左眼
            float leftDx = (x - leftEyeX) / eyeRX;
            float leftDy = (y - eyeY) / eyeRY;
            float leftDist = sqrtf(leftDx * leftDx + leftDy * leftDy);

            // 右眼
            float rightDx = (x - rightEyeX) / eyeRX;
            float rightDy = (y - eyeY) / eyeRY;
            float rightDist = sqrtf(rightDx * rightDx + rightDy * rightDy);

            // 绘制眼睛
            if (!isStarEyes) {
                if (leftDist < 1.0f || rightDist < 1.0f) {
                    float eyeDist = (std::min)(leftDist, rightDist);
                    float eyeFade = eyeDist > 0.85f ? (1.0f - eyeDist) / 0.15f : 1.0f;
                    pixels[idx + 0] = (BYTE)(255 * eyeFade);
                    pixels[idx + 1] = (BYTE)(255 * eyeFade);
                    pixels[idx + 2] = (BYTE)(255 * eyeFade);
                    pixels[idx + 3] = (BYTE)(255 * eyeFade);
                }

                // 瞳孔
                if (drawPupil && eyeOpenness > 0.3f) {
                    float pupilR = eyeRadiusBase * 0.55f;
                    float pupilOffX = sinf(wobblePhase_ * 0.5f) * 2.0f;
                    float pupilOffY = cosf(wobblePhase_ * 0.7f) * 1.5f;

                    float lpDist = sqrtf((x - leftEyeX - pupilOffX) * (x - leftEyeX - pupilOffX) +
                                         (y - eyeY - pupilOffY) * (y - eyeY - pupilOffY));
                    float rpDist = sqrtf((x - rightEyeX - pupilOffX) * (x - rightEyeX - pupilOffX) +
                                         (y - eyeY - pupilOffY) * (y - eyeY - pupilOffY));

                    if (lpDist < pupilR || rpDist < pupilR) {
                        float pDist = (std::min)(lpDist, rpDist);
                        float pFade = pDist > pupilR - 1.0f ? (pupilR - pDist) : 1.0f;
                        pixels[idx + 0] = (BYTE)(35 * pFade);
                        pixels[idx + 1] = (BYTE)(35 * pFade);
                        pixels[idx + 2] = (BYTE)(45 * pFade);
                        pixels[idx + 3] = (BYTE)(255 * pFade);

                        // 高光
                        float hlDist = sqrtf((x - leftEyeX - pupilOffX + 2) * (x - leftEyeX - pupilOffX + 2) +
                                             (y - eyeY - pupilOffY + 2) * (y - eyeY - pupilOffY + 2));
                        float hrDist = sqrtf((x - rightEyeX - pupilOffX + 2) * (x - rightEyeX - pupilOffX + 2) +
                                             (y - eyeY - pupilOffY + 2) * (y - eyeY - pupilOffY + 2));
                        if (hlDist < 2.5f || hrDist < 2.5f) {
                            pixels[idx + 0] = 255; pixels[idx + 1] = 255;
                            pixels[idx + 2] = 255; pixels[idx + 3] = 255;
                        }
                    }
                }
            } else {
                // 星星眼
                float starPhase = sparklePhase_;
                for (int eye = 0; eye < 2; eye++) {
                    float eX = (eye == 0) ? leftEyeX : rightEyeX;
                    float sdx = x - eX;
                    float sdy = y - eyeY;
                    float starR = eyeRadiusBase * 1.2f;

                    // 四角星
                    float angle = atan2f(sdy, sdx) + starPhase;
                    float starDist = sqrtf(sdx * sdx + sdy * sdy);
                    float starShape = starR * (0.5f + 0.5f * fabsf(sinf(angle * 2)));

                    if (starDist < starShape) {
                        float sFade = 1.0f - starDist / starShape;
                        pixels[idx + 0] = (BYTE)(100 + 155 * sFade);
                        pixels[idx + 1] = (BYTE)(200 + 55 * sFade);
                        pixels[idx + 2] = (BYTE)(255);
                        pixels[idx + 3] = (BYTE)(255 * sFade);
                    }
                }
            }

            // 4. 腮红
            float blushY = cy + bounce + radiusY * 0.25f;
            float blushSpacing = radiusX * 0.5f;
            float blushR = radiusX * 0.14f;

            float lbDist = sqrtf((x - cx + blushSpacing - wobble) * (x - cx + blushSpacing - wobble) +
                                 (y - blushY) * (y - blushY));
            float rbDist = sqrtf((x - cx - blushSpacing - wobble) * (x - cx - blushSpacing - wobble) +
                                 (y - blushY) * (y - blushY));

            if (lbDist < blushR || rbDist < blushR) {
                float bDist = (std::min)(lbDist, rbDist);
                float bAlpha = (1.0f - bDist / blushR) * 0.45f * breathe;
                BYTE oB = pixels[idx + 0], oG = pixels[idx + 1], oR = pixels[idx + 2];
                pixels[idx + 0] = (BYTE)(oB * (1 - bAlpha) + 140 * bAlpha);
                pixels[idx + 1] = (BYTE)(oG * (1 - bAlpha) + 120 * bAlpha);
                pixels[idx + 2] = (BYTE)(oR * (1 - bAlpha) + 255 * bAlpha);
            }

            // 5. 嘴巴（根据表情变化）
            float mouthY = cy + bounce + radiusY * 0.4f;
            float mouthW = radiusX * 0.28f;
            float mdx = x - cx - wobble;
            float mdy = y - mouthY;

            if (currentEmotion_ == 1) {
                // 惊讶 - O形嘴
                float oDist = sqrtf(mdx * mdx + mdy * mdy);
                if (oDist < mouthW * 0.6f && oDist > mouthW * 0.3f) {
                    float oFade = 1.0f - fabsf(oDist - mouthW * 0.45f) / (mouthW * 0.15f);
                    if (oFade > 0) {
                        pixels[idx + 0] = (BYTE)(60 * oFade);
                        pixels[idx + 1] = (BYTE)(50 * oFade);
                        pixels[idx + 2] = (BYTE)(70 * oFade);
                        pixels[idx + 3] = (BYTE)(220 * oFade);
                    }
                }
            } else {
                // 微笑弧线
                float smileHeight = (currentEmotion_ == 2) ? 5.0f : 3.5f;
                float curve = (mdx * mdx) / (mouthW * mouthW) * smileHeight;
                if (fabsf(mdx) < mouthW && mdy > curve - 1.2f && mdy < curve + 1.2f) {
                    float mFade = 1.0f - fabsf(mdy - curve) / 1.2f;
                    pixels[idx + 0] = (BYTE)(70 * mFade);
                    pixels[idx + 1] = (BYTE)(60 * mFade);
                    pixels[idx + 2] = (BYTE)(80 * mFade);
                    pixels[idx + 3] = (BYTE)(200 * mFade);
                }
            }

            // 6. 闪烁星星装饰（加速器开启时）
            if (boosterEnabled_) {
                float sparkles[3][2] = {{-radiusX * 0.8f, -radiusY * 0.6f},
                                        {radiusX * 0.9f, -radiusY * 0.3f},
                                        {radiusX * 0.5f, radiusY * 0.7f}};
                for (int s = 0; s < 3; s++) {
                    float sx = cx + sparkles[s][0];
                    float sy = cy + sparkles[s][1] + bounce;
                    float phase = sparklePhase_ + s * 2.1f;
                    float sparkleSize = 3.0f + sinf(phase) * 2.0f;
                    float sDist = sqrtf((x - sx) * (x - sx) + (y - sy) * (y - sy));
                    if (sDist < sparkleSize) {
                        float sAlpha = (1.0f - sDist / sparkleSize) * (0.5f + 0.5f * sinf(phase));
                        pixels[idx + 0] = (BYTE)(std::min)(255, pixels[idx + 0] + (int)(200 * sAlpha));
                        pixels[idx + 1] = (BYTE)(std::min)(255, pixels[idx + 1] + (int)(255 * sAlpha));
                        pixels[idx + 2] = (BYTE)(std::min)(255, pixels[idx + 2] + (int)(220 * sAlpha));
                        pixels[idx + 3] = (BYTE)(std::min)(255, pixels[idx + 3] + (int)(150 * sAlpha));
                    }
                }
            }
        }
    }

    // 更新分层窗口
    POINT ptSrc = { 0, 0 };
    SIZE size = { drawSize, drawSize };
    BLENDFUNCTION blend = {};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;

    RECT rc;
    GetWindowRect(ballHwnd_, &rc);
    int centerX = rc.left + BALL_SIZE / 2;
    int centerY = rc.top + BALL_SIZE / 2;
    POINT ptWnd = { centerX - drawSize / 2, centerY - drawSize / 2 };

    SetWindowPos(ballHwnd_, NULL, 0, 0, drawSize, drawSize, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    UpdateLayeredWindow(ballHwnd_, screenDC, &ptWnd, &size, memDC, &ptSrc, 0, &blend, ULW_ALPHA);

    SelectObject(memDC, oldBitmap);
    DeleteObject(hBitmap);
    DeleteDC(memDC);
    ReleaseDC(nullptr, screenDC);
}

void FloatingBall::UpdateMenuImage() {
    if (!menuHwnd_ || !menuOpen_) return;

    int menuHeight = MENU_ITEM_HEIGHT * 3 + 24;
    int actualHeight = (int)(menuHeight * menuAnimProgress_);
    if (actualHeight < 10) actualHeight = 10;

    HDC screenDC = GetDC(nullptr);
    HDC memDC = CreateCompatibleDC(screenDC);

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = MENU_WIDTH;
    bmi.bmiHeader.biHeight = -menuHeight;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP hBitmap = CreateDIBSection(screenDC, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, hBitmap);

    BYTE* pixels = (BYTE*)bits;

    // 绘制现代风格圆角矩形背景（毛玻璃效果模拟）
    int cornerRadius = 16;
    for (int y = 0; y < menuHeight; y++) {
        for (int x = 0; x < MENU_WIDTH; x++) {
            int idx = (y * MENU_WIDTH + x) * 4;

            bool inside = true;
            float alpha = 1.0f;

            // 四个角的圆角检测
            if (x < cornerRadius && y < cornerRadius) {
                float dx = (float)(cornerRadius - x);
                float dy = (float)(cornerRadius - y);
                float dist = sqrtf(dx * dx + dy * dy);
                if (dist > cornerRadius) inside = false;
                else if (dist > cornerRadius - 1.5f) alpha = (cornerRadius - dist) / 1.5f;
            } else if (x >= MENU_WIDTH - cornerRadius && y < cornerRadius) {
                float dx = (float)(x - (MENU_WIDTH - cornerRadius));
                float dy = (float)(cornerRadius - y);
                float dist = sqrtf(dx * dx + dy * dy);
                if (dist > cornerRadius) inside = false;
                else if (dist > cornerRadius - 1.5f) alpha = (cornerRadius - dist) / 1.5f;
            } else if (x < cornerRadius && y >= menuHeight - cornerRadius) {
                float dx = (float)(cornerRadius - x);
                float dy = (float)(y - (menuHeight - cornerRadius));
                float dist = sqrtf(dx * dx + dy * dy);
                if (dist > cornerRadius) inside = false;
                else if (dist > cornerRadius - 1.5f) alpha = (cornerRadius - dist) / 1.5f;
            } else if (x >= MENU_WIDTH - cornerRadius && y >= menuHeight - cornerRadius) {
                float dx = (float)(x - (MENU_WIDTH - cornerRadius));
                float dy = (float)(y - (menuHeight - cornerRadius));
                float dist = sqrtf(dx * dx + dy * dy);
                if (dist > cornerRadius) inside = false;
                else if (dist > cornerRadius - 1.5f) alpha = (cornerRadius - dist) / 1.5f;
            }

            if (inside) {
                int itemIdx = (y - 12) / MENU_ITEM_HEIGHT;
                float hoverProgress = (itemIdx >= 0 && itemIdx < 4) ? itemHoverProgress_[itemIdx] : 0.0f;
                bool isBoosterActive = (itemIdx == 0 && boosterEnabled_);

                // 基础颜色（深色毛玻璃风格）
                BYTE r = 28, g = 32, b = 38;

                // 悬停高亮（渐变过渡）
                if (hoverProgress > 0) {
                    r = (BYTE)(r + (50 - r) * hoverProgress * 0.4f);
                    g = (BYTE)(g + (55 - g) * hoverProgress * 0.4f);
                    b = (BYTE)(b + (65 - b) * hoverProgress * 0.4f);
                }

                // 加速器激活状态
                if (isBoosterActive) {
                    r = (BYTE)(r + 15);
                    g = (BYTE)(g + 35);
                    b = (BYTE)(b + 20);
                }

                // 顶部微光效果
                float topGlow = 0.0f;
                if (y < 20) {
                    topGlow = (20.0f - y) / 20.0f * 0.08f;
                }

                pixels[idx + 0] = (BYTE)((b + topGlow * 60) * alpha);
                pixels[idx + 1] = (BYTE)((g + topGlow * 60) * alpha);
                pixels[idx + 2] = (BYTE)((r + topGlow * 60) * alpha);
                pixels[idx + 3] = (BYTE)(240 * alpha * menuAnimProgress_);
            }
        }
    }

    // 绘制分隔线
    for (int i = 1; i < 3; i++) {
        int lineY = 12 + i * MENU_ITEM_HEIGHT;
        for (int x = 16; x < MENU_WIDTH - 16; x++) {
            int idx = (lineY * MENU_WIDTH + x) * 4;
            pixels[idx + 0] = 60;
            pixels[idx + 1] = 60;
            pixels[idx + 2] = 60;
            pixels[idx + 3] = (BYTE)(80 * menuAnimProgress_);
        }
    }

    // 绘制文字
    SetBkMode(memDC, TRANSPARENT);
    HFONT font = CreateFontW(15, 0, 0, 0, FW_MEDIUM, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei UI");
    HFONT oldFont = (HFONT)SelectObject(memDC, font);

    struct MenuItem {
        const wchar_t* icon;
        const wchar_t* label;
        COLORREF color;
    };
    MenuItem items[] = {
        { L"\x26A1", boosterEnabled_ ? L"加速器 (已开启)" : L"加速器", RGB(100, 220, 140) },
        { L"\x2699", L"设置", RGB(180, 185, 195) },
        { L"\x2139", L"关于", RGB(130, 170, 220) },
    };

    for (int i = 0; i < 3; i++) {
        int itemY = 12 + i * MENU_ITEM_HEIGHT + (MENU_ITEM_HEIGHT - 15) / 2;
        float hoverProgress = itemHoverProgress_[i];

        // 图标
        SetTextColor(memDC, items[i].color);
        RECT iconRc = { 16, itemY, 36, itemY + 20 };
        DrawTextW(memDC, items[i].icon, -1, &iconRc, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

        // 文字（悬停时变亮）
        BYTE textR = GetRValue(items[i].color);
        BYTE textG = GetGValue(items[i].color);
        BYTE textB = GetBValue(items[i].color);
        if (hoverProgress > 0) {
            textR = (BYTE)(std::min)(255, (int)(textR + 40 * hoverProgress));
            textG = (BYTE)(std::min)(255, (int)(textG + 40 * hoverProgress));
            textB = (BYTE)(std::min)(255, (int)(textB + 40 * hoverProgress));
        }
        SetTextColor(memDC, RGB(textR, textG, textB));
        RECT textRc = { 40, itemY, MENU_WIDTH - 12, itemY + 20 };
        DrawTextW(memDC, items[i].label, -1, &textRc, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    }

    SelectObject(memDC, oldFont);
    DeleteObject(font);

    // 更新分层窗口
    POINT ptSrc = { 0, 0 };
    SIZE size = { MENU_WIDTH, menuHeight };
    BLENDFUNCTION blend = {};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;

    RECT ballRect;
    GetWindowRect(ballHwnd_, &ballRect);
    int ballCenterX = (ballRect.left + ballRect.right) / 2;
    POINT ptWnd;
    ptWnd.x = ballCenterX - MENU_WIDTH / 2;
    ptWnd.y = ballRect.top - menuHeight - 8;

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    if (ptWnd.x < 10) ptWnd.x = 10;
    if (ptWnd.x + MENU_WIDTH > screenW - 10) ptWnd.x = screenW - MENU_WIDTH - 10;
    if (ptWnd.y < 10) ptWnd.y = ballRect.bottom + 8;

    UpdateLayeredWindow(menuHwnd_, screenDC, &ptWnd, &size, memDC, &ptSrc, 0, &blend, ULW_ALPHA);

    SelectObject(memDC, oldBitmap);
    DeleteObject(hBitmap);
    DeleteDC(memDC);
    ReleaseDC(nullptr, screenDC);
}

void FloatingBall::Show() {
    if (ballHwnd_) {
        ShowWindow(ballHwnd_, SW_SHOWNOACTIVATE);
        visible_ = true;
        UpdateBallImage();
    }
}

void FloatingBall::Hide() {
    if (ballHwnd_) {
        ShowWindow(ballHwnd_, SW_HIDE);
        visible_ = false;
    }
    CloseMenu();
}

void FloatingBall::SetBoosterEnabled(bool enabled) {
    boosterEnabled_ = enabled;
    UpdateBallImage();
    if (menuOpen_) UpdateMenuImage();
}

void FloatingBall::Update() {
    if (!visible_ || !ballHwnd_) return;

    DWORD now = GetTickCount();
    float dt = (now - lastUpdateTime_) / 1000.0f;
    if (dt > 0.1f) dt = 0.1f;
    lastUpdateTime_ = now;

    // 基础动画相位
    float speed = boosterEnabled_ ? 1.2f : 1.0f;
    pulsePhase_ += dt * 2.5f * speed;
    bouncePhase_ += dt * 3.5f * speed;
    wobblePhase_ += dt * 2.0f;
    sparklePhase_ += dt * 4.0f;

    if (pulsePhase_ > 2.0f * (float)M_PI) pulsePhase_ -= 2.0f * (float)M_PI;
    if (bouncePhase_ > 2.0f * (float)M_PI) bouncePhase_ -= 2.0f * (float)M_PI;
    if (wobblePhase_ > 2.0f * (float)M_PI) wobblePhase_ -= 2.0f * (float)M_PI;
    if (sparklePhase_ > 2.0f * (float)M_PI) sparklePhase_ -= 2.0f * (float)M_PI;

    // 眨眼动画（随机触发）
    blinkTimer_ += dt;
    if (blinkTimer_ > 3.0f + (rand() % 30) / 10.0f) {
        blinkTimer_ = 0;
        blinkProgress_ = 1.0f;
    }
    if (blinkProgress_ > 0) {
        blinkProgress_ -= dt * 8.0f;
        if (blinkProgress_ < 0) blinkProgress_ = 0;
    }

    // 表情切换（随机）
    emotionTimer_ += dt;
    if (emotionTimer_ > 5.0f + (rand() % 50) / 10.0f) {
        emotionTimer_ = 0;
        currentEmotion_ = rand() % 4;
    }

    // 挤压动画（平滑过渡）
    float squishSpeed = 12.0f;
    squishX_ += (targetSquishX_ - squishX_) * dt * squishSpeed;
    squishY_ += (targetSquishY_ - squishY_) * dt * squishSpeed;

    // 悬停缩放
    float scaleSpeed = 8.0f;
    if (hoverScale_ < targetScale_) {
        hoverScale_ += dt * scaleSpeed * (targetScale_ - hoverScale_ + 0.1f);
        if (hoverScale_ > targetScale_) hoverScale_ = targetScale_;
    } else if (hoverScale_ > targetScale_) {
        hoverScale_ -= dt * scaleSpeed * (hoverScale_ - targetScale_ + 0.1f);
        if (hoverScale_ < targetScale_) hoverScale_ = targetScale_;
    }

    // 菜单动画
    if (menuOpen_ && menuAnimProgress_ < 1.0f) {
        menuAnimProgress_ += dt * 6.0f;
        if (menuAnimProgress_ > 1.0f) menuAnimProgress_ = 1.0f;
        UpdateMenuImage();
    } else if (!menuOpen_ && menuAnimProgress_ > 0.0f) {
        menuAnimProgress_ -= dt * 8.0f;
        if (menuAnimProgress_ < 0.0f) menuAnimProgress_ = 0.0f;
    }

    // 菜单项悬停动画
    for (int i = 0; i < 4; i++) {
        float target = (i == hoveredItem_) ? 1.0f : 0.0f;
        if (itemHoverProgress_[i] < target) {
            itemHoverProgress_[i] += dt * 10.0f;
            if (itemHoverProgress_[i] > target) itemHoverProgress_[i] = target;
        } else if (itemHoverProgress_[i] > target) {
            itemHoverProgress_[i] -= dt * 6.0f;
            if (itemHoverProgress_[i] < target) itemHoverProgress_[i] = target;
        }
    }

    // 渲染（约 60fps）
    static DWORD lastRender = 0;
    if (now - lastRender > 16) {
        UpdateBallImage();
        if (menuOpen_) UpdateMenuImage();
        lastRender = now;
    }
}

void FloatingBall::ToggleMenu() {
    menuOpen_ = !menuOpen_;
    if (menuOpen_) {
        menuAnimProgress_ = 0.01f; // 开始动画
        ShowWindow(menuHwnd_, SW_SHOWNOACTIVATE);
        UpdateMenuImage();
    } else {
        // 关闭菜单
        CloseMenu();
    }
}

void FloatingBall::CloseMenu() {
    menuOpen_ = false;
    hoveredItem_ = -1;
    for (int i = 0; i < 4; i++) itemHoverProgress_[i] = 0.0f;
    if (menuHwnd_) {
        ShowWindow(menuHwnd_, SW_HIDE);
    }
    menuAnimProgress_ = 0.0f;
}

LRESULT CALLBACK FloatingBall::BallWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    FloatingBall* ball = g_floatingBall;
    if (!ball) return DefWindowProcW(hWnd, msg, wParam, lParam);

    RECT rc;
    switch (msg) {
    case WM_LBUTTONDOWN:
        ball->dragging_ = true;
        GetCursorPos(&ball->dragStart_);
        GetWindowRect(hWnd, &rc);
        ball->windowStart_.x = (rc.left + rc.right) / 2 - BALL_SIZE / 2;
        ball->windowStart_.y = (rc.top + rc.bottom) / 2 - BALL_SIZE / 2;
        SetCapture(hWnd);
        // 点击挤压效果
        ball->targetSquishX_ = 1.15f;
        ball->targetSquishY_ = 0.85f;
        ball->currentEmotion_ = 1; // 惊讶表情
        return 0;

    case WM_MOUSEMOVE: {
        if (ball->dragging_) {
            POINT pt;
            GetCursorPos(&pt);
            int newX = ball->windowStart_.x + (pt.x - ball->dragStart_.x);
            int newY = ball->windowStart_.y + (pt.y - ball->dragStart_.y);
            // 保持中心位置
            int drawSize = (int)(BALL_SIZE * ball->hoverScale_) + 16;
            SetWindowPos(hWnd, NULL, newX - (drawSize - BALL_SIZE) / 2, newY - (drawSize - BALL_SIZE) / 2,
                         0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            ball->UpdateBallImage();
            if (ball->menuOpen_) ball->UpdateMenuImage();
        } else {
            // 悬停效果
            if (!ball->isHovered_) {
                ball->isHovered_ = true;
                ball->targetScale_ = 1.12f;
                TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hWnd, 0 };
                TrackMouseEvent(&tme);
            }
        }
        return 0;
    }

    case WM_MOUSELEAVE:
        ball->isHovered_ = false;
        ball->targetScale_ = 1.0f;
        return 0;

    case WM_LBUTTONUP: {
        if (ball->dragging_) {
            POINT pt;
            GetCursorPos(&pt);
            int dx = abs(pt.x - ball->dragStart_.x);
            int dy = abs(pt.y - ball->dragStart_.y);
            ball->dragging_ = false;
            ReleaseCapture();

            // 恢复形状
            ball->targetSquishX_ = 1.0f;
            ball->targetSquishY_ = 1.0f;
            ball->currentEmotion_ = 2; // 眯眼笑

            // 如果移动距离很小，视为点击
            if (dx < 5 && dy < 5) {
                ball->ToggleMenu();
            }
        }
        return 0;
    }

    case WM_RBUTTONUP:
        ball->Hide();
        ShowToast(u8"悬浮球已隐藏，可在设置中重新开启", ToastType::Info, 3.0f);
        return 0;
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

LRESULT CALLBACK FloatingBall::MenuWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    FloatingBall* ball = g_floatingBall;
    if (!ball) return DefWindowProcW(hWnd, msg, wParam, lParam);

    switch (msg) {
    case WM_MOUSEMOVE: {
        POINT pt;
        GetCursorPos(&pt);
        ScreenToClient(hWnd, &pt);

        int oldHovered = ball->hoveredItem_;
        ball->hoveredItem_ = -1;

        int itemY = pt.y - 10;
        if (itemY >= 0 && pt.x >= 8 && pt.x < MENU_WIDTH - 8) {
            int idx = itemY / MENU_ITEM_HEIGHT;
            if (idx >= 0 && idx < 3) {
                ball->hoveredItem_ = idx;
            }
        }

        if (oldHovered != ball->hoveredItem_) {
            ball->UpdateMenuImage();
        }

        // 跟踪鼠标离开
        TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hWnd, 0 };
        TrackMouseEvent(&tme);
        return 0;
    }

    case WM_MOUSELEAVE:
        ball->hoveredItem_ = -1;
        ball->UpdateMenuImage();
        return 0;

    case WM_LBUTTONUP: {
        if (ball->hoveredItem_ >= 0) {
            switch (ball->hoveredItem_) {
            case 0:  // 加速器
                ball->boosterEnabled_ = !ball->boosterEnabled_;
                if (ball->boosterCallback_) {
                    ball->boosterCallback_();
                }
                ShowToast(ball->boosterEnabled_ ? u8"加速器已开启" : u8"加速器已关闭",
                          ball->boosterEnabled_ ? ToastType::Success : ToastType::Info, 2.0f);
                ball->UpdateBallImage();
                break;
            case 1:  // 设置
                ShowToast(u8"设置功能开发中", ToastType::Info, 2.0f);
                break;
            case 2:  // 关于
                ShowToast(u8"星铸畅玩 v1.0 - 悬浮球", ToastType::Info, 2.0f);
                break;
            }
            ball->CloseMenu();
        }
        return 0;
    }

    case WM_KILLFOCUS:
    case WM_ACTIVATE:
        if (LOWORD(wParam) == WA_INACTIVE) {
            SetTimer(hWnd, 1, 100, nullptr);
        }
        return 0;

    case WM_TIMER:
        if (wParam == 1) {
            KillTimer(hWnd, 1);
            POINT pt;
            GetCursorPos(&pt);
            RECT ballRc, menuRc;
            GetWindowRect(ball->ballHwnd_, &ballRc);
            GetWindowRect(hWnd, &menuRc);
            if (!PtInRect(&ballRc, pt) && !PtInRect(&menuRc, pt)) {
                ball->CloseMenu();
            }
        }
        return 0;
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

} // namespace sf
