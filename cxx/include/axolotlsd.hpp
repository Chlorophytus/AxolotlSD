#pragma once
#include "configuration.hpp"
#include <cstdint>
#include <forward_list>
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

// ============================================================================
enum class command_type : U8 {
  // regular
  note_on = 0x01,
  note_off = 0x02,
  pitchwheel = 0x03,
  program_change = 0x04,
  // meta
  version = 0xFC,
  rate = 0xFD,
  end_of_track = 0xFE
};
struct command {
  virtual command_type get_type() = 0;
};
struct tick_command : command {
  song_tick_t time; // time from the start
};
struct command_note_on : tick_command {
  virtual command_type get_type() { return command_type::note_on; }
  U8 channel;
  U8 note;
  U8 velocity;
};
struct command_note_off : tick_command {
  virtual command_type get_type() { return command_type::note_off; }
  U8 channel;
};
struct command_pitchwheel : tick_command {
  virtual command_type get_type() { return command_type::pitchwheel; }
  U8 channel;
  S32 bend;
};
struct command_program_change : tick_command {
  virtual command_type get_type() { return command_type::program_change; }
  U8 channel;
  U8 program;
};
struct command_version : command {
  virtual command_type get_type() { return command_type::version; }
  U16 song_version;
};
struct command_rate : command {
  virtual command_type get_type() { return command_type::rate; }
  U32 song_rate;
};
struct command_end_of_track : tick_command {
  virtual command_type get_type() { return command_type::end_of_track; }
};
// ============================================================================
struct song {
  song_tick_t ticks_end;
  song_tick_t ticks_current = 0;
  std::unique_ptr<command> last_command = nullptr;

  std::forward_list<std::unique_ptr<command>> command_list{};

  static song load(std::vector<U8> &&);
  void fill();
};
}
