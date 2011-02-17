#include "videosource.h"
#ifdef HAVE_DC1394

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <inttypes.h>

using namespace std;

namespace pangolin
{

void FirewireVideo::init_camera(
  uint64_t guid, int dma_frames,
  dc1394speed_t iso_speed,
  dc1394video_mode_t video_mode,
  dc1394framerate_t framerate
) {
  camera = dc1394_camera_new (d, guid);
  if (!camera)
    throw VideoException("Failed to initialize camera");

  dc1394_camera_free_list (list);

  cout << "Using camera with GUID " << camera->guid << endl;

  /*-----------------------------------------------------------------------
   *  setup capture
   *-----------------------------------------------------------------------*/

  err=dc1394_video_set_iso_speed(camera, iso_speed);
  if( err != DC1394_SUCCESS )
    throw VideoException("Could not set iso speed");

  err=dc1394_video_set_mode(camera, video_mode);
  if( err != DC1394_SUCCESS )
    throw VideoException("Could not set video mode");

  err=dc1394_video_set_framerate(camera, framerate);
  if( err != DC1394_SUCCESS )
    throw VideoException("Could not set framerate");

  err=dc1394_capture_setup(camera,dma_frames, DC1394_CAPTURE_FLAGS_DEFAULT);
  if( err != DC1394_SUCCESS )
    throw VideoException("Could not setup camera - check settings");

  /*-----------------------------------------------------------------------
   *  initialise width and height from mode
   *-----------------------------------------------------------------------*/
  dc1394_get_image_size_from_video_mode(camera, video_mode, &width, &height);

  Start();
}

void FirewireVideo::Start()
{
  if( !running )
  {
    err=dc1394_video_set_transmission(camera, DC1394_ON);
    if( err != DC1394_SUCCESS )
      throw VideoException("Could not start camera iso transmission");
    running = true;
  }
}

void FirewireVideo::Stop()
{
  if( running )
  {
    // Stop transmission
    err=dc1394_video_set_transmission(camera,DC1394_OFF);
    if( err != DC1394_SUCCESS )
      throw VideoException("Could not stop the camera");
    running = false;
  }
}

FirewireVideo::FirewireVideo(uint64_t guid)
  :running(false)
{
  d = dc1394_new ();
  if (!d)
    throw VideoException("");

  init_camera(guid,50,DC1394_ISO_SPEED_400,DC1394_VIDEO_MODE_640x480_RGB8,DC1394_FRAMERATE_30);
}

FirewireVideo::FirewireVideo(unsigned deviceid)
  :running(false)
{
  d = dc1394_new ();
  if (!d)
    throw VideoException("");

  err=dc1394_camera_enumerate (d, &list);
  if( err != DC1394_SUCCESS )
    throw VideoException("Failed to enumerate cameras");

  if (list->num == 0)
    throw VideoException("No cameras found");

  if( deviceid >= list->num )
    throw VideoException("Invalid camera index");

  const uint64_t guid = list->ids[deviceid].guid;
  init_camera(guid,50,DC1394_ISO_SPEED_400,DC1394_VIDEO_MODE_640x480_RGB8,DC1394_FRAMERATE_30);
}

bool FirewireVideo::GrabNext( unsigned char* image, bool wait )
{
  const dc1394capture_policy_t policy =
      wait ? DC1394_CAPTURE_POLICY_WAIT : DC1394_CAPTURE_POLICY_POLL;

  dc1394video_frame_t *frame;
  dc1394_capture_dequeue(camera, policy, &frame);
  if( frame )
  {
    memcpy(image,frame->image,frame->image_bytes);
    dc1394_capture_enqueue(camera,frame);
    return true;
  }
  return false;
}

bool FirewireVideo::GrabNewest( unsigned char* image, bool wait )
{
  dc1394video_frame_t *f;
  dc1394_capture_dequeue(camera, DC1394_CAPTURE_POLICY_POLL, &f);

  if( f ) {
    while( true )
    {
      dc1394video_frame_t *nf;
      dc1394_capture_dequeue(camera, DC1394_CAPTURE_POLICY_POLL, &nf);
      if( nf )
      {
        err=dc1394_capture_enqueue(camera,f);
        f = nf;
      }else{
        break;
      }
    }
    memcpy(image,f->image,f->image_bytes);
    err=dc1394_capture_enqueue(camera,f);
    return true;
  }else if(wait){
    return GrabNext(image,true);
  }
  return false;
}

FirewireFrame FirewireVideo::GetNext(bool wait)
{
  const dc1394capture_policy_t policy =
      wait ? DC1394_CAPTURE_POLICY_WAIT : DC1394_CAPTURE_POLICY_POLL;

  dc1394video_frame_t *frame;
  dc1394_capture_dequeue(camera, policy, &frame);
  return FirewireFrame(frame);
}

FirewireFrame FirewireVideo::GetNewest(bool wait)
{
  dc1394video_frame_t *f;
  dc1394_capture_dequeue(camera, DC1394_CAPTURE_POLICY_POLL, &f);

  if( f ) {
    while( true )
    {
      dc1394video_frame_t *nf;
      dc1394_capture_dequeue(camera, DC1394_CAPTURE_POLICY_POLL, &nf);
      if( nf )
      {
        err=dc1394_capture_enqueue(camera,f);
        f = nf;
      }else{
        break;
      }
    }
    return FirewireFrame(f);
  }else if(wait){
    return GetNext(true);
  }
  return FirewireFrame(0);
}

void FirewireVideo::PutFrame(FirewireFrame& f)
{
  if( f.frame )
  {
    dc1394_capture_enqueue(camera,f.frame);
    f.frame = 0;
  }
}


FirewireVideo::~FirewireVideo()
{
  Stop();

  // Close camera
  dc1394_video_set_transmission(camera, DC1394_OFF);
  dc1394_capture_stop(camera);
  dc1394_camera_free(camera);
  dc1394_free (d);
}

}

#endif