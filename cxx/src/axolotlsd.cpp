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
#include <axolotlsd.hpp>
#include <bit>
#include <cmath>
#include <forward_list>
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
  return std::pow(std::pow(2.0f, 1.0f / 12.0f), note - 69) * 440.0f;
}

player::player(U32 count, U32 freq, bool stereo) : frequency{1.0f / freq}, in_stereo{stereo}, max_voices{count} {
}

void player::play(song &&next) {
  std::swap(current, next);
  last_cursor = 0;

  std::for_each(channels.begin(), channels.end(), [](auto &&c){
    c.patch = 0;
    c.polyphony_off = 0;
    c.polyphony_on = 0;
    c.voices.clear();
  });

  seconds_elapsed = 0.0f;
  seconds_end = current.ticks_end / static_cast<F32>(current.ticks_per_second);

  if(current.version != CURRENT_VERSION) {
    throw std::runtime_error{"Version mismatch in wanted song"};
  }

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
  const auto cursor = static_cast<U32>(std::floor(current.ticks_per_second * seconds_elapsed));

  if(cursor > last_cursor) {
    auto [begin, end] = current.commands.equal_range(cursor);
    std::for_each(begin, end, [this](auto &&epair){
      auto &&[_, e] = epair;
      switch(e->get_type()) {
        case command_type::note_on: {
          if(on_voices < max_voices) {
            auto &&ptr = static_cast<command_note_on *>(e.get());
            auto phase = calculate_12tet(ptr->note) * (frequency * std::numbers::pi);
            channels[ptr->channel].voices.emplace_back(ptr->velocity / 127.0f, phase);
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

        audio[i + 0] = std::clamp(l / 4.0f, -1.0f, 1.0f);
        audio[i + 1] = std::clamp(r / 4.0f, -1.0f, 1.0f);

        seconds_elapsed += frequency;
        if(seconds_elapsed > seconds_end) {
          last_cursor = 0;
        }
        seconds_elapsed = std::fmod(seconds_elapsed, seconds_end);
      }
    } else {
      // Mono
      for (auto i = 0; i < size; i += 1) {
        auto l = 0.0f;
        auto r = 0.0f;

        handle_one(l, r);

        audio[i] = std::clamp((l + r) / 8.0f, -1.0f, 1.0f);

        seconds_elapsed += frequency;
        if(seconds_elapsed > seconds_end) {
          last_cursor = 0;
        }
        seconds_elapsed = std::fmod(seconds_elapsed, seconds_end);
      }
    }
  } else {
    std::for_each(audio.begin(), audio.end(), [](auto &&a) { a = 0; });
  }
}

song song::load(std::vector<U8> &data) {
  auto where = 4;
  auto end = data.size();
  auto &&the_song = song{};
  auto continue_for = 0;
  auto continue_data = std::forward_list<U8>{};

  auto magic_data = std::vector<U32>{data[0], data[1], data[2], data[3]};
  auto magic_concat = (magic_data[3] << 0) | (magic_data[2] << 8) |
                      (magic_data[1] << 16) | (magic_data[0] << 24);

  if (magic_concat != MAGIC) {
    throw std::runtime_error{"First 4 bytes of this song are not 'AXSD'!"};
  }

  auto data_byte = 0;

  while(where < end) {
    data_byte = data.at(where);

    auto what_value = static_cast<command_type>(data_byte);
    continue_for = byte_sizes.at(what_value);

    while (continue_for > 0) {
      where++;
      data_byte = data.at(where);
      continue_data.emplace_front(data_byte);
      continue_for--;
    }
    continue_data.reverse();



    switch (what_value) {
    case command_type::patch_data: {
      // initialize pointer
      auto &&command_ptr = new command_patch_data{};

      // initialize bytes
      auto patch = continue_data.front();
      continue_data.pop_front();

      // get sample size
      auto width = std::vector<U32>{0, 0, 0, 0};
      std::for_each(width.begin(), width.end(), [&continue_data](auto &&w) {
        w = continue_data.front();
        continue_data.pop_front();
      });
      auto patch_bytes = patch_t{};
      auto width_calc = (width[0] << 0) | (width[1] << 8) | (width[2] << 16) |
                        (width[3] << 24);
      patch_bytes.resize(width_calc);

      // sample is loaded here
      std::for_each(patch_bytes.begin(), patch_bytes.end(),
                    [&data, &where](auto &&b) { b = data.at(++where); });
      the_song.patches.insert({patch, patch_bytes});

      // dispatch pointer
      the_song.commands.emplace(0, command_ptr);
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

      // initialize bytes
      command_ptr->channel = continue_data.front();
      continue_data.pop_front();
      command_ptr->note = continue_data.front();
      continue_data.pop_front();
      command_ptr->velocity = continue_data.front();
      continue_data.pop_front();

      // dispatch pointer
      the_song.commands.emplace((time[0] << 0) | (time[1] << 8) | (time[2] << 16) | (time[3] << 24), command_ptr);
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

      // initialize bytes
      command_ptr->channel = continue_data.front();
      continue_data.pop_front();

      // dispatch pointer
      the_song.commands.emplace((time[0] << 0) | (time[1] << 8) | (time[2] << 16) | (time[3] << 24), command_ptr);
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
                  (bend_vec[2] << 16) | (bend_vec[3] << 24);
      // bit cast to signed
      command_ptr->bend = std::bit_cast<S32>(bend);

      // dispatch pointer
      the_song.commands.emplace((time[0] << 0) | (time[1] << 8) | (time[2] << 16) | (time[3] << 24), command_ptr);
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

      // initialize bytes
      command_ptr->channel = continue_data.front();
      continue_data.pop_front();
      command_ptr->program = continue_data.front();
      continue_data.pop_front();

      // dispatch pointer
      the_song.commands.emplace((time[0] << 0) | (time[1] << 8) | (time[2] << 16) | (time[3] << 24), command_ptr);
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

      the_song.version = command_ptr->song_version;

      // dispatch pointer
      the_song.commands.emplace(0, command_ptr);
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
          (rate[0] << 0) | (rate[1] << 8) | (rate[2] << 16) | (rate[3] << 24);

      the_song.ticks_per_second = command_ptr->song_rate;

      // dispatch pointer
      the_song.commands.emplace(0, command_ptr);
      break;
    }
    case command_type::end_of_track: {
      // initialize pointer
      auto &&command_ptr = new command_end_of_track{};

      // initialize version
      auto &&time = std::vector<song_tick_t>{0, 0, 0, 0};
      std::for_each(time.begin(), time.end(), [&continue_data](auto &&e) {
        e = continue_data.front();
        continue_data.pop_front();
      });

      // dispatch pointer
      the_song.commands.emplace(the_song.ticks_end, command_ptr);

      the_song.ticks_end = (time[0] << 0) | (time[1] << 8) | (time[2] << 16) | (time[3] << 24);
      break;
    }
    }
    where++;
  }

  return the_song;
}
