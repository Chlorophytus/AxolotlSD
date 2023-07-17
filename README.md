# AxolotlSD
AxolotlSD Sound Driver for Godot 4.

## GDScript files in `godot`
These are used in your Godot game.

### AXSDAudioStreamData
A sequencer dump, used to play note data, current breaking version is specified as 16-bit unsigned constant `VERSION`.

When a breaking `VERSION` change is encountered, you need to export the MIDI into a sequencer dump again.

### AXSDAudioStreamPlayer
Plays the `AXSDAudioStreamData` with `AXSDAudioWAVProfile`s. Each sequencer pitch is calculated based on [12 equal temperament][twelve-tet].

### AXSDAudioWAVProfile
Each AXSDAudioWAVProfile is mapped to a MIDI channel. The sequencer exporter may be updated so that the MIDI instruments are mapped rather than their channels.

#### Profile options
- `data`: Godot Wave file data.
- `attack`: [ADSR][adsr] attack in [rise-over-run][slope] format given the "run" is one second.
- `decay`: [ADSR][adsr] decay in [rise-over-run][slope] format given the "run" is one second. You don't need to make it negative.
- `sustain`: Normalized [ADSR][adsr] sustain.
- `release`: [ADSR][adsr] release in [rise-over-run][slope] format given the "run" is one second. You don't need to make it negative.
- `pitch_multiplier`: If a sample is too flat or sharp and setting the A440 doesn't help then use this to multiply the calculated pitch finely.
- `vibrato_frequency`: Vibrato frequency in cycles per second.
- `vibrato_intensity`: Vibrato intensity ratio into the calculated pitch. Set this to a very low number (like 0.005) when using vibrato.
- `volume`: Normalized result volume. Set to 0 to mute the sample.
- `a440`: A440 pitch in integer MIDI note pitch.

## `export/export.py`
Install mido. This is a Python 3 script that handles exporting MIDI note data to AxolotlSD sequencer note data.

- The first argument is an input MIDI file.
- The second argument is the file to export to.

## External links
- [adsr]: https://en.wikipedia.org/wiki/ADSR_envelope
- [slope]: https://en.wikipedia.org/wiki/Slope
- [twelve-tet]: https://en.wikipedia.org/wiki/12_equal_temperament
