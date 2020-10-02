/*!
 * MegaZeux
 *
 * Copyright (C) 2018, 2019 Adrian Siekierka
 * Copyright (C) 2020 Ian Burgmyer <spectere@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

(function() {
	const audioCtx = new (window.AudioContext || window.webkitAudioContext)();
	const audioUnlockEvents = [ "keydown", "mousedown", "touchstart" ];
	let swapSdlContext = false;

	window.replaceSdlAudioContext = function(sdl)
	{
		if(!swapSdlContext || sdl.audioContext.state !== "suspended")
			return;

		// Fetch some data from the original script processor node and create
		// a new one.
		let node = sdl.audio.scriptProcessorNode;
		let buffer = node.bufferSize;
		let channels = node.channelCount;
		let audioProcessFunc = node.onaudioprocess;

		// If any of the values is obviously invalid, abort.
		if(typeof(channels) !== "number" || channels < 1)
			return;

		// Replace the forever-locked context that SDL2 creates in favor
		// of the unlocked one we created earlier.
		sdl.audioContext = audioCtx;

		sdl.audio.scriptProcessorNode =
			sdl.audioContext.createScriptProcessor(buffer, 0, channels);

		// Connect the script processor to the destination and migrate SDL's
		// onaudioprocess event function over so that it can use the new node.
		sdl.audio.scriptProcessorNode.onaudioprocess = audioProcessFunc;
		sdl.audio.scriptProcessorNode.connect(sdl.audioContext.destination);
	}

	function addAudioEventListeners()
	{
		audioUnlockEvents.forEach(
			ev => window.addEventListener(ev, unlockAudioContext, false)
		);
	}

	function removeAudioEventListeners()
	{
		audioUnlockEvents.forEach(
			ev => window.removeEventListener(ev, unlockAudioContext)
		);
	}

	function unlockAudioContext()
	{
		// Safari requires some special handling to unlock the audio context.
		// We start unlocking it here so that the audio will be ready when SDL
		// initializes.

		// Gecko seems to automatically start the audio context very shortly
		// after it's created, so this helps us detect that behavior.
		if(audioCtx.state !== "suspended")
		{
			removeAudioEventListeners();
			return;
		}

		// Create a short buffer, set up the context, and play it.
		console.log("Unlocking audio context");
		var buffer = audioCtx.createBuffer(1, 1, 22050);
		var source = audioCtx.createBufferSource();
		source.buffer = buffer;

		source.connect(audioCtx.destination);
		source.start(0);

		// The newly unlocked context is ready to be swapped in.
		swapSdlContext = true;
		removeAudioEventListeners();
	}

	document.addEventListener("DOMContentLoaded", addAudioEventListeners);
})();

MzxrunLoad = function(options, callback) {
	if (!options.path) throw "Missing option: path!";

	var scripts_array = [];
	var script_ldr = function() {
		if (scripts_array.length == 0) {
			if (callback) {
				callback(MzxrunInitialize(options));
			} else {
				MzxrunInitialize(options);
			}
		} else {
			var scrSrc = scripts_array.shift();
			var scr = document.createElement("script");
			scr.onload = script_ldr;
			scr.src = scrSrc;
			document.body.appendChild(scr);
		}
	}

	scripts_array = [
		options.path+"uzip.min.js",
		options.path+"emzip.js",
		options.path+"mzxrun.js",
		options.path+"mzxrun_web.js"
	];

	script_ldr();
}
