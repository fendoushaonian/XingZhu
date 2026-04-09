#pragma once

#include <d3d11.h>
#include "imgui.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>

namespace sf {

// Global texture manager for Steam game images
// Downloads from Steam CDN, decodes with WIC, creates D3D11 textures.
// Usage:
//   InitTextures(device) at startup
//   UpdateTextures() each frame (loads completed downloads)
//   GetGameTexture(appId) returns ImTextureID or nullptr
//   RequestGameTexture(appId) queues a background download

void InitTextures(ID3D11Device* device);
void UpdateTextures();
void ShutdownTextures();

ImTextureID GetGameTexture(const std::string& appId);
void RequestGameTexture(const std::string& appId);

// Request a batch of game textures
void RequestGameTextures(const std::vector<std::string>& appIds);

// Priority requests (library textures — downloaded before normal queue)
void RequestGameTexturePriority(const std::string& appId);
void RequestGameTexturesPriority(const std::vector<std::string>& appIds);

// Generic: download from any URL and register with a custom key
void RequestTextureFromUrl(const std::string& key, const std::string& url);
ImTextureID GetTextureByKey(const std::string& key);
bool IsUrlTextureFailed(const std::string& key);  // Check if URL download failed

// Load a local image file as a texture (returns immediately, synchronous)
ImTextureID LoadTextureFromLocalFile(const std::string& key, const std::string& filePath);

// Get the original pixel dimensions of a loaded texture (returns false if not loaded)
bool GetTextureSize(const std::string& key, int& outW, int& outH);
bool GetGameTextureSize(const std::string& appId, int& outW, int& outH);

// Draw an image that fits a target rect while preserving aspect ratio.
// mode: 0 = Cover (fill rect, crop excess), 1 = Contain (fit inside, letterbox)
void DrawImageFit(ImDrawList* dl, ImTextureID tex, int texW, int texH,
                  float x, float y, float w, float h, int mode = 0,
                  float rounding = 0.0f, ImU32 tint = IM_COL32(255,255,255,255));

} // namespace sf
