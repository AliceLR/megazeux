## Reality Adlib Tracker Player

This is a modified version of the RAD 2.x player for MegaZeux. Included:

* RAD 1.x playback and validation support.
* New functions required for MegaZeux's set_order/set_position and get_length.
* A bugfix for Dxx. The vanilla player fails to skip directly to the parameter
  row and instead will play the next pattern extremely fast until the parameter
  row is reached.
* A bugfix for BPM validation. The vanilla validater incorrectly checks bit 6
  when the BPM-present flag is bit 5 in both the player and the documentation.
* A bugfix for MIDI instrument validation in the player. A MIDI instrument is 7
  bytes long including the first algorithm byte; the final byte is a "volume"
  byte that appears in the tracker but is not present in the documentation.
  The vanilla validator only skips 6 bytes.
* A bugfix for MIDI instrument processing in the player. The vanilla player
  will ignore processing an entire note when it encounters a line that would
  play a MIDI instrument, causing it to ignore effects and riffs.
* Some -pedantic warning fixes.

As the RAD player code is public domain, so are these modifications. If you find this
useful, go ahead and use it.

Please visit the Reality website for the latest version of the RAD tracker, the
original version of this playback code, and .RAD music releases by Reality:
<https://www.3eality.com/productions/reality-adlib-tracker>.

Thanks to Reality for the original software and to zzo38 for finding and
reporting several of these bugs.

-Lachesis
