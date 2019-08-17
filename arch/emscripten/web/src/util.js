/*!
 * MegaZeux
 *
 * Copyright (C) 2018, 2019 Adrian Siekierka
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
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
