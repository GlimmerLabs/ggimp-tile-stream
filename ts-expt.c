/**
 * ts-expt.c
 *   An experiment with tile streams.
 */


// +---------+---------------------------------------------------------
// | Headers |
// +---------+

#include <libgimp/gimp.h>

#include <stdio.h>

#include "tile-stream.h"


// +--------+----------------------------------------------------------
// | Macros |
// +--------+

/**
 * Print a string (somewhere).  Right now, this prints to stderr, which
 * means it only works when the program is run from the terminal.
 */
#define REPORT(str) fprintf (stderr, "*** %s ***\n", str)

/**
 * Print an integer.
 */
#define REPORT_INT(i) fprintf (stderr, "*** %s: %d ***\n", #i, i)


// +--------------------------+----------------------------------------
// | Function Predeclarations |
// +--------------------------+

static void query (void);
static void run (const gchar *name,
                       gint nparams,
                       const GimpParam *params,
                       gint *nreturn_vals,
                       GimpParam **return_vals);


// +-------------+-----------------------------------------------------
// | Boilerplate |
// +-------------+

/**
 * The four key functions.  This structure *must* be called PLUG_IN_INFO.
 */
GimpPlugInInfo PLUG_IN_INFO =
  {
    NULL,
    NULL,
    query,
    run
  };

/**
 * Indicate that we're ready to begin the main part of the code.
 */
MAIN ()


// +-------------------------+-----------------------------------------
// | Standard GIMP Functions |
// +-------------------------+

static void 
query (void)
{
  // Build the description of the parameters that the function expects.  
  // Each parameter has a type, a name, and a description
  // All plug-ins must take a run-mode.   Plug-ins associated with an
  // image
  static GimpParamDef args[] =
    {
      { GIMP_PDB_INT32, "run-mode", "Run mode" },
      { GIMP_PDB_IMAGE, "image", "Input image" },
      { GIMP_PDB_DRAWABLE, "drawable", "Input layer or other drawable" }
    }; // args

  // Tell the GIMP about our plugin.
  gimp_install_procedure (
    "munge-image",                                      // Name
    "Have fun with the image",                          // Blurb
    "I don't really care what this does.  It's just a test of tile-stream",
                                                        // Help
    "Samuel A. Rebelsky",                               // Author
    "Copyright (c) Samuel A. Rebelsky.  All rights reserved.",
                                                        // Copyright
    "2013",                                             // Year
    "<Image>/MediaScript/Munge",                        // Path
    "RGB",                                              // Image types
    GIMP_PLUGIN,                                        // Type
    3,                                                  // Number of params
    0,                                                  // Number of return vals
    args,                                               // Param descriptions
    NULL                                                // Return descriptions
    );

} // query

static void
run (const gchar      *name,
           gint              nparams,
           const GimpParam  *params,
           gint             *nreturn_vals,
           GimpParam       **return_vals)
{
  // Prepare return values
  static GimpParam results[1];
  results[0].type = GIMP_PDB_STATUS;
  results[0].data.d_status = GIMP_PDB_SUCCESS;
  *nreturn_vals = 1;
  *return_vals = results;

  // A bit of infrastructure to support debugging
#ifdef DEBUG
  fprintf (stderr, "pid is %d\n", getpid ());
  sleep (5);
#endif

  // Get parameters
  int image = params[1].data.d_image;
  int drawable = params[2].data.d_drawable;
  REPORT_INT (image);
  REPORT_INT (drawable);

  // Sanity check
  if (! gimp_image_is_valid (image))
    {
      results[0].data.d_status = GIMP_PDB_CALLING_ERROR;
      return;
    } 
  if (! gimp_drawable_is_valid (drawable))
    {
      results[0].data.d_status = GIMP_PDB_CALLING_ERROR;
      return;
    }

  // Set up the tile stream
  int stream = drawable_new_tile_stream (image, drawable);

  // Iterate through the region
  GimpPixelRgn *region;
  while ((region = tile_stream_get (stream)) != NULL)
    {
      guchar *data = region->data;

#ifdef DEBUG
      fprintf (stderr, "Starting tile x=%d, y=%d, w=%d, h=%d\n",
               region->x, region->y, region->w, region->h);
#endif

      int r;
      for (r = 0; r < region->h; r++)
        {
          guchar *pixel = data;
          int c;
          for (c = 0; c < region->w; c++)
            {
              // Change the pixel
              guchar tmp = pixel[0];
              pixel[0] = pixel[1];
              pixel[1] = pixel[2];
              pixel[2] = tmp;
              // Advance to the next pixel
              pixel += region->bpp;
            } // for c
          data += region->rowstride;
        } // for each row

      // Copy the modified data back
      tile_stream_update (stream, region->data);

      // Advance to the next tile
      tile_stream_advance (stream);
    } // while

  // Clean up
  tile_stream_close (stream);
} // run


