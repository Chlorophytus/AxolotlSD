// ============================================================================
//   Copyright 2023 Roland Metivier <metivier.roland@chlorophyt.us>
//
//   Licensed under the Apache License, Version 2.0 (the "License");
//   you may not use this file except in compliance with the License.
//   You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the License is distributed on an "AS IS" BASIS,
//   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//   See the License for the specific language governing permissions and
//   limitations under the License.
// ============================================================================
// Test program file
#include "configuration.hpp"
#include <axolotlsd.hpp>
#include <cstdio>
#include <cstdlib>
#include <raylib.h>

constexpr static auto FILL_FRAMES = 1800;
constexpr static auto USE_STEREO = true;

int main(int argc, char **argv) {
  std::fprintf(stderr,
               "AxolotlSD C++ tester " axolotlsd_test_VSTRING_FULL "\n");
  std::fprintf(stderr, "Using AxolotlSD C++ lib " axolotlsd_VSTRING_FULL "\n");

  if (argc != 2) {
    std::fprintf(stderr, "Please specify a file name to play back\n");
    return EXIT_FAILURE;
  }

  auto reader = std::fopen(argv[1], "r");
  auto song_bytes = std::vector<axolotlsd::U8>{};
  int ch;
  while ((ch = std::fgetc(reader)) != EOF) {
    auto byte = static_cast<axolotlsd::U8>(ch);
    song_bytes.emplace_back(byte);
  }
  auto player = axolotlsd::player{32, 44100, USE_STEREO};
  player.play(axolotlsd::song::load(song_bytes),
              axolotlsd::environment{.feedback_L = 0.85f,
                                     .feedback_R = 0.85f,
                                     .wet_L = -0.33f,
                                     .wet_R = 0.33f,
                                     .cursor_increment = 0x01,
                                     .cursor_max = 0x1800});

  InitWindow(640, 480, "AxolotlSD C++ tester " axolotlsd_test_VSTRING_FULL);
  InitAudioDevice();
	SetAudioStreamBufferSizeDefault(FILL_FRAMES);
  auto audio_stream =
      LoadAudioStream(44100, 8 * sizeof(axolotlsd::F32), (USE_STEREO ? 2 : 1));
  auto buffer_vector = std::vector<axolotlsd::F32>{};
  buffer_vector.resize(FILL_FRAMES * (USE_STEREO ? 2 : 1), 0.0f);
  PlayAudioStream(audio_stream);
  SetTargetFPS(60);
  while (!WindowShouldClose()) {
   	while(IsAudioStreamProcessed(audio_stream))  {
			player.tick(buffer_vector);
      UpdateAudioStream(audio_stream, buffer_vector.data(),
                        buffer_vector.size() / (USE_STEREO ? 2 : 1));
    } 
    BeginDrawing();
    ClearBackground(RAYWHITE);
    DrawFPS(20, 20);
    if (IsKeyReleased('P')) {
      player.playback = !player.playback;
    }
    if (player.playback) {
      DrawText("Music playing, 'P' pauses", 20, 50, 20, GREEN);
    } else {
      DrawText("Music paused, 'P' plays", 20, 50, 20, RED);
    }
    EndDrawing();
  }
  StopAudioStream(audio_stream);
  UnloadAudioStream(audio_stream);
  CloseAudioDevice();
  CloseWindow();

  return EXIT_SUCCESS;
}
