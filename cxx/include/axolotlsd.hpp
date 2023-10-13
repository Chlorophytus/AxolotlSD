#pragma once
#include "configuration.hpp"
#include <cstdint>
#include <forward_list>
#include <map>
#include <memory>
#include <vector>

namespace axolotlsd {
using U8 = std::uint8_t;
using U16 = std::uint16_t;
using U32 = std::uint32_t;
using U64 = std::uint64_t;

using S8 = std::int8_t;
using S16 = std::int16_t;
using S32 = std::int32_t;
using S64 = std::int64_t;

using F32 = float;
using F64 = double;

using audio_data_t = F32;
using song_tick_t = U32;
using patch_t = std::vector<U8>;
// ============================================================================
enum class command_type : U8 {
  // regular
  note_on = 0x01,
  note_off = 0x02,
  pitchwheel = 0x03,
  program_change = 0x04,
  // patches
  patch_data = 0x80,
  // meta
  version = 0xFC,
  rate = 0xFD,
  end_of_track = 0xFE
};
struct command {
  song_tick_t time = 0;
  virtual command_type get_type() = 0;
};
struct command_note_on : command {
  virtual command_type get_type() { return command_type::note_on; }
  U8 channel;
  U8 note;
  U8 velocity;
};
struct command_note_off : command {
  virtual command_type get_type() { return command_type::note_off; }
  U8 channel;
};
struct command_pitchwheel : command {
  virtual command_type get_type() { return command_type::pitchwheel; }
  U8 channel;
  S32 bend;
};
struct command_program_change : command {
  virtual command_type get_type() { return command_type::program_change; }
  U8 channel;
  U8 program;
};
struct command_patch_data : command {
  virtual command_type get_type() { return command_type::patch_data; }
  U8 patch;
  std::shared_ptr<patch_t> bytes;
};
struct command_version : command {
  virtual command_type get_type() { return command_type::version; }
  U16 song_version;
};
struct command_rate : command {
  virtual command_type get_type() { return command_type::rate; }
  U32 song_rate;
};
struct command_end_of_track : command {
  virtual command_type get_type() { return command_type::end_of_track; }
};
// ============================================================================
struct voice {
  U8 patch;
  F32 velocity;
  F32 phase_add_by;
  F32 phase;

  void accumulate_into(std::vector<S16> &);
};
// ============================================================================
struct song_info {
  U16 version;
  song_tick_t ticks_end;
  song_tick_t ticks_per_second;
};
struct song {
  std::forward_list<std::shared_ptr<command>> command_list{};
  std::map<U8, std::weak_ptr<patch_t>> patch_map{};

  song_info info;

  static song load(std::vector<U8> &&);
};

struct player {
  F32 rate_over_freq;
  F32 seconds_elapsed;
  U32 frequency;

  U32 polyphony_on = 0;
  U32 polyphony_off = 0;

  song_info info;
  bool in_stereo;

  explicit player(U32, U32, bool);

  std::vector<voice> voices;
  std::multimap<song_tick_t, std::weak_ptr<command>> commands_cache{};

  bool playback = false;

  void play(song &&);
  void pause();
  void tick(std::vector<S16> &);
};
} // namespace axolotlsd
