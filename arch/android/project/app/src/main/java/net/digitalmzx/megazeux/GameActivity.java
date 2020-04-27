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

import android.app.Activity;
import android.os.Build;
import android.os.Environment;
import android.view.View;

import org.libsdl.app.SDLActivity;

import java.io.File;

public class GameActivity extends SDLActivity
{
  static File getAssetPath(Activity activity)
  {
    File extDir = activity.getExternalFilesDir(null);
    if(extDir == null)
    {
      extDir = new File(Environment.getExternalStorageDirectory(), "data/megazeux");
      if(!extDir.exists())
      {
        //noinspection StatementWithEmptyBody
        if(!extDir.mkdirs())
        {
          // pass
        }
      }
    }

    return extDir;
  }

  @Override
  protected String[] getArguments()
  {
    return new String[] { new File(getAssetPath(this), "megazeux").getAbsolutePath() };
  }
}
