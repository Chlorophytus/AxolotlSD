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

@export var audios: Array[AXSDAudioWAVProfile]
@export var current_song: AXSDAudioStreamData

var ticks = 0.0
var playback: AudioStreamPlaybackPolyphonic
var poly_stream: AudioStreamPolyphonic

enum ADSRState {
	OFF,
	ATTACK,
	DECAY,
	SUSTAIN,
	RELEASE
}

class Voice:
	class Assignment:
		var pitch: float = 0.0
		var id = AudioStreamPlaybackPolyphonic.INVALID_ID
		var vibrato_current_offset = 0.0
		var key: bool = false
		var amplitude: float = 0.0
		var velocity: float = 0.0
		var state: ADSRState = ADSRState.ATTACK
		
	
	var profile: AXSDAudioWAVProfile = null
	var assignments: Array[Assignment] = []
	
	func _init(vprofile: AXSDAudioWAVProfile):
		self.profile = vprofile
	
	func tick(playback: AudioStreamPlaybackPolyphonic, delta: float):
		for i in range(self.assignments.size()):
			var assignment = self.assignments[i]
			if assignment.key:
				match assignment.state:
					ADSRState.OFF:
						print_debug("what, off state can't be encountered here")
					ADSRState.ATTACK:
						assignment.amplitude += self.profile.attack * delta
						if assignment.amplitude > 1.0:
							assignment.state = ADSRState.DECAY
							assignment.amplitude = 1.0
					ADSRState.DECAY:
						assignment.amplitude -= self.profile.decay * delta
						if assignment.amplitude < self.profile.sustain:
							assignment.state = ADSRState.SUSTAIN
							assignment.amplitude = self.profile.sustain
					ADSRState.SUSTAIN:
						assignment.amplitude = self.profile.sustain
					ADSRState.RELEASE:
						assignment.state = ADSRState.ATTACK
			else:
				match assignment.state:
					ADSRState.OFF:
						playback.stop_stream(assignment.id)
					ADSRState.ATTACK:
						assignment.amplitude -= self.profile.release * delta
						if assignment.amplitude < 0.0:
							playback.stop_stream(assignment.id)
							assignment.state = ADSRState.OFF
					ADSRState.DECAY:
						assignment.amplitude -= self.profile.release * delta
						if assignment.amplitude < 0.0:
							playback.stop_stream(assignment.id)
							assignment.state = ADSRState.OFF
					ADSRState.SUSTAIN:
						assignment.amplitude -= self.profile.release * delta
						if assignment.amplitude < 0.0:
							playback.stop_stream(assignment.id)
							assignment.state = ADSRState.OFF
					ADSRState.RELEASE:
						assignment.amplitude -= self.profile.release * delta
						if assignment.amplitude < 0.0:
							playback.stop_stream(assignment.id)
							assignment.state = ADSRState.OFF
			assignment.vibrato_current_offset += delta * self.profile.vibrato_frequency * PI
			assignment.vibrato_current_offset = fmod(assignment.vibrato_current_offset, TAU)
			var decibels = linear_to_db(clamp(assignment.amplitude * assignment.velocity * profile.volume, 0.0, 1.0))
			playback.set_stream_pitch_scale(assignment.id, assignment.pitch + (cos(assignment.vibrato_current_offset) * self.profile.vibrato_intensity))
			playback.set_stream_volume(assignment.id, decibels)

		self.assignments = self.assignments.filter(func(assign):
			return assign.state != ADSRState.OFF
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
	pass

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
	for profile in self.audios:
		self.voices.append(Voice.new(profile))

# Called every frame. 'delta' is the elapsed time since the previous frame.
func _process(delta):
	if self.get_meta("Sequencing"):
		if not self.playing:
			self.play()
			self.poly_stream = self.get_stream()
			self.playback = self.get_stream_playback()
			_voices_init()

		var events = self.current_song.run_tick(delta)
		for e in events:
			match e.type:
				AXSDAudioStreamData.EventType.NOTE_ON:
					self.voices[e.channel].on(self.playback, e.note, e.velocity / 127.0)
				AXSDAudioStreamData.EventType.NOTE_OFF:
					self.voices[e.channel].off()
				AXSDAudioStreamData.EventType.END_OF_MUSIC:
					self.current_song.run_tick(delta, true)
		for voice in self.voices:
			voice.tick(self.playback, delta)
		
		ticks += delta
