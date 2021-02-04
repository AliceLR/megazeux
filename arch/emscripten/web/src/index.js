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

import { getIndexedDB, getLocalStorage, drawErrorMessage } from "./util.js";
import { createInMemoryStorage, createBrowserBackedStorage, wrapAsyncStorage, createIndexedDbBackedAsyncStorage, createCompositeStorage, createZipStorage } from "./storage.js";
import { wrapStorageForEmscripten } from "./storage_emscripten.js";
import { zip } from "./zip.js";

class LoadingScreen {
    constructor(canvas, ctx, options) {
        this.canvas = canvas;
        this.ctx = ctx;
        this.loaded = false;
        this.path = options.path;

        const self = this;
    }

    _drawBackground() {
        const self = this;
        return new Promise((resolve, reject) => {
            const loadingImage = new Image();
            loadingImage.onload = function() {
                const w = loadingImage.width;
                const h = loadingImage.height;
                self.ctx.drawImage(loadingImage,0,0,w,h,(self.canvas.width - w)/2,(self.canvas.height - h)/2,w,h);

                self.loaded = true;
                resolve(true);
            };
            loadingImage.src = self.path + "loading.png";
        });
    }

    progress(p) {
        if (!this.loaded) return;

        const canvas = this.canvas;
        const ctx = this.ctx;
        const cx = (canvas.width - 640) / 2;
        const cy = (canvas.height - 350) / 2;

        ctx.fillStyle = "#ff0000";
        ctx.fillRect(cx + 14*2, cy + 112*2, p * 292*2, 20);
    }
}

/**
 * Initialize the MegaZeux frontend and run MegaZeux.
 * @param {Object} options Options to configure the frontend and MegaZeux.
 * @returns {Promise} Promise to run MegaZeux.
 */
