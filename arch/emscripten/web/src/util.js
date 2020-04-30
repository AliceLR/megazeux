/*!
 * MegaZeux
 *
 * Copyright (C) 2018, 2019 Adrian Siekierka
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

let date_s = Date.now();

export function drawErrorMessage(canvas, ctx, text) {
	let fontSize = 32;
	while (fontSize > 8) {
		ctx.font = "bold " + fontSize + "px sans-serif";
		if (ctx.measureText(text).width <= canvas.width - 4) break;
		fontSize--;
	}

	ctx.fillStyle = "rgba(0, 0, 0, 0.75)";
	ctx.fillRect(0, 0, canvas.width, canvas.height);
	ctx.textBaseline = "middle";

	const xOffset = (canvas.width - ctx.measureText(text).width) / 2;
	const yOffset = canvas.height / 2;

	ctx.fillStyle = "#000000";
	ctx.fillText(text, xOffset + 2, yOffset + 2);

	ctx.fillStyle = "#ffffff";
	ctx.fillText(text, xOffset, yOffset);
}

export function time_ms() {
	return Date.now() - date_s;
}

export function getIndexedDB() {
	return window.indexedDB || window.webkitIndexedDB || window.mozIndexedDB;
}

export function getLocalStorage() {
	return window.localStorage;
}

export function xhrFetchAsArrayBuffer(url, progressCallback) {
	return new Promise((resolve, reject) => {
		var xhr = new XMLHttpRequest();
		xhr.open("GET", url, true);
		xhr.overrideMimeType('text/plain; charset=x-user-defined');
		xhr.responseType = "arraybuffer";

		xhr.onprogress = event => {
			if (progressCallback) progressCallback(event.loaded / event.total);
		};

		xhr.onload = event => {
			if (progressCallback) progressCallback(1);
			if (xhr.status != 200) {
				reject("Error downloading " + url + " (" + xhr.status + ")");
				return;
			}
			resolve(xhr);
		};

		xhr.onerror = event => {
			reject("Error downloading " + url + " (XHR)");
		}

		xhr.send();
	});
}
