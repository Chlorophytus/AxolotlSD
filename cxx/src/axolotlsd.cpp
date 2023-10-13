#include "../include/axolotlsd.hpp"
#include <algorithm>
#include <bit>
#include <map>
#include <stdexcept>
using namespace axolotlsd;

constexpr static U32 MAGIC = 0x41585344; // "AXSD"
constexpr static U16 CURRENT_VERSION = 0x0002;

const static std::map<command_type, size_t> byte_sizes{
  {command_type::note_on, sizeof(song_tick_t) + (sizeof(U8) * 3)},
  {command_type::note_off, sizeof(song_tick_t) + (sizeof(U8) * 1)},
  {command_type::pitchwheel, sizeof(song_tick_t) + (sizeof(U8) * 1) + sizeof(U16)},
  {command_type::program_change, sizeof(song_tick_t) + (sizeof(U8) * 2)},

  {command_type::version, sizeof(U16)},
  {command_type::rate, sizeof(U32)},
  {command_type::end_of_track, sizeof(song_tick_t)}
};

song song::load(std::vector<U8> &&data) {
  auto where = 0;
  auto end = data.size();
  auto &&the_song = song{};
  auto continue_for = 0;
  auto continue_data = std::forward_list<U8>{};

  auto magic_data = std::vector<U32>{data[0], data[1], data[2], data[3]};
  auto magic_concat = (magic_data[0] <<  0) | \
                      (magic_data[1] <<  8) | \
                      (magic_data[2] << 15) |  \
                      (magic_data[3] << 24);

  if(magic_concat != MAGIC) {
    throw std::runtime_error{"First 4 bytes of this song are not 'AXSD'!"};
  }

  for(where = 4; where < end; where++) {
    auto &&data_byte = data.at(where);
    if(continue_for > 0) {
      continue_data.emplace_front(data_byte);
      continue_for--;

      continue;
    } else {
      continue_data.reverse();
      auto &&raw_ptr = std::unique_ptr<command>{};
      auto what_value = static_cast<command_type>(data_byte);

      switch(what_value) {
        case command_type::note_on: {
          // initialize pointer
          auto &&command_ptr = new command_note_on{};

          // initialize time
          auto time = std::vector<song_tick_t>{0, 0, 0, 0};
          std::for_each(time.begin(), time.end(), [&continue_data](auto &&t) {
            t = continue_data.front();
            continue_data.pop_front();
          });
          command_ptr->time = (time[0] << 0) | (time[1] << 8) | (time[2] << 15) |  (time[3] << 24);

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
          command_ptr->time = (time[0] << 0) | (time[1] << 8) | (time[2] << 15) |  (time[3] << 24);

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
          command_ptr->time = (time[0] << 0) | (time[1] << 8) | (time[2] << 15) |  (time[3] << 24);

          // initialize bytes
          command_ptr->channel = continue_data.front();
          continue_data.pop_front();

          // initialize bend
          auto &&bend_vec = std::vector<U32>{0, 0, 0, 0};
          std::for_each(bend_vec.begin(), bend_vec.end(), [&continue_data](auto &&b) {
            b = continue_data.front();
            continue_data.pop_front();
          });
          auto bend = (bend_vec[0] << 0) | (bend_vec[1] << 8) | (bend_vec[2] << 15) |  (bend_vec[3] << 24);
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
          command_ptr->time = (time[0] << 0) | (time[1] << 8) | (time[2] << 15) |  (time[3] << 24);

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
          command_ptr->song_rate = (rate[0] << 0) | (rate[1] << 8) | (rate[2] << 15) | (rate[3] << 24);

          // dispatch pointer
          raw_ptr.reset(command_ptr);
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
          command_ptr->time = (end[0] << 0) | (end[1] << 8) | (end[2] << 15) | (end[3] << 24);

          // dispatch pointer
          raw_ptr.reset(command_ptr);
          break;
        }
      }

      continue_for = byte_sizes.at(what_value);
      the_song.command_list.emplace_front(std::move(raw_ptr));
    }
  }

  return the_song;
}
