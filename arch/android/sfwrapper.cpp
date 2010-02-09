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

#include <binder/ProcessState.h>
#include <binder/IPCThreadState.h>

// Android has a util.h too
#include "../../src/util.h"

extern "C"
{
  /* FIXME: Hack, remove! */
  void *__dso_handle = NULL;
}

namespace android {

static sp<SurfaceComposerClient>  mSession;

static sp<SurfaceControl>         mSurfaceControl;
static sp<Surface>                mSurface;

extern "C" bool SurfaceFlingerInitialize(int target_width, int target_height,
 int depth, bool fullscreen)
{
  if(mSession == NULL)
  {
    ProcessState::self()->startThreadPool();
    mSession = new SurfaceComposerClient();
    if(mSession == NULL)
    {
      warn("Failed to create SurfaceFlinger session\n");
      return false;
    }
  }

  if(mSurfaceControl == NULL)
  {
	uint32_t flags = 0;
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

    // FIXME: Don't assume GL renderers
    flags |= ISurfaceComposer::eHardware | ISurfaceComposer::eGPU;

    mSurfaceControl = mSession->createSurface(getpid(), 0,
     target_width, target_height, pixelFormat, flags);
    if(mSurfaceControl == NULL)
    {
      warn("Failed to create surface control\n");
      return false;
    }

    debug("Asked SurfaceFlinger for a %dx%d (format %d) surface\n",
          target_width, target_height, pixelFormat);

    if(fullscreen)
    {
      mSession->openTransaction();
      mSurfaceControl->setLayer(0x40000000);
      mSession->closeTransaction();
    }
  }

  if(mSurface == NULL)
    mSurface = mSurfaceControl->getSurface();

  return true;
}

extern "C" void SurfaceFlingerDeinitialize(void)
{
  if(mSurface != NULL)
  {
    mSurface.clear();
    mSurface = NULL;
  }

  if(mSurfaceControl != NULL)
  {
    mSurfaceControl.clear();
    mSurfaceControl = NULL;
  }

  if(mSession != NULL)
  {
    mSession = NULL;
	IPCThreadState::self()->stopProcess();
  }
}

extern "C" NativeWindowType SurfaceFlingerGetNativeWindow(void)
{
  assert(mSurface != NULL);
  return mSurface.get();
}

extern "C" void SurfaceFlingerSetSwapRectangle(int x, int y, int w, int h)
{
  assert(mSurface != NULL);
  const android::Rect r(x, y, 0 + w, 0 + h);
  mSurface->setSwapRectangle(r);
  debug("Set swap rectangle (%d,%d) [%d,%d]\n", r.left, r.top, r.width(), r.height());
}

} // namespace android
