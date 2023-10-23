# AxolotlSD

AxolotlSD Sound Driver for C++20.

## `cxx`

This is the library itself. `axolotlsd` is a shared library, `axolotlsd_s` is a static one.

### Example usage

#### Creating an AxolotlSD player

Create an AxolotlSD player with 32 maximum voice polyphony, 44100Hz sample rate, and stereo sound.

```cxx
auto player = axolotlsd::player{32, 44100, true};
```

#### Loading and playing an AxolotlSD song

Load a song and play it back.

```cxx
player.play(axolotlsd::song::load(song_bytes),
            axolotlsd::environment{.feedback_L = 0.85f,
                                   .feedback_R = 0.85f,
                                   .wet_L = -0.33f,
                                   .wet_R = 0.33f,
                                   .cursor_increment = 0x01,
                                   .cursor_max = 0x1800});
```

`song_bytes` is a `std::vector` with unsigned bytes.
Load your `.axsd` sequence's bytecode into the vector and it should work.

The `axolotlsd::environment` is optional, but it provides a profile for the echo buffer.
When not using the environment/echo buffer, pass as `std::nullopt`.

### Note: Statically including a song with `xxd -i`

If you have `xxd`, you could export a song's bytecode with `xxd -i <your sequence here>.axsd > out.h`.
Use `axolotlsd::song::load_xxd_format` instead of `axolotlsd::song::load`.

## `cxx_test`

A raylib test player, linked dynamically with AxolotlSD.
The player `CMakeLists.txt` can be modified to run a static AxolotlSD.

## `export`

This is used to export AxolotlSD sequencer dumps.

### Going into venv then using pip

```shell
$ cd export
$ python3 -m venv .
$ . ./bin/activate
$ pip install -r requirements.txt
$ ./export.py input.mid output.axsd sample_pack/
```

### Details

This is a Python 3 script that handles exporting MIDI note data to AxolotlSD sequencer note data.
- The first argument is an input MIDI file.
- The second argument is the file to export to.
- The third argument is a directory with drums, patches, and a sample pack JSON description.

You can hex edit `Funk.axsd`, and compare it to `Funk.mid` which is a General MIDI song that was used for testing.
