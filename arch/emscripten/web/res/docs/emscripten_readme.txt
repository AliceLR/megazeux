MegaZeux HTML5 port: (very) rough instructions

index.html provides an effective template of a MegaZeux instance which will span the whole browser frame.

Basic requirements:

* A canvas object specified in options.render.canvas.
* A presence of all the files herein other than "index.html" in a certain directory.

The entrypoint is "MzxrunLoad(options);".

Mandatory option keys:

* render.canvas: the element of the desired canvas.
* path: the (relative or absolute) path to the MegaZeux engine files.
* files: an array containing file entries to be loaded.

Optional option keys:

* storage: define the settings for persistent storage. If not present, save files etc. will be stored in memory, and lost with as little as a page refresh.
    * type: can be "auto" (preferred), "localstorage" or "indexeddb".
    * database: for all of the above, a required database name. If you're hosting multiple games on the same domain, you may want to make this unique.
* config: a string which will be appended to "config.txt".

File entries can be either a string (denoting the relative or absolute path to a .ZIP file), or an array of a string and an options object containing the following optional keys:

* filenameFilter: function which returns true if a given filename should be accepted. This runs after filenameMap.
* filenameMap: filename mapping - can be in one of three forms:
    * string - describes the subdirectory the ZIP's data is loaded to
    * object - describes a mapping of ZIP filenames to target filesystem filenames; no other files are loaded
    * function - accepts a ZIP filename and returns a target filesystem filename; return "undefined" to not load a file
