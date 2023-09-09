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
#   Audio player for AxolotlSD files
extends AudioStreamPlayer

@export var audio_mappings: Dictionary # anything not channel 10
@export var drum_mappings: Dictionary # anything that's channel 10

@export var audio_profiles: Array[AXSDAudioWAVProfile]
@export var drum_profiles: Array[AXSDAudioWAVProfile]

@export var current_song: AXSDAudioStreamData

@export var sequencing: bool = true

var ticks = 0.0
var playback: AudioStreamPlaybackPolyphonic
var poly_stream: AudioStreamPolyphonic

class Voice:
	class Assignment:
		var pitch: float = 0.0
		var id = AudioStreamPlaybackPolyphonic.INVALID_ID
		var vibrato_current_offset = 0.0
		var key: bool = false
		var amplitude: float = 0.0
		var velocity: float = 0.0
		var seconds: float = 0.0


	var profile: AXSDAudioWAVProfile = null
	var assignments: Array[Assignment] = []

	func _init(vprofile: AXSDAudioWAVProfile):
		self.profile = vprofile

	func tick(playback: AudioStreamPlaybackPolyphonic, delta: float):
		# Loop through assignments
		for i in range(self.assignments.size()):
			# This assignment
			var assignment = self.assignments[i]
			
			# Determine assignment hold seconds...
			if assignment.key:
				if assignment.seconds < self.profile.sustain_at:
					assignment.seconds += delta
			else:
				assignment.seconds += delta

			# ... then determine the amplitude based on the ADSR envelope
			if assignment.seconds > self.profile.envelope.get_point_position(self.profile.envelope.get_point_count() - 1).x:
				# Stop this stream, it's exceeded its envelope end
				playback.stop_stream(assignment.id)
				assignment.id = AudioStreamPlaybackPolyphonic.INVALID_ID
			else:
				# Continue this stream, it hasn't ended its envelope
				assignment.amplitude = self.profile.envelope.sample(assignment.seconds)
				# Handle vibrato
				assignment.vibrato_current_offset += delta * self.profile.vibrato_frequency * PI
				assignment.vibrato_current_offset = fmod(assignment.vibrato_current_offset, TAU)

				# Handle final loudness and pitch
				var decibels = linear_to_db(clamp(assignment.amplitude * assignment.velocity * profile.volume, 0.0, 1.0))
				playback.set_stream_pitch_scale(assignment.id, assignment.pitch + (cos(assignment.vibrato_current_offset) * self.profile.vibrato_intensity))
				playback.set_stream_volume(assignment.id, decibels)

		# Remove all assignments that have released
		self.assignments = self.assignments.filter(func(assign):
			return assign.id != AudioStreamPlaybackPolyphonic.INVALID_ID
		)

	func on(playback: AudioStreamPlaybackPolyphonic, note: int, velocity: float):
		var assignment = Assignment.new()
		assignment.velocity = velocity
		assignment.pitch = self.profile.pitch_multiplier * pow(pow(2.0, 1.0 / 12.0), note - self.profile.a440)
		assignment.key = true
		assignment.id = playback.play_stream(self.profile.data, 0, -INF, assignment.pitch)
		if assignment.id != AudioStreamPlaybackPolyphonic.INVALID_ID:
			self.assignments.append(assignment)
		else:
			print_debug("invalid voice on")

	func off():
		var size = self.assignments.size()
		for i in range(size):
			var assignment = self.assignments[i]
			if assignment.key:
				assignment.key = false
				break


var voices: Array[Voice]

# Called when the node enters the scene tree for the first time.
func _ready():
	self.poly_stream = self.get_stream()
	self.playback = self.get_stream_playback()
	_voices_init()

func _voices_panic():
	for voice in self.voices:
		voice.off()
		for assignment in voice.assignments:
			assignment.key = false
			self.playback.stop_stream(assignment.id)
			assignment.id = false
		voice.assignments = []

func _voices_init():
	self.voices = []
	for i in range(16):
		self.voices.append(Voice.new(self.audio_profiles[0]))

# Called every frame. 'delta' is the elapsed time since the previous frame.
func _process(delta):
	if self.sequencing:
		var events = self.current_song.run_tick(delta)
		for e in events:
			match e.type:
				AXSDAudioStreamData.EventType.INSTRUMENT_CHANGE:
					var id = self.audio_mappings[e.program]
					self.voices[e.channel].profile = self.audio_profiles[id]
				AXSDAudioStreamData.EventType.NOTE_ON:
					if e.channel == 9:
						# Drum notes go to drum mappings
						if e.note in self.drum_mappings.keys():
							var drum_id = self.drum_mappings[e.note]
							self.voices[9].profile = self.drum_profiles[drum_id]
							self.voices[9].on(self.playback, 69, e.velocity / 127.0)
					else:
						# Melody notes go to audio mappings
						self.voices[e.channel].on(self.playback, e.note, e.velocity / 127.0)
				AXSDAudioStreamData.EventType.NOTE_OFF:
					self.voices[e.channel].off()
				AXSDAudioStreamData.EventType.END_OF_MUSIC:
					self.current_song.run_tick(delta, true)
		for voice in self.voices:
			voice.tick(self.playback, delta)

		ticks += delta
	else:
		_voices_panic()
		
