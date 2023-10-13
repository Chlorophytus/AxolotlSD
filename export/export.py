#!/usr/bin/env python3
# =============================================================================
#   Copyright 2023 Roland Metivier <metivier.roland@chlorophyt.us>
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
# =============================================================================
#   Exporter for MIDI files, convert to AxolotlSD dumps
import mido
import sys
import struct
import json
import wave

RATE = 60  # hertz

with open(sys.argv[2], 'wb') as writer:
    reader = mido.MidiFile(sys.argv[1])
    writer.write(b'AXSD')
    writer.write(struct.pack('<BH', 0xFC, 0x0003))
    writer.write(struct.pack('<BI', 0xFD, RATE))

    # TODO: 8-bit Sample playback
    sample_data = json.load(open(sys.argv[3], 'r'))


    tempo = 500000.0

    # pitch bends affect all notes in a channel
    channel_bend = [0.0] * 16
    end_of_track = None

    for track in reader.tracks:
        time = 0
        real_time = 0.0

        for message in track:
            time += message.time
            if message.is_meta:
                # print(f"META {time} {real_time}/{reader.length} {message}")
                match message.type:
                    case 'set_tempo':
                        tempo = message.tempo
                        print(f"{time} -> tempo {tempo}")
                    case 'end_of_track':
                        this_end = mido.tick2second(
                            time, reader.ticks_per_beat, tempo)
                        if end_of_track is None or this_end > end_of_track:
                            end_of_track = this_end
                    case _:
                        pass
            else:
                # print(f"MIDI {time} {real_time}/{reader.length} {message}")
                real_time = mido.tick2second(
                    time, reader.ticks_per_beat, tempo)
                match message.type:
                    case 'note_on':
                        writer.write(struct.pack('<BIBBB', 0x01, int(
                            real_time * RATE), message.channel, message.note,
                            message.velocity))
                    case 'note_off':
                        writer.write(struct.pack('<BIB', 0x02, int(
                            real_time * RATE), message.channel))
                    case 'pitchwheel':
                        writer.write(struct.pack('<BIBi', 0x03, int(
                            real_time * RATE), message.channel, message.pitch))
                    case 'program_change':
                        writer.write(struct.pack('<BIBB', 0x04, int(
                            real_time * RATE), message.channel,
                            message.program))
                    case _:
                        pass

    if end_of_track is None:
        end_of_track = mido.tick2second(time, reader.ticks_per_beat, tempo)
    writer.write(struct.pack('<BI', 0xFE, int(end_of_track * RATE)))
    print(f"ends at {end_of_track}")
