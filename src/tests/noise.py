import numpy as np
from scipy.io import wavfile

# Parameters
duration_seconds = 5
sample_rate = 44100
num_samples = duration_seconds * sample_rate

# Generate white noise in range [-1.0, 1.0]
noise = np.random.uniform(low=-1.0, high=1.0, size=num_samples)

# Scale to 16-bit and convert
audio_int16 = (noise * 32767).astype(np.int16)

# Write to WAV file
wavfile.write("white_noise.wav", sample_rate, audio_int16)

print("Saved white_noise.wav")
