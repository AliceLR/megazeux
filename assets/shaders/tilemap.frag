uniform sampler2D baseMap;

varying vec2 vTexcoord;

void main( void )
{
    int x = int(vTexcoord.x * 256.0 * 8.0);
    int y = int(vTexcoord.y * 14.0);

    int cx = x / 8;
    int cy = y / 14;

    vec4 tileinfo = texture2D( baseMap, vec2(float(cx) / 512.0, (float(cy) + 901.0)/1024.0));
    int ti_byte1 = int(tileinfo.x * 255.0);
    int ti_byte2 = int(tileinfo.y * 255.0);
    int ti_byte3 = int(tileinfo.z * 255.0);
    int ti_byte4 = int(tileinfo.w * 255.0);

    // 00000000 00000000 00000000 00000000
    // CCCCCCCC CCCCCCBB BBBBBBBF FFFFFFFF

    int fg_color = ti_byte1 + int(mod(float(ti_byte2), 2.0)) * 256;
    int bg_color = ti_byte2 / 2 + int(mod(float(ti_byte3), 4.0)) * 128;
    int char_num = ti_byte3 / 4 + ti_byte4 * 64;
    if (fg_color == 272 || bg_color == 272) {
      gl_FragColor = vec4(0.0, 0.0, 0.0, 0.0);
      return;
    }
    
    int px = int(mod(float(x), 8.0));
    int py = int(mod(float(y), 14.0));
    int char_x = int(mod(float(char_num), 64.0)) * 8 + px;
    int char_y = char_num / 64 * 14 + py;
    
    bool pixset = texture2D(baseMap, vec2(1.0 / 512.0 * float(char_x), 1.0 / 1024.0 * float(char_y))).x > 0.5;
    
    if(pixset)
    {
      gl_FragColor = texture2D( baseMap, vec2(1.0 / 512.0 * float(fg_color), 896.0/1024.0));
    }
    else
    {
      gl_FragColor = texture2D( baseMap, vec2(1.0 / 512.0 * float(bg_color), 896.0/1024.0));
    }
}
