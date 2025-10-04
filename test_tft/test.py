import os
import time
import matplotlib.pyplot as plt
from PIL import Image

# Base folder containing all animations
base_folder = "./anime_smile_loop_frames_320x254"
animations = [
    # "blink",
    # "eyes_lr",
    # "surprised",
    # "angry",
    # "love",
    # "idle",
    # "talk",
    # "sleep",
    # "shock",
    # "wave"
    "bmp_frames_time10_cropped"
]

# Load frames for each animation
frames_dict = {}
for anim in animations:
    folder = os.path.join(base_folder, anim)
    frames = sorted([f for f in os.listdir(folder) if f.lower().endswith(".bmp")])
    frames_dict[anim] = [Image.open(os.path.join(folder, f)) for f in frames]

# Setup figure with 1 row x 5 columns
plt.ion()
fig, axes = plt.subplots(1, len(animations), figsize=(12, 3))

# Ensure axes is iterable even if one subplot
if len(animations) == 1:
    axes = [axes]

while True:
    for i in range(len(frames_dict[animations[0]])):  # assume equal frame count
        for ax, anim in zip(axes, animations):
            ax.clear()
            ax.imshow(frames_dict[anim][i])
            ax.set_title(anim)
            ax.axis("off")
        plt.draw()
        plt.pause(0.2)

plt.ioff()
plt.show()
