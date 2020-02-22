/*!
 * MegaZeux
 *
 * Copyright (C) 2020 Alice Rowan
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
  initialize: function()
  {
    if(zip.emzip === undefined)
    {
      // If emzip isn't loaded, silently fail (it might not be needed anyway).
      if(typeof(emzip)!=='function')
        return {then:function(cb){return cb();}};

      zip.emzip = emzip();

      zip.emzip_functions.forEach(function(fn)
      {
        if(typeof(zip.emzip[fn])!=='function')
          throw "zip.initialize: function emzip." + fn + " not found!";
      });

      return zip.emzip;
    }
    else
      throw "zip.initialize: already initialized!";
  },
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
