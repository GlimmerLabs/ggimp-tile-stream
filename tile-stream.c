/**
 * tile-stream.c
 *   Simple support for streams of tiles using a slightly simpler
 *   API than that the one built into the GIMP.  Intended primarily
 *   as support for sending data over D-Bus, but potentially usable
 *   for other purposes.
 *
 * Copyright (c) 2013 Samuel A. Rebelsky.  All rights reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the Lesser GNU General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */


// +---------+---------------------------------------------------------
// | Headers |
// +---------+

#include <libgimp/gimp.h>
#include <string.h>             //  For memcpy.

#include "tile-stream.h"


// +-----------+-------------------------------------------------------
// | Constants |
// +-----------+

/**
 * The maximum number of simultaneous tile streams supported.
 */
#define MAX_TILE_STREAMS 16


// +-------+-----------------------------------------------------------
// | Types |
// +-------+

/**
 * Information on a tile iterator.  (While we tend to index tile streams
 * with numbers, we still need the data.
 */
struct TileStream
  {
    int image;
    int drawable;
    int left;
    int top;
    int width;
    int height;
    GimpDrawable *source;
    GimpDrawable *target;
    gpointer iterator;
    GimpPixelRgn source_region;
    GimpPixelRgn target_region;
  };
typedef struct TileStream TileStream;


// +---------+---------------------------------------------------------
// | Globals |
// +---------+

/**
 * All of the currently active tile streams.  (Put in an array so
 * that we can refer to them by number.)
 */
TileStream *streams[MAX_TILE_STREAMS];


// +-----------------+-------------------------------------------------
// | Predeclarations |
// +-----------------+

static void invert_pixels (GimpPixelRgn *rgn);


// +-----------------+-------------------------------------------------
// | Local Utilities |
// +-----------------+

/**
 * Copy pixels into a region.
 */
static int
copy_pixels (GimpPixelRgn *rgn, guchar *data)
{
  int size = rgn->h * rgn->rowstride;
  memcpy (rgn->data, data, size);
} // copy_pixels

static int
copy_pixels_new (GimpPixelRgn *rgn, guchar *data)
{
  guchar *target = rgn->data;
  int r;
  for (r = 0; r < rgn->h; r++)
    {
      int c;
      int width = rgn->w * rgn->bpp;
      for (c = 0; c < width; c++)
        {
          target[c] = data[c];
        } // for c
      data += rgn->rowstride;
      target += rgn->rowstride;
    } // for r
  return 1;
} // copy_pixels

/**
 * Invert pixels in a region.  (Intended mostly for testing.)
 */
static void
invert_pixels (GimpPixelRgn *rgn)
{
  guchar *data = rgn->data;
  int r;
  for (r = 0; r < rgn->h; r++)
    {
      int c;
      for (c = 0; c < rgn->rowstride; c++)
        { 
          data[c] = (guchar) (255 - data[c]);
        } // for c
      data += rgn->rowstride;
    } // for r
} // invert_pixels

/**
 * Get the next available iterator id.
 */
static int
next_iterator_id (void)
{
  int i;
  for (i = 0; i < MAX_TILE_STREAMS; i++)
    {
      if (streams[i] == NULL)
        return i;
    } // for
  return -1;
} // next_iterator_id

static int
valid_stream_id (int id)
{
  return ((id >= 0)
          && (id < MAX_TILE_STREAMS)
          && (streams[id] != NULL));
} // valid_stream_id


// +--------------+----------------------------------------------------
// | Constructors |
// +--------------+

/**
 * Get a read-write tile iterator for a portion of a drawable.
 * Returns -1 if it cannot create the iterator.
 */
int
rectangle_new_tile_stream (int image, int drawable, 
                           int left, int top,
                           int width, int height)
{
  // Get an id to use for the iterator
  int id = next_iterator_id ();
  if (id < 0)
    {
      return id;
    }

  // Allocate space for information on the iterator
  TileStream *stream = (TileStream *) g_malloc0 (sizeof (TileStream));
  if (stream == NULL)
    {
      return -1;
    }

  // Fill in the basic data
  stream->image = image;
  stream->drawable = drawable;
  stream->left = left;
  stream->top = top;
  stream->width = width;
  stream->height = height;
  stream->source = gimp_drawable_get (drawable);
  if (stream->source == NULL)
    {
      g_free (stream);
      return -1;
    } // if we could not get the drawable
  stream->target = gimp_drawable_get (drawable);
  if (stream->target == NULL)
    {
      g_free (stream->source);
      g_free (stream);
      return -1;
    }

  // Fill in the more advanced data
  gimp_pixel_rgn_init (&(stream->source_region), 
                       stream->source,
                       left, top, width, height,
                       FALSE, FALSE);
  gimp_pixel_rgn_init (&(stream->target_region), 
                       stream->target,
                       left, top, width, height,
                       TRUE, TRUE);
  stream->iterator = gimp_pixel_rgns_register (2, &(stream->source_region),
                                                  &(stream->target_region));

  // Copy pixels over in case the user doesn't change them
  copy_pixels (&(stream->target_region), stream->source_region.data);

  // HACK FOR TESTING
  invert_pixels (&(stream->target_region));

  // And we're done
  streams[id] = stream;
  return id;
} // rectangle_new_tile_stream

/**
 * Get a tile stream for a drawable.
 */
int
drawable_new_tile_stream (int image, int drawable)
{
  return rectangle_new_tile_stream (image, drawable,
                                    0, 0, 
                                    gimp_image_width (image),
                                    gimp_image_height (image));
} // drawable_new_tile_stream


// +-----------------+-------------------------------------------------
// | Primary Methods |
// +-----------------+

/**
 * Advance to the next tile.  Returns true if it succeeds and
 * false otherwise.
 */
gboolean
tile_stream_advance (int id)
{
  // Sanity checks
  if (! valid_stream_id (id))
    return 0;

  // Advance the iterator.  This has the side effect of changing
  // streams[id]->source_region and streams[id]->target_region.
  streams[id]->iterator = gimp_pixel_rgns_process (streams[id]->iterator);

  // Copy pixels over in case the user doesn't change them
  if (streams[id]->iterator != NULL)
    {
      copy_pixels (&(streams[id]->target_region),
                   streams[id]->source_region.data);
    } // if we're not at the end

  // Did we succeed?
  return (streams[id]->iterator != NULL);
} //  tile_stream_advance

/**
 * Close the tile stream, writing changes back (I hope).
 */
void
tile_stream_close (int id)
{
  if (! valid_stream_id (id))
    return;
  TileStream *stream = streams[id];
  gimp_drawable_flush (stream->target);
  gimp_drawable_merge_shadow (stream->drawable, TRUE);
  gimp_drawable_update (stream->drawable,
                        stream->left, stream->top,
                        stream->width, stream->height);
  gimp_displays_flush ();
  gimp_drawable_detach (stream->source);
  gimp_drawable_detach (stream->target);
  g_free (stream);
  streams[id] = NULL;
#ifdef DEBUG
  fprintf (stderr, "closed %d\n", id);
#endif
} // tile_stream_close

/**
 * Get the data from the current tile.
 */
GimpPixelRgn *
tile_stream_get (int id)
{
  // Sanity checks
  if (! valid_stream_id (id))
    return NULL;
  if (streams[id]->iterator == NULL)
    return NULL;
  return &(streams[id]->source_region);
} // tile_stream_get

/**
 * Update the pixel data in the current tile.
 */
int
tile_stream_update (int id, guchar *data)
{
  if (! valid_stream_id (id))
    return -1;
  copy_pixels (&(streams[id]->target_region), data);
  return 0;
} // tile_stream_update
