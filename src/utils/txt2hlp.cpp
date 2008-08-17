/* MegaZeux
 *
 * Copyright (C) 1996 Greg Janson
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

// Cleaned up/revised txt2hlp.cpp for compiling outside of DOS

// Endian safe file readers taken from world.cpp..
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

void fputw(int src, FILE *fp)
{
  fputc(src & 0xFF, fp);
  fputc(src >> 8, fp);
}

void fputd(int src, FILE *fp)
{
  fputc(src & 0xFF, fp);
  fputc((src >> 8) & 0xFF, fp);
  fputc((src >> 16) & 0xFF, fp);
  fputc((src >> 24) & 0xFF, fp);
}

// Custom fgetc function to dump \r's and replace \xFF's and /x1's.
int fgetc2(FILE *fp)
{
  int current_char = fgetc(fp);
  switch(current_char)
  {
    case '\r':
    {
      return fgetc2(fp);
    }

    case 255:
    {
      return 32;
    }

    case 1:
    {
      return 2;
    }

    default:
    {
      return current_char;
    }
  }
}

#define fgetc(f) fgetc2(f)

int main(int argc, char *argv[])
{
  int current_char;
  int t1, t2, t3;
  char tstr[81] = "";
  int tlong;
  int i;

  // Source/dest files open. Now scan source for number of filenames.
  // A new file is designated by a # as the first character of a line.

  int num_files = 1;

  // Count context sensitive links too. A link is designated by a : as
  // the first character of a line, followed by three numerals, followed
  // by the end of the line. (a \n)

  int num_links = 0;

  FILE *source;
  FILE *dest;

  printf("\n\n");

  if(argc < 3)
  {
    printf("Usage: TXT2HLP source.txt output.fil\n");
    return 1;
  }

  source = fopen(argv[1], "rb");

  if(source == NULL)
  {
    printf("Error opening %s for input.\n", argv[1]);
    return -1;
  }

  dest = fopen(argv[2],"wb");

  if(dest == NULL)
  {
    fclose(source);
    printf("Error opening %s for output.\n", argv[2]);
    return 1;
  }

  do
  {
    current_char = fgetc(source);

    switch(current_char)
    {
      case '#':
      {
        num_files++;
        break;
      }

      case ':':
      {
        // Get upcoming link
        int link = fscanf(source, "%3d");

        // Get next char
        current_char = fgetc(source);
        
        if(((current_char == '\n') || (current_char == ':')) &&
         link >= numlinks))
        {
          num_links = link + 1;
        }
      }
    }

    // Get the rest of the line
    while((current_char != '\n') && (current_char != -1))
    {
      current_char == fgetc(source);
    }
  } while(current_char != -1);

  printf("Files: %d  Links: %d\n", numfiles, numlinks);

  // Number of files obtained. Write header, with room, to dest. This
  // includes number of links and blank spaces to add in filenames and
  // link info.

  fputw(num_files, dest);
  for(i = 0; i < numfiles; i++)
  {
    fwrite("\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 21, 1, dest);
  }

  fputw(num_links, dest);
  if(numlinks > 0)
  {
    for(i = 0; i < num_links; i++)
    {
      fwrite("\0\0\0\0\0\0\0\0\0\0\0\0", 12, 1, dest);
    }
  }

  //Now we start our actual file chain. Current file info-
  char curr_file[13]="MAIN.HLP";
  char max_file[13]="(none)";
  long file_offs=ftell(dest);
  long file_len=1;//For first byte
  long curr_file_storage=2;//Location to store current file's info
  int curr_lnum=1;//For error/warning info
  //Links info-
  char curr_link_ref=0;//Ref- not actual link number
  int link_numbers[1000];//Actual numbers of the links
  long link_offsets[1000];//Offsets within current file
  //Current line info-
  int line_len=0;//Used for warnings
  //Global info-
  long biggest_file=0;//For help allocation

  //Reset position on source...
  fseek(source,0,SEEK_SET);
  //...and begin our parse! New files start with a 1.
  fputc(1,dest);
  printf("Processing file %s...\n",curr_file);

  do
  {
    //Our loop. Read first char of current line
    line_len=0;
    t1=fgetc(source);
    switch(t1)
    {
      case '$'://Centered line- write prefix...
        fputc(255,dest); fputc('$',dest); file_len+=2;
        //...and read first character...
      case '.'://Normal line- read first character
        mesg://Jump point to copy until and through an \n
        t1=fgetc(source);
      default://Normal line (first char in t1)
        //Copy until a \n
        if(t1=='\n') goto blanko;
        while(!feof(source))
        {
          fputc(t1,dest); file_len++;
          line_len++;
          t1=fgetc(source);
          if(t1=='\n') break;
        }
        //Now put an \n
        blanko:
        fputc('\n',dest); file_len++;
        break;
      case '>'://Choice of some type
        fputc(255,dest); file_len++;
        //See if dest is normal or file
        t1=fgetc(source);
        if(t1=='#')
        {
          //File.
          fputc('<',dest); file_len++;
          //Grab name.
          t2=t3=0;//t2 counts str size, t3 counts colons passed
          do
          {
            t1=fgetc(source);
            if((t1==':')&&t3) break;
            if(t1=='\n') break;
            if(t1==':') t3++;
            tstr[t2++]=t1;
          } while(!feof(source));
          tstr[t2]=0;
          if(t2==10) //10 len not allowed
            printf("Error- Label length of 10 on line %d, file %s.\n",
                   curr_lnum,curr_file);
          fputc(t2,dest); file_len++;
          fwrite(tstr,++t2,1,dest); file_len+=t2;
          //Now do actual message after the label/file.
          goto mesg;
        }
        //Non-file.
        fputc('>',dest); file_len++;
        //Grab name.
        t2=0;//t2 counts str size
        do
        {
          if((t1==':')||(t1=='\n')) break;
          tstr[t2++]=t1;
          t1=fgetc(source);
        } while(!feof(source));
        tstr[t2]=0;
        if(t2==1) //1 len not allowed
          printf("Error- Label length of 1 on line %d, file %s.\n",
                 curr_lnum,curr_file);
        if(t2==10) //10 len not allowed
          printf("Error- Label length of 10 on line %d, file %s.\n",
                 curr_lnum,curr_file);
        fputc(t2,dest); file_len++;
        fwrite(tstr,++t2,1,dest); file_len+=t2;
        //Now do actual message after the label.
        goto mesg;
      case ':'://Label, possibly with a message attached
        fputc(255,dest); fputc(':',dest); file_len+=2;
        //Grab name.
        t2=0;//Counts str length
        do
        {
          t1=fgetc(source);
          if((t1==':')||(t1=='\n')) break;
          tstr[t2++]=t1;
        } while(!feof(source));
        tstr[t2]=0;
        if(t2==1) //1 len not allowed
          printf("Error- Label length of 1 on line %d, file %s.\n",
                 curr_lnum,curr_file);
        if(t2==10) //10 len not allowed
          printf("Error- Label length of 10 on line %d, file %s.\n",
                 curr_lnum,curr_file);
        fputc(t2,dest); file_len++;
        fwrite(tstr,++t2,1,dest); file_len+=t2;
        //Is this a context-link?
        if((t2==4)&&(isdigit(tstr[0]))&&
           (isdigit(tstr[1]))&&(isdigit(tstr[2])))
        {
          //Yep.
          t3=atoi(tstr);
          link_numbers[curr_link_ref]=t3;
          link_offsets[curr_link_ref++]=file_len-7;
        }
        //Is there a message next?
        if(t1==':') goto mesg;//Yep.
        //Nope- Just output an EOL.
        fputc('\n',dest); file_len++;
        break;
      case '#'://New file
        //First, end current
        fputc(0,dest); file_len++;
        if(file_len>biggest_file)
        {
          biggest_file=file_len;
          strcpy(max_file,curr_file);
        }
        //Put it's info at the start
        tlong=ftell(dest);
        fseek(dest,curr_file_storage,SEEK_SET);
        fwrite(curr_file,1,13,dest);
        fwrite(&file_offs,1,4,dest);
        fwrite(&file_len,1,4,dest);
        if(file_len>65535U) printf("Warning- File %s over 64k bytes in length.\n",curr_file);
        //Write in links info
        for(t1=0;t1<curr_link_ref;t1++)
        {
          fseek(dest,4+numfiles*21+link_numbers[t1]*12,SEEK_SET);
          fwrite(&file_offs,1,4,dest);
          fwrite(&file_len,1,4,dest);
          fwrite(&link_offsets[t1],1,4,dest);
        }
        //Start next file
        fseek(dest,tlong,SEEK_SET);
        file_offs=tlong;
        file_len=curr_lnum=1;
        curr_file_storage+=21;
        curr_link_ref=0;
        fputc(1,dest);
        //Get new filename
        t2=0;//Counts str length
        do
        {
          t1=fgetc(source);
          if(t1=='\n') break;
          tstr[t2++]=t1;
        } while(!feof(source));
        tstr[t2]=0;
        if(t2>12) tstr[12]=0;
        //Copy over new filename
        strcpy(curr_file,tstr);
        //Next file ready to roar!
        printf("Processing file %s...\n",curr_file);
        break;
      case '@'://End of help.doc file
        goto alldone;
    }
    if(line_len>64)
      printf("Warning: Line %d over 64 chars in file %s.\n",curr_lnum,curr_file);
    curr_lnum++;
    //Done with this line.
  } while(!feof(source));
  alldone:
  //End last file
  fputc(0,dest); file_len++;
  if(file_len>biggest_file)
  {
    biggest_file=file_len;
    strcpy(max_file,curr_file);
  }
  //Put it's info at the start
  fseek(dest,curr_file_storage,SEEK_SET);
  fwrite(curr_file,1,13,dest);
  fwrite(&file_offs,1,4,dest);
  fwrite(&file_len,1,4,dest);
  if(file_len>65535U) printf("Warning- File %s over 64k bytes in length.\n",curr_file);
  //Write in links info
  for(t1=0;t1<curr_link_ref;t1++)
  {
    fseek(dest,4+numfiles*21+link_numbers[t1]*12,SEEK_SET);
    fwrite(&file_offs,1,4,dest);
    fwrite(&file_len,1,4,dest);
    fwrite(&link_offsets[t1],1,4,dest);
  }
  //All done! Close files.
  fclose(dest);
  fclose(source);
  printf("Done! Biggest file- %ld bytes. (%s)\n\n",biggest_file,max_file);
}
