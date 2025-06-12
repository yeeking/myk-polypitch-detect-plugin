#!/usr/bin/env python3
# Script to generate test MIDI files, note-list text files, and render to audio using a SoundFont

import os
import subprocess
import mido
from mido import Message, MidiFile, MidiTrack, bpm2tempo, second2tick

# Adjustable parameters
FRAME_LENGTHS = [1.0, 2.0, 5.0]       # in seconds
TOTAL_LENGTH_FACTOR = 2.0              # total MIDI file length = frame_length * factor
TEMPO_BPM = 120                        # playback tempo
TICKS_PER_BEAT = 480                   # standard MIDI resolution
OUTPUT_DIR = 'test_data/output_midis'            # folder to save generated files
SOUND_FONT_FILE = 'test_data/default.sf2'        # path to SoundFont file (.sf2)
SAMPLE_RATE = 44100                    # audio sample rate for rendering

# Instruments and corresponding General MIDI program numbers (0-based)
INSTRUMENTS = {
    'piano': 0,    # Acoustic Grand Piano
    'violin': 40,  # Violin
    'sax': 65      # Alto Sax
}

# Define polyphony settings
POLYPHONY_MAP = {
    'mono': [60],          # Middle C
    'poly': [60, 64, 67]   # C major triad
}

# Define test scenarios
TEST_CASES = [
    {'name': 'within_frame',        'start_factor': 0.1, 'duration_factor': 0.6},
    {'name': 'cross_frame',         'start_factor': 0.8, 'duration_factor': 0.8},
    {'name': 'start_at_frame_start','start_factor': 0.0, 'duration_factor': 0.5},
    {'name': 'end_at_frame_end',    'start_factor': 0.5, 'duration_factor': 0.5},
]


def render_audio(midi_in_path, instrument_name, program_number, sound_font, sample_rate):
    """
    Render a MIDI file to WAV using fluidsynth, injecting a program change for the specified instrument.
    """
    # Create a temporary MIDI with program change at the start
    mid = MidiFile(midi_in_path)
    # Insert program change at time 0 on channel 0
    mid.tracks[0].insert(0, Message('program_change', program=program_number, time=0))
    temp_midi = midi_in_path.replace('.mid', f'_{instrument_name}.mid')
    mid.save(temp_midi)

    # Prepare output audio path
    audio_out = midi_in_path.replace('.mid', f'_{instrument_name}.wav')

    # Call fluidsynth
    subprocess.run([
        'fluidsynth', 
        # '-ni', 
        sound_font,
        temp_midi,
        '-F', audio_out,
        '-r', str(sample_rate)
    ], check=True)
    print(f"Rendered audio: {audio_out}")


def generate_midis(frame_lengths, total_length_factor,
                   tempo_bpm, ticks_per_beat, output_dir,
                   sound_font, instruments, sample_rate):
    """
    Generate MIDI files, note-list text files, and render to audio for each
    combination of frame length, polyphony, test case, and instrument.
    """
    # Ensure output directory exists
    os.makedirs(output_dir, exist_ok=True)

    # Assert SoundFont exists
    if not os.path.isfile(sound_font):
        raise FileNotFoundError(f"SoundFont file not found: {sound_font}")

    tempo = bpm2tempo(tempo_bpm)

    for frame in frame_lengths:
        total_length = frame * total_length_factor

        for poly_label, pitches in POLYPHONY_MAP.items():
            for test in TEST_CASES:
                start_time = frame * test['start_factor']
                duration = frame * test['duration_factor']

                base = f"frame_{int(frame)}s_{poly_label}_{test['name']}"
                midi_path = os.path.join(output_dir, base + '.mid')

                # Create MIDI file
                mid = MidiFile(ticks_per_beat=ticks_per_beat)
                track = MidiTrack()
                mid.tracks.append(track)
                track.append(mido.MetaMessage('set_tempo', tempo=tempo, time=0))

                # Prepare note entries and events
                note_entries = []  # (on_sec, off_sec, note, velocity)
                events = []
                start_tick = second2tick(start_time, ticks_per_beat, tempo)
                duration_tick = second2tick(duration, ticks_per_beat, tempo)

                for note in pitches:
                    velocity = 64
                    note_entries.append((start_time, start_time + duration, note, velocity))
                    events.append({'time': start_tick, 'msg': Message('note_on', note=note, velocity=velocity)})
                    events.append({'time': start_tick + duration_tick, 'msg': Message('note_off', note=note, velocity=velocity)})

                # Sort events and write to track
                events.sort(key=lambda e: e['time'])
                prev_tick = 0
                for e in events:
                    delta = int(e['time'] - prev_tick)
                    msg = e['msg'].copy(time=delta)
                    track.append(msg)
                    prev_tick = e['time']

                # Save MIDI and note-list
                mid.save(midi_path)
                print(f"Generated MIDI: {midi_path}")

                txt_path = os.path.join(output_dir, base + '.txt')
                with open(txt_path, 'w') as txt_file:
                    for on_sec, off_sec, note, vel in note_entries:
                        txt_file.write(f"{on_sec:.6f} : {off_sec:.6f} : {note} : {vel}\n")
                print(f"Generated text: {txt_path}")

                # Render audio for each instrument
                for name, prog in instruments.items():
                    render_audio(midi_path, name, prog, sound_font, sample_rate)


if __name__ == '__main__':
    generate_midis(
        FRAME_LENGTHS,
        TOTAL_LENGTH_FACTOR,
        TEMPO_BPM,
        TICKS_PER_BEAT,
        OUTPUT_DIR,
        SOUND_FONT_FILE,
        INSTRUMENTS,
        SAMPLE_RATE
    )
