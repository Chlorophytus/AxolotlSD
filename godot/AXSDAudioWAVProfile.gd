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
#   A wavetable entry for an AXSDAudioStreamPlayer
extends Resource

class_name AXSDAudioWAVProfile

@export var data: AudioStreamWAV

@export var envelope: Curve
@export var sustain_at: float

@export var pitch_multiplier: float = 1.0
@export var vibrato_frequency: float = 0.0
@export var vibrato_intensity: float = 0.0

@export var volume: float = 1.0
@export var a440: int = 69
