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
#include <raylib.h>
#include <axolotlsd.hpp>
#include <cstdlib>
#include <cstdio>

static auto player = std::unique_ptr<axolotlsd::player>{nullptr};

void audio_callback(void *buffer_data, unsigned int frames) {
  auto buffer_vector = std::vector<axolotlsd::F32>{};
  buffer_vector.resize(frames * 2, 0.0f);

  player->tick(buffer_vector);

  for(auto i = 0; i < frames * 2; i++) {
    reinterpret_cast<axolotlsd::F32 *>(buffer_data)[i] = buffer_vector[i];
  }
}

int main(int argc, char **argv) {
  std::fprintf(stderr, "AxolotlSD C++ tester " axolotlsd_test_VSTRING_FULL "\n");
  std::fprintf(stderr, "Using AxolotlSD C++ lib " axolotlsd_VSTRING_FULL "\n");

  if(argc != 2) {
    std::fprintf(stderr, "Please specify a file name to play back\n");
    return EXIT_FAILURE;
  }

  auto reader = std::fopen(argv[1], "r");
  auto song_bytes = std::vector<axolotlsd::U8>{};
  int ch;
  while((ch = std::fgetc(reader)) != EOF) {
    auto byte = static_cast<axolotlsd::U8>(ch);
    song_bytes.emplace_back(byte);
    std::fprintf(stderr, "%02hhx ", byte);
  }
  std::fprintf(stderr, "\n");
  player = std::make_unique<axolotlsd::player>(32, 44100, true);
  player->play(axolotlsd::song::load(song_bytes));

  InitWindow(640, 480, "AxolotlSD C++ tester " axolotlsd_test_VSTRING_FULL);
  InitAudioDevice();
  auto audio_stream = LoadAudioStream(44100, 8 * sizeof(axolotlsd::F32), 2);
  SetAudioStreamCallback(audio_stream, &audio_callback);
  PlayAudioStream(audio_stream);
  SetTargetFPS(60);
  while (!WindowShouldClose()) {
    BeginDrawing();
    ClearBackground(RAYWHITE);
    EndDrawing();
  }
  UnloadAudioStream(audio_stream);
  CloseAudioDevice();
  CloseWindow();

  return EXIT_SUCCESS;
}
