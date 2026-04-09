#pragma once
#include <string>
#include <d3d11.h>
#include "imgui.h"

namespace sf {

// Initialize the video player subsystem (call once after D3D11 device is created)
void InitVideoPlayer(ID3D11Device* device);

// Shutdown and release all resources
void ShutdownVideoPlayer();

// Play a video from URL (HLS .m3u8, DASH .mpd, or direct mp4)
bool PlayVideo(const std::string& url);

// Stop current playback
void StopVideo();

// Pause / Resume
void PauseVideo();
void ResumeVideo();

// Is a video currently playing (not paused)?
bool IsVideoPlaying();

// Is video paused?
bool IsVideoPaused();

// Has a video source been loaded (playing or paused)?
bool IsVideoActive();

// Duration and current position (in seconds)
double GetVideoDuration();
double GetVideoPosition();
void SeekVideo(double seconds);

// Must be called each frame to pump Media Foundation events and update the texture
void UpdateVideoPlayer();

// Get the current video frame as an ImGui texture. Returns 0 if no frame ready.
ImTextureID GetVideoFrame(int* outWidth = nullptr, int* outHeight = nullptr);

} // namespace sf