window.MzxrunInitialize = function(options)
{
    console.log("Initializing MegaZeux web frontend");

    if (!options.render) throw "Missing option: render!";
    if (!options.render.canvas) throw "Missing option: render.canvas!";

    let canvas = options.render.canvas;
    canvas.contentEditable = true;
    let ctx = canvas.getContext('2d', {alpha: false});
    ctx.imageSmoothingEnabled = false;

    canvas.addEventListener("webglcontextlost", e => {
        drawErrorMessage(canvas, ctx, 'Error: WebGL context lost!');
        e.preventDefault();
    }, false);

    /* Disable the default functions for several common MegaZeux shortcuts.
     * FIXME: Opera defaults for left alt and F3 can't be disabled this way (as of 63).
     */
    document.body.addEventListener('keydown', event => {
        let key = event.key.toUpperCase();
        if ((event.altKey && (
                   key == 'C' // Select char
                || key == 'D' // Delete file/directory
                || key == 'N' // New directory
                || key == 'R' // Rename file/directory
            ))
            || key == 'F1'  // Help
            || key == 'F2'  // Settings
            || key == 'F3'  // Save, Load World
            || key == 'F4'  // Load save
            || key == 'F9'  // Quicksave
            || key == 'F10' // Quickload Save

        ) event.preventDefault()
    })

    try {
        if (!options.path) throw "Missing option: path!";
        if (!options.files) throw "Missing option: files!";
    } catch (e) {
        drawErrorMessage(canvas, ctx, e);
        return Promise.reject(e);
    }

    const loadingScreen = new LoadingScreen(canvas, ctx, options);

    var vfsPromises = [];
    var vfsProgresses = [];
    var vfsObjects = [];

  return zip.initialize().then(_ =>
  {
    for (var s in options.files) {
        vfsProgresses.push(0);
        const file = options.files[s];
        const i = vfsProgresses.length - 1;
        const progressUpdater = function(p) {
            vfsProgresses[i] = Math.min(p, 1);
            loadingScreen.progress(vfsProgresses.reduce((p, c) => p + c) / options.files.length);
        }

        if (Array.isArray(file)) {
            var opts = file[1];
            if (typeof(opts) == "string") {
                opts = {filenameMap: opts};
            }
            if (opts && !opts.hasOwnProperty("readonly")) {
                opts.readonly = true;
            }
            vfsPromises.push(
                createZipStorage(file[0], opts, progressUpdater)
                    .then(o => vfsObjects.push(o))
            );
        } else {
            vfsPromises.push(
                createZipStorage(file, {"readonly": true}, progressUpdater)
                    .then(o => vfsObjects.push(o))
            );
        }
    }
  }).then(_ => loadingScreen._drawBackground()).then(_ => Promise.all(vfsPromises)).then(_ =>
  {
        // add MegaZeux config.txt vfs
        // Set startup_path first so user config will override it...
        var configString = "startup_path = /data/game\n";

        configString += options.config + "\n";

        configString += "audio_sample_rate = 48000\n";
        if (navigator.userAgent.toLowerCase().indexOf('firefox') >= 0) {
                // Firefox copes better with audio buffering, based on current testing.
                configString += "audio_buffer_samples = 2048\n";
        } else {
                configString += "audio_buffer_samples = 4096\n";
        }

        vfsObjects.push(createInMemoryStorage({"etc/config.txt": new TextEncoder().encode(configString)}));

        // add storage vfs
        if (options.storage && options.storage.type == "auto") {
            if (getIndexedDB() != null) {
                options.storage.type = "indexeddb";
            } else if (getLocalStorage() != null) {
                options.storage.type = "localstorage";
            } else {
                console.log("Browser does not support any form of local storage! Storing to memory...");
                options.storage = undefined;
            }
        }

        function initMemoryStorage() {
            console.log("Using memory storage. FILES WILL NOT PERSIST BETWEEN SESSIONS!");
            vfsObjects.push(createInMemoryStorage({}, {"readonly": false}));
            return true;
        }

        function initIndexedDBStorage() {
            if (!options.storage.database) throw "Missing option: storage.database!";
            return createIndexedDbBackedAsyncStorage("mzx_" + options.storage.database)
                .then(result => wrapAsyncStorage(result))
                .then(result => vfsObjects.push(result))
                .catch(reason => {
                    console.log("Failed to initialize IndexedDB storage: " + reason);
                    initMemoryStorage();
            });
        }

        function initLocalStorage() {
            if (!options.storage.database) throw "Missing option: storage.database!";
            let storageObj = window.localStorage;
            if (options.storage.storage) storageObj = options.storage.storage;
            if (storageObj == undefined) throw "Could not find storage object!";
            vfsObjects.push(createBrowserBackedStorage(storageObj, "mzx_" + options.storage.database));
            return true;
        }

        try {
            if (options.storage && options.storage.type) {
                if (options.storage.type == "indexeddb") {
                    return initIndexedDBStorage();
                } else if (options.storage.type == "localstorage") {
                    return initLocalStorage();
                } else {
                    throw "Unknown storage type: " + options.storage.type;
                }
            }
        } catch(reason) {
            console.log(reason);
        }

        return initMemoryStorage();

    }).then(_ => new Promise((resolve, reject) => {
        const vfs = createCompositeStorage(vfsObjects);
        console.log(vfs);

        const oldCanvas = canvas;

        // after using the canvas in 2D mode, it cannot be used in WebGL mode
        // as such, create a new canvas
        canvas = oldCanvas.cloneNode(true);
        canvas.oncontextmenu = () => { event.preventDefault(); };
        ctx = null;

        oldCanvas.parentNode.replaceChild(canvas, oldCanvas);
        options.render.canvas = canvas;

        Module({
          canvas: options.render.canvas,
          preRun: function(module)
          {
            window.FS = module["FS"];
            FS.mkdir("/data");
            FS.mount(wrapStorageForEmscripten(vfs), null, "/data");
            console.log("Filesystem initialization complete!");
          }
        }).then((module) =>
        {
            // This event listener refocuses MZX when clicked if it's inside of
            // an iframe (e.g. embedded on itch.io). This is necessary to regain
            // keyboard control after focus is lost (though tab can be used too).
            // This needs to be done HERE since something clobbers all event
            // listeners added to the canvas.
            var canvas = options.render.canvas;
            canvas.addEventListener("mousedown", function(){ canvas.focus(); });

            resolve();
        });
    })).then(_ => true).catch(reason => {
        drawErrorMessage(canvas, ctx, reason);
    });
}
