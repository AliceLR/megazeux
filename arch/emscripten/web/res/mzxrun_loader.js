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
		options.path+"mzxrun.js",
		options.path+"mzxrun_web.js"
	];

	script_ldr();
}
