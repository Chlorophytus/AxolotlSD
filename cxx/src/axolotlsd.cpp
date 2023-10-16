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
//   AxolotlSD for C++ source code
#include "../include/axolotlsd.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <stdexcept>
#include <numbers>
using namespace axolotlsd;

constexpr static U32 MAGIC = 0x41585344; // "AXSD"
constexpr static U16 CURRENT_VERSION = 0x0003;

const static std::map<command_type, size_t> byte_sizes{
    {command_type::note_on, sizeof(song_tick_t) + (sizeof(U8) * 3)},
    {command_type::note_off, sizeof(song_tick_t) + (sizeof(U8) * 1)},
    {command_type::pitchwheel,
     sizeof(song_tick_t) + (sizeof(U8) * 1) + sizeof(U16)},
    {command_type::program_change, sizeof(song_tick_t) + (sizeof(U8) * 2)},

    {command_type::patch_data, sizeof(U32) + sizeof(U8)},

    {command_type::version, sizeof(U16)},
    {command_type::rate, sizeof(U32)},
    {command_type::end_of_track, sizeof(song_tick_t)}};

static F32 calculate_12tet(U8 note) {
  return std::pow(2.0f, std::pow(1.0f / 12.0f, note)) * 440.0f;
}

player::player(U32 count, U32 freq, bool stereo) : frequency{1.0f / freq}, in_stereo{stereo}, max_voices{count} {
}

void player::play(song &&next) {
  info = next.info;

  last_cursor = 0;

  std::for_each(channels.begin(), channels.end(), [](auto &&c){
    c.patch = 0;
    c.polyphony_off = 0;
    c.polyphony_on = 0;
    c.voices.clear();
  });

  // rate_over_freq = static_cast<F32>(info.ticks_per_second) / static_cast<F32>(frequency);
  seconds_elapsed = 0.0f;
  seconds_end = info.ticks_end / static_cast<F32>(info.ticks_per_second);

  if(info.version != CURRENT_VERSION) {
    throw std::runtime_error{"Version mismatch in wanted song"};
  }

  commands_cache.clear();
  std::for_each(next.command_list.begin(), next.command_list.end(), [this](auto &&item) {
    commands_cache.insert({item->time, item});
  });

  playback = true;
}

void voice_group::accumulate_into(F32 &l, F32 &r) {
  std::for_each(voices.begin(), voices.end(), [&l, &r](auto &&v) {
    // TODO: wavetable
    l += std::sin(v.phase - std::numbers::pi) * v.velocity;
    r += std::sin(v.phase - std::numbers::pi) * v.velocity;

    v.phase += v.phase_add_by;
    v.phase = std::fmod(v.phase, std::numbers::pi * 2.0f);
  });
}

void player::handle_one(F32& l, F32 &r) {
  const auto cursor = static_cast<U32>(std::floor(info.ticks_per_second * seconds_elapsed));

  if(cursor > last_cursor) {
    auto &&[cache_begin, cache_end] = commands_cache.equal_range(cursor);
    std::for_each(cache_begin, cache_end, [this](auto &&epair){
      auto &&[_, eweak] = epair;
      auto &&e = eweak.lock();
      switch(e->get_type()) {
        case command_type::note_on: {
          if(on_voices < max_voices) {
            auto &&ptr = static_cast<command_note_on *>(e.get());
            channels[ptr->channel].voices.emplace_back(ptr->velocity / 127.0f, calculate_12tet(ptr->note) / frequency);
            on_voices++;
          }
          break;
        }
        case command_type::note_off: {
          if(on_voices > 0) {
            auto &&ptr = static_cast<command_note_off *>(e.get());
            auto &&voices = channels[ptr->channel].voices;
            voices.erase(voices.begin());
            on_voices--;
          }
          break;
        }
        default: {
          break;
        }
      }
    });

    last_cursor = cursor;
  }

  std::for_each(channels.begin(), channels.end(), [&l, &r](auto &&c) {
    c.accumulate_into(l, r);
  });
}

