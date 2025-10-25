/* 
 * MIKPLAY.C - Simple MOD player with Turbo Vision-style TUI
 * 
 * Compile with:
 * wcl386 -l=dos32a -5s -bt=dos -fp5 -fpi87 -mf -oeatxh -w4 -ei -zp8 -zq -dMIKMOD_STATIC=1 -i..\libmikmod-3.3.13\include\ mikplay.c ..\libmikmod-3.3.13\dos\mikmod.lib
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <conio.h>
#include <time.h>
#include <sys/stat.h>
#include <mikmod.h>

#define MEM_LOAD_THRESHOLD 6291456
#define VER_MAJ 0
#define VER_MIN 4
#define VER_STR "retrohw"

// IBM Chars (CP437)
#define CH_ULCORNER 218  // Ú
#define CH_URCORNER 191  // ¿
#define CH_LLCORNER 192  // À
#define CH_LRCORNER 217  // Ù
#define CH_HLINE    196  // Ä
#define CH_VLINE    179  // ³
#define CH_LTEE     195  // Ã
#define CH_RTEE     180  // ´
#define CH_TTEE     194  // Â
#define CH_BTEE     193  // Á
#define CH_CROSS    197  // Å
#define CH_BLOCK    219  // Û

// Colors
#define COL_CYAN_BLUE    0x31    // Cyan on blue
#define COL_WHITE_CYAN   0x3F    // White on cyan
#define COL_BLACK_CYAN   0x30    // Black on cyan
#define COL_YELLOW_CYAN  0x3E    // Yellow on cyan
#define COL_BLACK_LGRAY  0x70    // Black on light gray
#define COL_WHITE_BLUE   0x1F    // White on blue
#define COL_CYAN_BLACK   0x03    // Cyan on black
#define COL_WHITE_BLACK  0x0F    // White on black
#define COL_YELLOW_BLACK 0x0E    // Yellow on black
#define COL_LGRAY_BLACK  0x07    // Light gray on black

// Global vars
MODULE *g_module = NULL;
int g_comment_scroll = 0;
int current_volume = 128;
int max_voices = 64;
int mix_freq = 22050;
int force_mono = 0;
char *filename;
long file_size;

void write_char_attr(int x, int y, unsigned char ch, unsigned char attr)
{
    unsigned short *vram = (unsigned short *)0xB8000;
    vram[y * 80 + x] = (attr << 8) | ch;
}

void write_string_attr(int x, int y, const char *str, unsigned char attr)
{
    while (*str) {
        write_char_attr(x++, y, *str++, attr);
    }
}

void fill_rect(int x1, int y1, int x2, int y2, unsigned char ch, unsigned char attr)
{
    int x, y;
    for (y = y1; y <= y2; y++)
        for (x = x1; x <= x2; x++)
            write_char_attr(x, y, ch, attr);
}

void draw_box(int x1, int y1, int x2, int y2, unsigned char attr)
{
    int i;

    // TODO: set background
    fill_rect(x1, x2, y1, y2, '+', COL_BLACK_CYAN);

    write_char_attr(x1, y1, CH_ULCORNER, attr);
    write_char_attr(x2, y1, CH_URCORNER, attr);
    write_char_attr(x1, y2, CH_LLCORNER, attr);
    write_char_attr(x2, y2, CH_LRCORNER, attr);
    
    for (i = x1 + 1; i < x2; i++)
    {
        write_char_attr(i, y1, CH_HLINE, attr);
        write_char_attr(i, y2, CH_HLINE, attr);
    }
    
    for (i = y1 + 1; i < y2; i++)
    {
        write_char_attr(x1, i, CH_VLINE, attr);
        write_char_attr(x2, i, CH_VLINE, attr);   
    }
}

void draw_title_bar(const char *title)
{
    fill_rect(0, 0, 79, 0, ' ', COL_BLACK_LGRAY);
    write_string_attr(2, 0, title, COL_BLACK_LGRAY);
}

void draw_menu_bar()
{
    fill_rect(0, 1, 79, 1, ' ', COL_BLACK_CYAN);
    write_string_attr(2, 1, "File", COL_BLACK_CYAN);
    write_string_attr(8, 1, "Playback", COL_BLACK_CYAN);
    write_string_attr(19, 1, "Help", COL_BLACK_CYAN);
}

void draw_status_bar(const char *text)
{
    fill_rect(0, 24, 79, 24, ' ', COL_BLACK_LGRAY);
    write_string_attr(1, 24, text, COL_BLACK_LGRAY);
}

void draw_info_panel(MODULE *module, int volume)
{
    char buf[80];
    int active_channels = 0;
    int i, max_volume = 0, vu_level;
    
    // Count active voices
    for (i = 0; i < module->numvoices; i++)
    {
        if (!Voice_Stopped(i))
        {
            int vol = Voice_GetVolume(i);
            active_channels++;
            if (vol > max_volume) max_volume = vol;
        }
    }
    
    vu_level = (max_volume * 10) / 256;
    if (vu_level > 10) vu_level = 10;
    
    draw_box(1, 3, 40, 11, COL_BLACK_CYAN);
    write_string_attr(3, 3, "Modinfo", COL_YELLOW_CYAN);
    
    sprintf(buf, "%.28s", module->songname);
    write_string_attr(3, 4, buf, COL_BLACK_CYAN);
    sprintf(buf, "%s", module->modtype);
    write_string_attr(3, 5, buf, COL_BLACK_CYAN);
    sprintf(buf, "File    : %s (%.2f KB)", filename, file_size / 1024.0);
    write_string_attr(3, 6, buf, COL_BLACK_CYAN);
    sprintf(buf, "Channels: %d,  Patterns: %d", module->numchn, module->numpos);
    write_string_attr(3, 7, buf, COL_BLACK_CYAN);
    sprintf(buf, "Instrmts: %d,  Samples : %d", module->numins, module->numsmp);
    write_string_attr(3, 8, buf, COL_BLACK_CYAN);
    sprintf(buf, "Output  : %dHz, %s, %d voices", mix_freq, force_mono ? "mono" : "stereo", max_voices);
    write_string_attr(3, 9, buf, COL_BLACK_CYAN);
    sprintf(buf, "Volume  : %3d, Channels: %02d/%02d", volume, active_channels, module->numvoices);
    write_string_attr(3, 10, buf, COL_WHITE_CYAN);
}

void draw_playback_panel(MODULE *module)
{
    char buf[80];
    int i, vu_level, max_volume = 0;
    
    // Calculate VU
    for (i = 0; i < module->numvoices; i++)
    {
        if (!Voice_Stopped(i))
        {
            int vol = Voice_GetVolume(i);
            if (vol > max_volume) max_volume = vol;
        }
    }

    vu_level = (max_volume * 20) / 256;
    if (vu_level > 20) vu_level = 20;
    
    draw_box(41, 3, 78, 11, COL_BLACK_CYAN);
    write_string_attr(43, 3, "Playback", COL_YELLOW_CYAN);
    
    sprintf(buf, "Pattrn: %02d/%02d", module->sngpos, module->numpos - 1);
    write_string_attr(43, 4, buf, COL_BLACK_CYAN);
    sprintf(buf, "Row   : %02d", module->patpos);
    write_string_attr(43, 5, buf, COL_BLACK_CYAN);
    sprintf(buf, "Speed : %02d", module->sngspd);
    write_string_attr(43, 6, buf, COL_BLACK_CYAN);
    sprintf(buf, "BPM   : %03d", module->bpm);
    write_string_attr(43, 7, buf, COL_BLACK_CYAN);
    
    // VU meter
    write_string_attr(43, 10, "VU: [", COL_WHITE_CYAN);
    for (i = 0; i < 20; i++) write_char_attr(48 + i, 10, i < vu_level ? CH_BLOCK : 196, i < vu_level ? 0x08 : 0x08);
    write_char_attr(68, 10, ']', COL_WHITE_CYAN);
}

void draw_comment_panel(MODULE *module)
{
    int i, y;
    char *comment = module->comment;
    int line_count = 0;
    int start_line = g_comment_scroll;
    
    draw_box(1, 12, 78, 22, COL_BLACK_CYAN);
    write_string_attr(3, 12, "Notes", COL_YELLOW_CYAN);
    
    if (!comment || strlen(comment) == 0) return;
    
    // Comment lines
    y = 13;
    for (i = 0; comment[i] && y < 22; )
    {
        char line[76];
        int len = 0;
        
        // Skip to start line
        if (line_count < start_line)
        {
            while (comment[i] && comment[i] != '\n' && comment[i] != '\r') i++;
            if (comment[i] == '\r') i++;
            //if (comment[i] == '\n') i++;
            line_count++;
            continue;
        }
        
        // Read line
        while (comment[i] && comment[i] != '\n' && comment[i] != '\r' && len < 74) line[len++] = comment[i++];
        line[len] = '\0';
        
        // Skip CR/LF
        if (comment[i] == '\r') i++;
        //if (comment[i] == '\n') i++;
        
        write_string_attr(3, y++, line, COL_LGRAY_BLACK);
        line_count++;
    }
}

void draw_ui(MODULE *module, int volume, char *s_profile)
{
    int x, y;
    char title[80];

    sprintf(title, "MikPlayer-%d.%d-%s - %s mode", VER_MAJ, VER_MIN, VER_STR, s_profile);

    // BACKGROUND FILL
    for (y = 1; y < 24; y++)
        for (x = 0; x < 80; x++) write_char_attr(x, y, 176, COL_BLACK_CYAN);

    draw_title_bar(title);
    //draw_menu_bar();
    draw_info_panel(module, volume);
    draw_playback_panel(module);
    draw_comment_panel(module);
    //draw_status_bar(Player_Paused() ? "[PAUSED] ESC=Quit SPACE=Play <-/->=Skip" : "[PLAYING] ESC=Quit SPACE=Pause <-/->=Skip +/-=Vol");
}

void update_ui(MODULE *module, int volume)
{
    draw_info_panel(module, volume);
    draw_playback_panel(module);
    draw_status_bar(Player_Paused() ? "[PAUSED] ESC=Quit SPACE=Play <-/->=Skip" : "[PLAYING] ESC=Quit SPACE=Pause <-/->=Skip +/-=Vol");
}

int process_keyboard(MODULE *module, int volume)
{
    if (kbhit())
    {
        int ch = getch();
        
        if (ch == 27) return 0; // ESC
        else if (ch == 'q' || ch == 'Q') return 0;
        else if (ch == ' ')
        {
            Player_TogglePause();
            update_ui(module, volume);
        }
        else if (ch == '+' || ch == '=')
        {
            if (volume < 128)
            {
                volume += 8;
                Player_SetVolume(volume);
                current_volume = volume;
                update_ui(module, volume);
            }
        }
        else if (ch == '-' || ch == '_')
        {
            if (volume > 0)
            {
                volume -= 8;
                Player_SetVolume(volume);
                current_volume = volume;
                update_ui(module, volume);
            }
        }
        else if (ch == 0)
        {
            ch = getch();
            
            if (ch == 75) // Left arrow
            {
                if (module->sngpos > 0)
                {
                    Player_SetPosition(module->sngpos - 1);
                    update_ui(module, volume);
                }
            }
            else if (ch == 77) // Right arrow
            {
                if (module->sngpos < module->numpos - 1) {
                    Player_SetPosition(module->sngpos + 1);
                    update_ui(module, volume);
                }
            }
            else if (ch == 72) // Up arrow 
            {
                if (g_comment_scroll > 0)
                {
                    g_comment_scroll--;
                    draw_comment_panel(module);
                }
            }
            else if (ch == 80) // Down arrow
            {
                g_comment_scroll++;
                draw_comment_panel(module);
            }
        }
    }
    return 1;
}

int main(int argc, char *argv[])
{
    MODULE *module;
    clock_t last_update = 0;
    int update_interval = CLOCKS_PER_SEC / 8;
    struct stat file_info;
    int use_memory_load = 0;
    char *s_profile = "default";
    int i;
    
    if (argc < 2)
    {
        printf("\nUsage: mikplay <module_file> [options]\n");
        printf("Options:\n");
        printf("  -386      386 mode: 11kHz, mono, 8 voices, slow display\n");
        printf("  -486      Slow 486 mode: 22kHz, stereo, 16 voices\n");
        printf("  -hifi     Hi-Fi mode: 44kHz, stereo, 64 voices (default: 22kHz)\n");
        printf("  -mono     Force mono output\n");
        printf("  -v<num>   Set max voices (default: 64)\n");
        printf("  -f<freq>  Set mixing frequency: 11025, 22050, 44100\n");
        printf("\nSupports: IT, MOD, S3M, XM, etc.\n");
        return 1;
    }

    printf("\nMikPlayer, ver.%d.%d-%s\n(c) 2025 Dimitar Angelov\n\n", VER_MAJ, VER_MIN, VER_STR);

    // Parse command line options
    for (i = 2; i < argc; i++)
    {
        if (strcmp(argv[i], "-386") == 0)
        {
            mix_freq = 11025;
            max_voices = 8;
            force_mono = 1;
            update_interval = CLOCKS_PER_SEC;
            s_profile = "386";
        }
        else if (strcmp(argv[i], "-486") == 0)
        {
            mix_freq = 22050;
            max_voices = 16;
            s_profile = "slow 486";
        }
        else if (strcmp(argv[i], "-hifi") == 0)
        {
            mix_freq = 44100;
            max_voices = 64;
            s_profile = "hifi";
        }
        else if (strcmp(argv[i], "-mono") == 0)
        {
            force_mono = 1;
            printf("Forced mono output\n");
        }
        else if (argv[i][0] == '-' && argv[i][1] == 'v')
        {
            max_voices = atoi(argv[i] + 2);
            printf("Max voices set to: %d\n", max_voices);
        }
        else if (argv[i][0] == '-' && argv[i][1] == 'f')
        {
            mix_freq = atoi(argv[i] + 2);
            printf("Mix frequency set to: %d Hz\n", mix_freq);
        }
    }
    
    filename = argv[1];

    printf("Initializing MikMod Library v.%ld.%ld.%ld ...\n",
           (MikMod_GetVersion() >> 16) & 0xFF,
           (MikMod_GetVersion() >> 8) & 0xFF,
           MikMod_GetVersion() & 0xFF);
    
    MikMod_RegisterAllLoaders();
    MikMod_RegisterAllDrivers();
    
    md_mode = DMODE_SOFT_MUSIC;
    md_mixfreq = mix_freq;
    if (force_mono) md_mode &= ~DMODE_STEREO;
    else md_mode |= DMODE_STEREO;
    
    printf("Audio output (%s): %dHz, %s, %d voices max\n",
        s_profile, mix_freq, force_mono ? "mono" : "stereo", max_voices);

    if (MikMod_Init(""))
    {
        printf("Could not initialize MikMod: %s\n", MikMod_strerror(MikMod_errno));
        return 1;
    }
    
    if (stat(filename, &file_info) == 0)
    {
        file_size = file_info.st_size;
        if (file_size < MEM_LOAD_THRESHOLD) use_memory_load = 1;
    } else
        file_size = 0;

    if (use_memory_load)
    {
        FILE *fp;
        char *buffer;
        
        printf("Loading %s to memory (%.2f KB) ...\n", filename, file_size / 1024.0);
        
        fp = fopen(filename, "rb");
        if (!fp)
        {
            printf("Could not open file\n");
            MikMod_Exit();
            return 1;
        }
        
        buffer = (char *)malloc(file_size);
        if (!buffer)
        {
            printf("Out of memory\n");
            fclose(fp);
            MikMod_Exit();
            return 1;
        }
        
        if (fread(buffer, 1, file_size, fp) != file_size)
        {
            printf("Read error\n");
            free(buffer);
            fclose(fp);
            MikMod_Exit();
            return 1;
        }
        fclose(fp);
        
        module = Player_LoadMem(buffer, file_size, max_voices, 0);
        free(buffer);
    }
    else
    {
        printf("Streaming %s from disk ...\n", filename);
        module = Player_Load(filename, max_voices, 0);
    }

    if (!module)
    {   
        printf("Could not load module: %s\n", MikMod_strerror(MikMod_errno));
        MikMod_Exit();
        return 1;
    }
    
    g_module = module;
    Player_Start(module);
    Player_SetVolume(current_volume);

    // Switch to TUI
    //_settextcursor(0x2000); // Hide cursor
    draw_ui(module, current_volume, s_profile);
    
    while (process_keyboard(module, current_volume)) 
    {
        clock_t now = clock();

        MikMod_Update();

        if (now - last_update >= update_interval)
        {
            update_ui(module, current_volume);
            last_update = now;
        }
        
        if (!Player_Active()) break;
    }
    
    // Cleanup
    //_settextcursor(0x0607); // Restore cursor 
    printf("EOF\n");
    Player_Stop();
    Player_Free(module);
    MikMod_Exit();
    
    return 0;
}
