/* MegaZeux
 *
 * Copyright (C) 2009 Alistair John Strachan <alistair@devzero.co.uk>
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

#include "sfwrapper.h"

#include <ui/Rect.h>
#include <ui/Surface.h>
#include <ui/PixelFormat.h>
#include <ui/DisplayInfo.h>
#include <ui/SurfaceComposerClient.h>
#include <ui/EGLNativeWindowSurface.h>

// Android has a util.h too
#include "../../src/util.h"

namespace android {

static sp<SurfaceComposerClient>  mSession;
static sp<Surface>                mFlingerSurface;
static sp<EGLNativeWindowSurface> mNativeWindowSurface;

extern "C" bool SurfaceFlingerInitialize(int target_width, int target_height,
 int depth, bool fullscreen)
{
  if(mSession == NULL)
  {
    mSession = new SurfaceComposerClient();
    if(mSession == NULL)
    {
      warn("Failed to create SurfaceFlinger session\n");
      return false;
    }
  }

  if(mFlingerSurface == NULL)
  {
    DisplayInfo dinfo;
    status_t status;
    int pixelFormat;

    status = mSession->getDisplayInfo(0, &dinfo);
    if(status)
    {
      warn("Failed to obtain display information\n");
      return false;
    }

    if((unsigned int)target_width > dinfo.w)
      target_width = dinfo.w;

    if((unsigned int)target_height > dinfo.h)
      target_height = dinfo.h;

    if(depth == 32)
      pixelFormat = PIXEL_FORMAT_RGBA_8888;
    else
      pixelFormat = PIXEL_FORMAT_RGB_565;

    mFlingerSurface = mSession->createSurface(getpid(), 0,
     target_width, target_height, pixelFormat);
    if(mFlingerSurface == NULL)
    {
      warn("Failed to create native window surface\n");
      return false;
    }

    debug("Asked SurfaceFlinger for a %dx%d (format %d) surface\n",
          target_width, target_height, pixelFormat);

    if(fullscreen)
    {
      mSession->openTransaction();
      mFlingerSurface->setLayer(0x40000000);
      mSession->closeTransaction();
    }
  }

  if(mNativeWindowSurface == NULL)
    mNativeWindowSurface = new EGLNativeWindowSurface(mFlingerSurface);

  return true;
}

extern "C" void SurfaceFlingerDeinitialize(void)
{
  if(mNativeWindowSurface != NULL)
  {
    mNativeWindowSurface.clear();
    mNativeWindowSurface = NULL;
  }

  if(mFlingerSurface != NULL)
  {
    // FIXME: Should de-initialize properly
  }

  if(mSession != NULL)
  {
    // FIXME: Should de-initialize properly
  }
}

extern "C" EGLNativeWindowType SurfaceFlingerGetNativeWindow(void)
{
  assert(mNativeWindowSurface != NULL);
  return mNativeWindowSurface.get();
}

extern "C" void SurfaceFlingerSetSwapRectangle(int x, int y, int w, int h)
{
  assert(mNativeWindowSurface != NULL);
  const android::Rect r(x, y, w, h);
  debug("Set swap rectangle (%d,%d) [%d,%d]\n", r.left, r.top, r.width(), r.height());
  mNativeWindowSurface->setSwapRectangle(r.left, r.top, r.width(), r.height());
}

} // namespace android