void player::tick(std::vector<F32> &audio) {
  if(playback) {
    const auto size = audio.size();
    if(in_stereo) {
      // Stereo
      for (auto i = 0; i < size; i += 2) {
        auto l = 0.0f;
        auto r = 0.0f;

        handle_one(l, r);

        // Over here we don't need to normalize
        audio[i + 0] = std::clamp(l / 16.0f, -1.0f, 1.0f);
        audio[i + 1] = std::clamp(r / 16.0f, -1.0f, 1.0f);

        seconds_elapsed += frequency;
        seconds_elapsed = std::fmod(seconds_elapsed, seconds_end);
      }
    } else {
      // Mono
      for (auto i = 0; i < size; i += 1) {
        auto l = 0.0f;
        auto r = 0.0f;

        handle_one(l, r);

        // Normalize by dividing by 2
        audio[i] = std::clamp((l + r) / 32.0f, -1.0f, 1.0f);

        seconds_elapsed += frequency;
        seconds_elapsed = std::fmod(seconds_elapsed, seconds_end);
      }
    }
  } else {
    std::for_each(audio.begin(), audio.end(), [](auto &&a) { a = 0; });
  }
}

song song::load(std::vector<U8> &&data) {
  auto where = 0;
  auto end = data.size();
  auto &&the_song = song{};
  auto continue_for = 0;
  auto continue_data = std::forward_list<U8>{};

  auto magic_data = std::vector<U32>{data[0], data[1], data[2], data[3]};
  auto magic_concat = (magic_data[0] << 0) | (magic_data[1] << 8) |
                      (magic_data[2] << 15) | (magic_data[3] << 24);

  if (magic_concat != MAGIC) {
    throw std::runtime_error{"First 4 bytes of this song are not 'AXSD'!"};
  }

  for (where = 4; where < end; where++) {
    auto &&data_byte = data.at(where);
    if (continue_for > 0) {
      continue_data.emplace_front(data_byte);
      continue_for--;

      continue;
    } else {
      continue_data.reverse();
      auto what_value = static_cast<command_type>(data_byte);

      auto &&raw_ptr = std::shared_ptr<command>{};
      switch (what_value) {
      case command_type::patch_data: {
        // initialize pointer
        auto &&command_ptr = new command_patch_data{};

        // initialize bytes
        command_ptr->patch = continue_data.front();
        continue_data.pop_front();

        // get sample size
        auto width = std::vector<U32>{0, 0, 0, 0};
        std::for_each(width.begin(), width.end(), [&continue_data](auto &&w) {
          w = continue_data.front();
          continue_data.pop_front();
        });
        command_ptr->bytes.reset(new patch_t{});
        auto width_calc = (width[0] << 0) | (width[1] << 8) | (width[2] << 15) |
                          (width[3] << 24);
        command_ptr->bytes->resize(width_calc);

        // sample is loaded here
        std::for_each(command_ptr->bytes->begin(), command_ptr->bytes->end(),
                      [&data, &where](auto &&b) { b = data.at(++where); });
        the_song.patch_map[command_ptr->patch] = command_ptr->bytes;

        // dispatch pointer
        raw_ptr.reset(command_ptr);
        break;
      }

      case command_type::note_on: {
        // initialize pointer
        auto &&command_ptr = new command_note_on{};

        // initialize time
        auto time = std::vector<song_tick_t>{0, 0, 0, 0};
        std::for_each(time.begin(), time.end(), [&continue_data](auto &&t) {
          t = continue_data.front();
          continue_data.pop_front();
        });
        command_ptr->time =
            (time[0] << 0) | (time[1] << 8) | (time[2] << 15) | (time[3] << 24);

        // initialize bytes
        command_ptr->channel = continue_data.front();
        continue_data.pop_front();
        command_ptr->note = continue_data.front();
        continue_data.pop_front();
        command_ptr->velocity = continue_data.front();
        continue_data.pop_front();

        // dispatch pointer
        raw_ptr.reset(command_ptr);
        break;
      }
      case command_type::note_off: {
        // initialize pointer
        auto &&command_ptr = new command_note_off{};

        // initialize time
        auto &&time = std::vector<song_tick_t>{0, 0, 0, 0};
        std::for_each(time.begin(), time.end(), [&continue_data](auto &&t) {
          t = continue_data.front();
          continue_data.pop_front();
        });
        command_ptr->time =
            (time[0] << 0) | (time[1] << 8) | (time[2] << 15) | (time[3] << 24);

        // initialize bytes
        command_ptr->channel = continue_data.front();
        continue_data.pop_front();

        // dispatch pointer
        raw_ptr.reset(command_ptr);
        break;
      }
      case command_type::pitchwheel: {
        // initialize pointer
        auto &&command_ptr = new command_pitchwheel{};

        // initialize time
        auto &&time = std::vector<song_tick_t>{0, 0, 0, 0};
        std::for_each(time.begin(), time.end(), [&continue_data](auto &&t) {
          t = continue_data.front();
          continue_data.pop_front();
        });
        command_ptr->time =
            (time[0] << 0) | (time[1] << 8) | (time[2] << 15) | (time[3] << 24);

        // initialize bytes
        command_ptr->channel = continue_data.front();
        continue_data.pop_front();

        // initialize bend
        auto &&bend_vec = std::vector<U32>{0, 0, 0, 0};
        std::for_each(bend_vec.begin(), bend_vec.end(),
                      [&continue_data](auto &&b) {
                        b = continue_data.front();
                        continue_data.pop_front();
                      });
        auto bend = (bend_vec[0] << 0) | (bend_vec[1] << 8) |
                    (bend_vec[2] << 15) | (bend_vec[3] << 24);
        // bit cast to signed
        command_ptr->bend = std::bit_cast<S32>(bend);

        // dispatch pointer
        raw_ptr.reset(command_ptr);
        break;
      }
      case command_type::program_change: {
        // initialize pointer
        auto &&command_ptr = new command_program_change{};

        // initialize time
        auto &&time = std::vector<song_tick_t>{0, 0, 0, 0};
        std::for_each(time.begin(), time.end(), [&continue_data](auto &&t) {
          t = continue_data.front();
          continue_data.pop_front();
        });
        command_ptr->time =
            (time[0] << 0) | (time[1] << 8) | (time[2] << 15) | (time[3] << 24);

        // initialize bytes
        command_ptr->channel = continue_data.front();
        continue_data.pop_front();
        command_ptr->program = continue_data.front();
        continue_data.pop_front();

        // dispatch pointer
        raw_ptr.reset(command_ptr);
        break;
      }
      case command_type::version: {
        // initialize pointer
        auto &&command_ptr = new command_version{};

        // initialize version
        auto &&ver = std::vector<U16>{0, 0};
        std::for_each(ver.begin(), ver.end(), [&continue_data](auto &&v) {
          v = continue_data.front();
          continue_data.pop_front();
        });
        command_ptr->song_version = (ver[0] << 0) | (ver[1] << 8);

        // dispatch pointer
        raw_ptr.reset(command_ptr);

        the_song.info.version = command_ptr->song_version;
        break;
      }
      case command_type::rate: {
        // initialize pointer
        auto &&command_ptr = new command_rate{};

        // initialize version
        auto &&rate = std::vector<U32>{0, 0, 0, 0};
        std::for_each(rate.begin(), rate.end(), [&continue_data](auto &&r) {
          r = continue_data.front();
          continue_data.pop_front();
        });
        command_ptr->song_rate =
            (rate[0] << 0) | (rate[1] << 8) | (rate[2] << 15) | (rate[3] << 24);

        // dispatch pointer
        raw_ptr.reset(command_ptr);

        the_song.info.ticks_per_second = command_ptr->song_rate;
        break;
      }
      case command_type::end_of_track: {
        // initialize pointer
        auto &&command_ptr = new command_end_of_track{};

        // initialize version
        auto &&end = std::vector<song_tick_t>{0, 0, 0, 0};
        std::for_each(end.begin(), end.end(), [&continue_data](auto &&e) {
          e = continue_data.front();
          continue_data.pop_front();
        });
        command_ptr->time =
            (end[0] << 0) | (end[1] << 8) | (end[2] << 15) | (end[3] << 24);

        // dispatch pointer
        raw_ptr.reset(command_ptr);

        the_song.info.ticks_end = command_ptr->time;
        break;
      }
      }

      continue_for = byte_sizes.at(what_value);
      the_song.command_list.emplace_front(std::move(raw_ptr));
    }
  }
  the_song.command_list.reverse();

  return the_song;
}
