#include "../include/axolotlsd.hpp"
#include <algorithm>
#include <bit>
#include <stdexcept>
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

player::player(U32 count, U32 freq, bool stereo) : voices{}, frequency{freq}, in_stereo{stereo} {
  voices.resize(count);
  std::for_each(voices.begin(), voices.end(), [](auto &&v) {
    v = voice{.patch = 0, .velocity = 0.0f, .phase_add_by = 0.0f, .phase = 0.0f};
  });
}

void player::play(song &&next) {
  info = next.info;

  polyphony_off = 0;
  polyphony_on = 0;
  rate_over_freq = static_cast<F32>(info.ticks_per_second) / static_cast<F32>(frequency);

  if(info.version != CURRENT_VERSION) {
    throw std::runtime_error{"Version mismatch in wanted song"};
  }

  commands_cache.clear();
  for(auto &&item : next.command_list) {
    commands_cache.insert({item->time, item});
  }

  playback = true;
}

void player::tick(std::vector<S16> &audio) {
  if(playback) {

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
