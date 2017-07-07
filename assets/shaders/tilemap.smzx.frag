uniform sampler2D baseMap;

varying vec2 vTexcoord;

float mod_(float v, float r)
{
  return clamp(mod(v + 0.01, r) - 0.01, 0.0, r);
}

int int_(float v)
{
  return int(v + 0.01);
}

void main( void )
{
    int x = int_(vTexcoord.x * 256.0 * 8.0);
    int y = int_(vTexcoord.y * 14.0);

    int cx = x / 8;
    int cy = y / 14;

    vec4 tileinfo = texture2D( baseMap, vec2(float(cx) / 512.0, (float(cy) + 901.0)/1024.0));
    int ti_byte1 = int_(tileinfo.x * 255.0);
    int ti_byte2 = int_(tileinfo.y * 255.0);
    int ti_byte3 = int_(tileinfo.z * 255.0);
    int ti_byte4 = int_(tileinfo.w * 255.0);

    // 00000000 00000000 00000000 00000000
    // CCCCCCCC CCCCCCBB BBBBBBBF FFFFFFFF

    int fg_color = ti_byte1 + int_(mod_(float(ti_byte2), 2.0)) * 256;
    int bg_color = ti_byte2 / 2 + int_(mod_(float(ti_byte3), 4.0)) * 128;
    int char_num = ti_byte3 / 4 + ti_byte4 * 64;
    if (fg_color == 272 || bg_color == 272) {
      gl_FragColor = vec4(0.0, 0.0, 0.0, 0.0);
    } else {
      //char_num = mod_(char_num, 256);
      
      int px = int_(mod_(float(x), 8.0));
      int py = int_(mod_(float(y), 14.0));
      /*
      if (px == 0 && py == 13)
        gl_FragColor = texture2D( baseMap, vec2(1.0 / 512.0 * fg_color, 896.0/1024.0));
      else
        gl_FragColor = texture2D( baseMap, vec2(1.0 / 512.0 * bg_color, 896.0/1024.0));
      */
      int char_x = int_(mod_(float(char_num), 64.0)) * 8 + (px / 2 * 2);
      int char_y = char_num / 64 * 14 + py;
      
      int smzx_col;
      if (texture2D(baseMap, vec2(1.0 / 512.0 * float(char_x), 1.0 / 1024.0 * float(char_y))).x < 0.5) {
        if (texture2D(baseMap, vec2(1.0 / 512.0 * float(char_x + 1), 1.0 / 1024.0 * float(char_y))).x < 0.5) {
          smzx_col = 0;
        } else {
          smzx_col = 1;
        }
      } else {
        if (texture2D(baseMap, vec2(1.0 / 512.0 * float(char_x + 1), 1.0 / 1024.0 * float(char_y))).x < 0.5) {
          smzx_col = 2;
        } else {
          smzx_col = 3;
        }
      }

      int mzx_col = int_(mod_(float(bg_color), 16.0)) * 16 + int_(mod_(float(fg_color), 16.0));
      int real_col = int_(texture2D( baseMap, vec2(1.0 / 512.0 * float(mzx_col), (897.0 + float(smzx_col))/1024.0)).r * 255.0);
      gl_FragColor = texture2D( baseMap, vec2(1.0 / 512.0 * float(real_col), 896.0/1024.0));
    }
}