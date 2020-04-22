/* MegaZeux
 *
 * Copyright (C) 2019 Adrian Siekierka <kontakt@asie.pl>
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

package net.digitalmzx.megazeux;

import android.Manifest;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Bundle;
import android.app.Activity;
import android.view.View;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOError;
import java.io.IOException;
import java.io.InputStream;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

public class MainActivity extends Activity
{
  @Override
  protected void onCreate(Bundle savedInstanceState)
  {
    super.onCreate(savedInstanceState);

    if(Build.VERSION.SDK_INT >= 23)
    {
      if(checkSelfPermission(android.Manifest.permission.WRITE_EXTERNAL_STORAGE)
       != PackageManager.PERMISSION_GRANTED)
      {
        requestPermissions(new String[]{Manifest.permission.WRITE_EXTERNAL_STORAGE}, 1000);
      }
      else
        launchGame();
    }
  }

  private void launchGame()
  {
    // unzip assets
    File targetPath = GameActivity.getAssetPath(this);
    InputStream assetStream = getResources().openRawResource(R.raw.assets);
    ZipInputStream assetZip = new ZipInputStream(assetStream);
    byte[] buffer = new byte[8192];
    FileOutputStream output = null;

    try
    {
      ZipEntry entry;
      while((entry = assetZip.getNextEntry()) != null)
      {
        File target = new File(targetPath, entry.getName());

        if(entry.isDirectory())
        {
          if(target.exists())
            continue;

          target.mkdirs();
        }
        else
        {
          if(target.exists())
          {
            if(target.lastModified() == entry.getTime())
              continue;
          }

          int read;
          output = new FileOutputStream(target);

          while((read = assetZip.read(buffer, 0, buffer.length)) >= 0)
          {
            output.write(buffer, 0, read);
          }

          output.close();
          output = null;

          target.setLastModified(entry.getTime());
        }

        assetZip.closeEntry();
      }
    }
    catch(IOException e)
    {
      throw new RuntimeException(e);
    }
    finally
    {
      if(output != null)
      {
        try
        {
          output.close();
        }
        catch(IOException e)
        {
          // pass
        }
      }

      try
      {
        assetZip.close();
      }
      catch(IOException e)
      {
        // pass
      }

      try
      {
        assetStream.close();
      }
      catch(IOException e)
      {
        // pass
      }
    }

    // launch game
    startActivity(new Intent(getApplicationContext(), GameActivity.class));
  }

  @Override
  public void onRequestPermissionsResult(int requestCode, String[] permissions,
   int[] grantResults)
  {
    super.onRequestPermissionsResult(requestCode, permissions, grantResults);
    if(grantResults.length > 0 &&
     grantResults[0] == PackageManager.PERMISSION_GRANTED)
    {
      launchGame();
    }
  }
}
