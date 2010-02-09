/*
 * Write me
 */
#include <stdlib.h>
#include <stdio.h>

int fgetw(FILE *fp)
{
  int r = fgetc(fp);
  r |= fgetc(fp) << 8;
  return r;
}

int fgetd(FILE *fp)
{
  int r = fgetc(fp);
  r |= fgetc(fp) << 8;
  r |= fgetc(fp) << 16;
  r |= fgetc(fp) << 24;

  return r;
}

#define error(...) \
  { \
    fprintf(stderr, __VA_ARGS__); \
    fflush(stderr); \
  }

struct subfile
{
  char name[13];
  int offset;
  int length;
};

int main(int argc, char *argv[])
{
  FILE *in = NULL, *out = NULL;
  int i, num_files, ret = 1;
  struct subfile *subfiles;

  if(argc != 3)
  {
    error("usage: %s [help.fil] [help.txt]\n", argv[0]);
    goto exit_out;
  }

  in = fopen(argv[1], "r");
  if(!in)
  {
    error("Failed to open input file '%s' for reading.\n", argv[1]);
    goto exit_close;
  }

  out = fopen(argv[2], "w");
  if(!out)
  {
    error("Failed to open output file '%s' for writing.\n", argv[2]);
    goto exit_close;
  }

  /* Each help FIL is split into subfiles internally, aligned to pages,
   * and with a fixed offset and length. Walk these subfiles here.
   */
  num_files = fgetw(in);
  subfiles = calloc(num_files, sizeof(struct subfile));

  for(i = 0; i < num_files; i++)
  {
    if(fread(&subfiles[i].name, 13, 1, in) != 1)
      goto err_read_io;
    subfiles[i].offset = fgetd(in);
    subfiles[i].length = fgetd(in);
  }

  for(i = 0; i < num_files; i++)
  {
    int start_pos;

    if(fseek(in, subfiles[i].offset, SEEK_SET) != 0)
      goto err_read_io;

    /* Each help subfile contains a single byte indicating the index
     * of the subfile. We must ignore this for converting out.
     */
    if(fseek(in, 1, SEEK_CUR) != 0)
      goto err_read_io;
    subfiles[i].length--;

    /* We're going through line-wise but we need an absolute reference. */
    start_pos = ftell(in);
    if(start_pos < 0)
      goto err_read_io;

    if(i > 0)
    {
      /* If we're not the zero'th subfile we need to write out our name. */
      if(fprintf(out, "#%s\n", subfiles[i].name) < 0)
        goto err_write_io;
    }

    while(1)
    {
      int byte, curr_pos;
      char buffer[256];

      curr_pos = ftell(in);
      if(curr_pos < 0)
        goto err_read_io;

      if(curr_pos - start_pos >= subfiles[i].length)
        break;

      /* Peek at the next byte to check it's not 0xFF. This character
       * indicates that the line is something special, like a link.
       */
      byte = fgetc(in);
      if(byte == EOF)
        goto err_read_io;

      /* We got a control sequence of some kind. Try to decode it. */
      if(byte == 0xff)
      {
        byte = fgetc(in);
        if(byte == EOF)
          goto err_read_io;

        switch(byte)
        {
          /* Centered line */
          case '$':
            /* Write out the control code */
            if(fputc('$', out) == EOF)
              goto err_write_io;
            break;

          /* Label */
          case ':':
          /* Choice (non-file) */
          case '>':
            /* Write out the control code */
            if(fputc(byte, out) == EOF)
              goto err_write_io;

            /* Read label length in bytes */
            byte = fgetc(in);
            if(byte == EOF)
              goto err_read_io;

            /* Read this many bytes and write them out */
            if(fread(buffer, byte, 1, in) != 1)
              goto err_read_io;
            if(fwrite(buffer, byte, 1, out) != 1)
              goto err_write_io;

            /* Skip over NULL terminator, and defer */
            if(fseek(in, 1, SEEK_CUR) != 0)
              goto err_read_io;

            /* Write out separator */
            if(fputc(':', out) == EOF)
              goto err_write_io;
            break;

          /* Choice (file) */
          case '<':
            /* Write out the control code */
            if(fputc('>', out) == EOF)
              goto err_write_io;

            /* FIXME: Assume it's a file link */
            if(fputc('#', out) == EOF)
              goto err_write_io;

            /* Read label length in bytes */
            byte = fgetc(in);
            if(byte == EOF)
              goto err_read_io;

            /* Read this many bytes and write them out */
            if(fread(buffer, byte, 1, in) != 1)
              goto err_read_io;
            if(fwrite(buffer, byte, 1, out) != 1)
              goto err_write_io;

            /* Write out separator */
            if(fputc(':', out) == EOF)
              goto err_write_io;

            /* Skip over NULL terminator, and defer */
            if(fseek(in, 1, SEEK_CUR) != 0)
              goto err_read_io;
            break;

          default:
            error("Unhandled control sequence '%c', trying to skip.\n", byte);
            if(!fgets(buffer, 256, in))
              goto err_read_io;
            continue;
        }

        /* Read the tail end of the command as a regular string. */
        if(!fgets(buffer, 256, in))
          goto err_read_io;
      }
      else
      {
        /* Otherwise backtrace this peek, prepping for a normal read. */
        if(fseek(in, -1, SEEK_CUR) != 0)
          goto err_read_io;

        /* Regular string; can be read as ascii. */
        if(!fgets(buffer, 256, in))
          goto err_read_io;

        /* Special case escaping -- obviously since the first column of any
         * line can contain "control" characters, we must not emit these with
         * "normal" strings. The system has a reserved '.' character for this.
         */
        switch(buffer[0])
        {
          case '>':
          case '<':
          case ':':
          case '#':
          case '.':
            if(fputc('.', out) == EOF)
              goto err_write_io;
            break;
        }
      }

      if(fprintf(out, "%s", buffer) < 0)
        goto err_write_io;
    }
  }

  ret = 0;

exit_free:
  free(subfiles);
exit_close:
  if(out)
    fclose(out);
  if(in)
    fclose(in);
exit_out:
  return ret;

err_read_io:
  error("I/O error on read, aborting.\n");
  goto exit_free;

err_write_io:
  error("I/O error on write, aborting.\n");
  goto exit_free;
}
