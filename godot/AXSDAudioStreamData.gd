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
#   A sequencer data entry for an AXSDAudioStreamPlayer
#
#   If you want to sequence, there should be a MIDI-to-AXSD converter in the
#   repository.
extends Resource

class_name AXSDAudioStreamData

const VERSION = 0x0002
const MAGIC = 0x44535841

var has_song = false
var rate: int
var position: int = 0
var tick: float = 0.0

@export var path: String:
	set(epath):
		self.has_song = false
		self.position = 0
		self.tick = 0.0
		var file = FileAccess.open(epath, FileAccess.READ)
		file.big_endian = false
		var loaded: Array[Event] = []
		var running = true
		var emagic = file.get_32()
		if emagic != MAGIC:
			print_debug("Magic header didn't match: %x" % emagic)
			return
		var eversion_op = file.get_8()
		if eversion_op != EventType.VERSION:
			print_debug("No or improperly placed version info")
			return
		var eversion = file.get_16()
		if eversion != VERSION:
			print_debug("Version didn't match: %x" % eversion)
			return
		var erate_op = file.get_8()
		if erate_op != EventType.REFRESH_RATE:
			print_debug("No or improperly placed rate info")
			return
		self.rate = file.get_32()
		while running and not file.eof_reached():
			var op = file.get_8()
			match op:
				0x00:
					print_debug("Ignoring zero no-op in AXSD VM")
				EventType.NOTE_ON:
					var etick = file.get_32()
					var channel = file.get_8()
					var note = file.get_8()
					var velocity = file.get_8()
					loaded.append(EventNoteOn.new(etick, channel, note, velocity))
				EventType.NOTE_OFF:
					var etick = file.get_32()
					var channel = file.get_8()
					loaded.append(EventNoteOff.new(etick, channel))
				EventType.PITCHWHEEL:
					var etick = file.get_32()
					var channel = file.get_8()
					var pitch = file.get_32()
					loaded.append(EventPitchwheel.new(etick, channel, pitch))
				EventType.INSTRUMENT_CHANGE:
					var etick = file.get_32()
					var channel = file.get_8()
					var program = file.get_8()
					loaded.append(EventInstrumentChange.new(etick, channel, program))
				EventType.END_OF_MUSIC:
					var etick = file.get_32()
					loaded.append(EventEndOfMusic.new(etick))
					running = false
				_:
					print_debug("Invalid AXSD VM opcode %x" % op)
					running = false
		
		self.events = loaded
		self.events.sort_custom(func(lhs, rhs): return lhs.time < rhs.time)
		self.has_song = true

var events: Array[Event]

func run_tick(delta: float, rewind: bool = false) -> Array[Event]:
	var these_events: Array[Event] = []
	if self.has_song:
		if rewind:
			self.position = 0
			self.tick = 0.0
		while self.position < self.events.size():
			var this_event = self.events[self.position]
			if this_event.time < self.tick:
				these_events.append(this_event)
				self.position += 1
			else:
				break
		self.tick += self.rate * delta
	return these_events

enum EventType {
	NOTE_ON = 0x01,
	NOTE_OFF = 0x02,
	PITCHWHEEL = 0x03,
	INSTRUMENT_CHANGE = 0x04,
	VERSION = 0xFC,
	REFRESH_RATE = 0xFD,
	END_OF_MUSIC = 0xFE,
	MAX = 0xFF
}

class Event:
	var type: EventType
	var time: int

class EventNoteOn extends Event:
	var channel: int
	var note: int
	var velocity: int
	func _init(etime: int, echannel: int, enote: int, evelocity: int):
		self.type = EventType.NOTE_ON
		self.time = etime
		self.channel = echannel
		self.note = enote
		self.velocity = evelocity
		
class EventNoteOff extends Event:
	var channel: int
	func _init(etime: int, echannel: int):
		self.type = EventType.NOTE_OFF
		self.time = etime
		self.channel = echannel

class EventPitchwheel extends Event:
	var channel: int
	var pitch: int
	func _init(etime: int, echannel: int, epitch: int):
		self.type = EventType.PITCHWHEEL
		self.time = etime
		self.channel = echannel
		self.pitch = epitch

class EventEndOfMusic extends Event:
	func _init(etime: int):
		self.type = EventType.END_OF_MUSIC
		self.time = etime

class EventInstrumentChange extends Event:
	var channel: int
	var program: int
	func _init(etime: int, echannel: int, eprogram: int):
		self.type = EventType.INSTRUMENT_CHANGE
		self.time = etime
		self.channel = echannel
		self.program = eprogram
