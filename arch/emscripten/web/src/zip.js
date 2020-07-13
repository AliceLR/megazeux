/*!
 * MegaZeux
 *
 * Copyright (C) 2020 Alice Rowan
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

export var zip =
{
  emzip: undefined,
  emzip_functions:
  [
    "_emzip_open",
    "_emzip_length",
    "_emzip_filename",
    "_emzip_extract",
    "_emzip_skip",
    "_emzip_close",
    "_emzip_malloc",
    "_emzip_free",
    "UTF8ToString"
  ],

  /**
   * Initialize the zip utility.
   * @returns {Promise} Promise to initialize the zip utility.
   * @throws {string} Error string if an error occurred.
   */
  initialize: function()
  {
    if(zip.emzip === undefined)
    {
      // If emzip isn't loaded, silently fail (it might not be needed anyway).
      if(typeof(emzip)!=='function')
        return Promise.resolve();

      return emzip().then(module =>
      {
        zip.emzip = module;
        zip.emzip_functions.forEach(function(fn)
        {
          if(typeof(zip.emzip[fn])!=='function')
          {
            console.error('zip.initialize: typeof(zip.emzip["'+fn+'"]) == ' +
             typeof(zip.emzip[fn]));
            throw "zip.initialize: function emzip." + fn + " not found!";
          }
        });
      });
    }
    else
      throw "zip.initialize: already initialized!";
  },

  /**
   * Extract files from a zip archive.
   * @param {ArrayBuffer} bytes Zip archive to extract.
   * @returns {Object.<string, Uint8Array>}
   *   Object containing {filename => file data} for each file in the archive.
   * @throws {string} Error string if an error occurred.
   */
  extract: function(bytes)
  {
    // Attempt UZIP first since it's lighter on memory usage. If that fails,
    // fall back to emzip (a wrapper for MZX's internal zip handler).
    try
    {
      return UZIP.parse(bytes);
    }
    catch(e)
    {
      console.error('UZIP: ' + e);
      try
      {
        return zip.extract_emzip(bytes);
      }
      catch(e)
      {
        console.error('emzip: ' + e);
      }
    }
    throw "Failed to extract archive.";
  },
  extract_emzip: function(bytes)
  {
    if(zip.emzip === undefined)
      throw "Not initialized!";

    var emzip = zip.emzip;
    var bytes_array = new Uint8Array(bytes);

    var src_len = bytes_array.length;
    var src_ptr = emzip._emzip_malloc(src_len);

    if(!src_ptr)
      throw "Failed to allocate buffer.";

    var src_array = new Uint8Array(emzip.HEAPU8.buffer, src_ptr, src_len);
    src_array.set(bytes_array);

    var zp = emzip._emzip_open(src_ptr, src_len);
    var result = {};

    if(!zp)
      throw "Failed to open archive.";

    while(true)
    {
      var filename_ptr = emzip._emzip_filename(zp);
      if(filename_ptr)
      {
        var filename = emzip.UTF8ToString(filename_ptr);
        emzip._emzip_free(filename_ptr);

        var data_length = emzip._emzip_length(zp);
        if(data_length)
        {
          var data_ptr = emzip._emzip_extract(zp);
          if(data_ptr)
          {
            var data_array = new Uint8Array(emzip.HEAPU8.buffer, data_ptr, data_length);
            var data_output = new Uint8Array(data_length);
            data_output.set(data_array);
            emzip._emzip_free(data_ptr);

            result[filename] = data_output;
            continue;
          }
          else
            throw "Failed to extract file: " + filename;
        }
        else
        {
          emzip._emzip_skip(zp);

          if(filename[filename.length - 1] != '/')
            result[filename] = new Uint8Array(0);

          continue;
        }
      }
      break;
    }

    emzip._emzip_close(zp);
    emzip._emzip_free(src_ptr);
    return result;
  }
};
