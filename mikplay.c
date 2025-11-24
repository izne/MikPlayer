/* 
 * MIKPLAY.C - A compact 32-bit DOS modfile player, based on the MikMod library
 * 
 * 
 * Compile:
 * wcl386 -cc++ -l=dos32a -5s -bt=dos -fp5 -fpi87 -mf -oeatxh -w4 -ei -zp8 -zq -dMIKMOD_STATIC=1 -i..\libmikmod-3.3.13\include\ mikplay.c ..\libmikmod-3.3.13\dos\mikmod.lib
 *
 * experimental cflags for optimized builds
 * AMD X5-160:
 * wcl386 -5r -fp5 -fpi87 -ox -om -s -ot -bt=dos -DMIKMOD_STATIC modtest.c mikmod.lib
 * 
 * 486 DX:
 * wcl386 -4r -fp3 -fpi87 -ox -om -s -ot -bt=dos -DMIKMOD_STATIC modtest.c mikmod.lib
 *
 * 386 DX:
 * wcl386 -3r -fp3 -fpi87 -ox -om -s -ot -bt=dos -DMIKMOD_STATIC modtest.c mikmod.lib
 *
 * and:
 * -ot = optimize for time (speed)
 * -ox = maximum optimization
 * -om = inline math functions
 * -s = remove stack overflow checks (faster)
 */


#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <conio.h>
#include <graph.h>
#include <time.h>
#include <sys/stat.h>
#include <mikmod.h>

#define MEM_LOAD_THRESHOLD 6291456
#define VER_MAJ 0
#define VER_MIN 71
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
#define CH_IND      254  // indicator light
#define CH_SHADE    176  // °

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

#define MAX_VOICES_CAP 128
#define MAX_IND 20
#define VOICE_QUERY_INTERVAL 4

typedef struct {
    int sngpos, numpos, patpos, volume;
    int active_inst, active_chan, active_samp;
    int numins, numchn, numsmp;
    int progress_level, row_level, vu_level;
} PanelCache;

// Globals
MODULE *g_module = NULL;
int g_comment_scroll = 0;
int g_comment_end = 0;
int g_view_mode = 1; // 0=browser, 1=comments //TODO: make ENUM for that
int g_channel_offset = 0;
int g_max_visible_channels = 8;
int current_volume = 128;
int max_voices = 64;
int mix_freq = 22050;
int force_mono = 0;
char *filename;
long file_size;
int audio_verbose = 0;
extern MDRIVER *md_driver;
static unsigned short *vram_base = (unsigned short *)0xB8000;

// looping
bool loop_enabled = false;
int loop_start = 42;
int loop_end = 46;

// cache instance
PanelCache cache = {
    -1, -1, -1, -1, // counter data (song/row/numpos and volume)
    -1, -1, -1,     // active samp/inst/chan indicators
    -1, -1, -1,     // number samp/inst/chan 
    -1, -1, -1      // row data (progress)
};

// JIJEI
float pitch_factor = 1.0;
int pitch_percent = 0;
ULONG voice_base_freq[MAX_VOICES_CAP];
unsigned char voice_base_set[MAX_VOICES_CAP];


static inline void write_char_attr(int x, int y, unsigned char ch, unsigned char attr)
{
    vram_base[y * 80 + x] = (attr << 8) | ch;
}

void write_string_attr(int x, int y, const char *s, unsigned char attr)
{
    unsigned short *p = vram_base + y*80 + x;
    while (*s)
        *p++ = (attr << 8) | *s++;
}

void fill_rect(int x1, int y1, int x2, int y2, unsigned char ch, unsigned char attr)
{
    int x, y;
    for (y = y1; y <= y2; y++)
        for (x = x1; x <= x2; x++)
            write_char_attr(x, y, ch, attr);
}

static const char digits[] = "0123456789";

static inline void itoa2(char *buf, int val)
{
    buf[0] = digits[val / 10];
    buf[1] = digits[val % 10];
    buf[2] = '\0';
}

static inline void itoa3(char *buf, int val)
{
    buf[0] = digits[val / 100];
    buf[1] = digits[(val / 10) % 10];
    buf[2] = digits[val % 10];
    buf[3] = '\0';
}

void writef2(char *buf, int x, int y, int formatType, int val1, int cacheVal1, int val2, int cacheVal2)
{
    const int val_start = 0;
    if (formatType == 3) // format 1: XXX/YYY
    {
        if(cacheVal1 != val1 || cacheVal2 != val2)
        {
            itoa3(buf + val_start, val1);
            buf[val_start + 3] = '/';
            itoa3(buf + val_start + 4, val2);
            buf[val_start + 7] = '\0'; 
            cacheVal1 = val1;
            cacheVal2 = val2;
        }
    }
    else if (formatType == 2) // format 2: XX/YY
    {
        if(cacheVal1 != val1 || cacheVal2 != val2)
        {
            itoa2(buf + val_start, val1);
            buf[val_start + 2] = '/';
            itoa2(buf + val_start + 3, val2);
            buf[val_start + 5] = '\0';
            cacheVal1 = val1;
            cacheVal2 = val2;
        }
    }
    else if (formatType == 1) // format 3: XXX (Single Value)
    {
        if(cacheVal1 != val1)
        {
            itoa3(buf + val_start, val1);
            buf[val_start + 3] = '\0';
            cacheVal1 = val1;
        }
    }

    write_string_attr(x, y, buf, COL_BLACK_CYAN);
}

void draw_box(int x1, int y1, int x2, int y2, unsigned char attr)
{
    int i;

    // Set background
    fill_rect(x1, y1, x2, y2, ' ', COL_BLACK_CYAN);

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
    write_string_attr(8, 1, "Settings", COL_BLACK_CYAN);
    write_string_attr(19, 1, "Help", COL_BLACK_CYAN);
}

void draw_status_bar(const char *text)
{
    fill_rect(0, 24, 79, 24, ' ', COL_BLACK_LGRAY);
    write_string_attr(1, 24, text, COL_BLACK_LGRAY);
}

void draw_info_panel(const MODULE *module)
{
    char buf[80];
    const char *drvname = (md_driver && md_driver->Name) ? md_driver->Name : "unknown";
    
    draw_box(1, 3, 40, 11, COL_BLACK_CYAN);
    write_string_attr(3, 3, "Info", COL_YELLOW_CYAN);
    
    sprintf(buf, "%.28s", module->songname);
    write_string_attr(3, 4, buf, COL_BLACK_CYAN);
    sprintf(buf, "%s", module->modtype);
    write_string_attr(3, 5, buf, COL_BLACK_CYAN);
    sprintf(buf, "File : %s (%.2f KB)", filename, file_size / 1024.0);
    write_string_attr(3, 6, buf, COL_BLACK_CYAN);
    sprintf(buf, "Chans: %02d, Patts: %d, BPM: %03d", module->numchn, module->numpos, module->bpm);
    write_string_attr(3, 7, buf, COL_BLACK_CYAN);
    sprintf(buf, "Instr: %02d, Samps: %d, Spd: %02d", module->numins, module->numsmp, module->sngspd);
    write_string_attr(3, 8, buf, COL_BLACK_CYAN);
    sprintf(buf, "Out  : %dHz, %s, %d voices", mix_freq, force_mono ? "mono" : "stereo", max_voices);
    write_string_attr(3, 9, buf, COL_BLACK_CYAN);
    sprintf(buf, "Drv  : %s", drvname);
    write_string_attr(3, 10, buf, COL_BLACK_CYAN);
}

void draw_playback_panel(const MODULE *module, int volume, int update)
{
    char buf[80];
    int i, vu_level = 0, max_volume = 0;
    int active_channels = 0, active_samples = 0, active_instruments = 0;
    int progress_level, row_level, vol;
    int current_pattern = module->sngpos;
    int max_rows = 64; // fallback
    int num_voices = module->numvoices;
    char active_channel_bar[MAX_IND + 1]; // MAX_IND chars + null terminator
    char active_sample_bar[MAX_IND + 1];
    char active_instrument_bar[MAX_IND + 1];

    static VOICEINFO voice_info[MAX_VOICES_CAP]; //64 or MAX_VOICES_CAP or module->numvoices ?
    unsigned char sample_active[MAX_VOICES_CAP] = {0};
    unsigned char instrument_active[MAX_VOICES_CAP] = {0};
    static int query_counter = 0;

    if (++query_counter >= VOICE_QUERY_INTERVAL) // every 4th frame only
    {
        Player_QueryVoices((num_voices > MAX_VOICES_CAP) ? MAX_VOICES_CAP : num_voices, voice_info);
        query_counter = 0;
    }
    

    // active channel/sample/instrument bar placeholders
    memset(active_channel_bar, CH_HLINE, MAX_IND);
    active_channel_bar[MAX_IND] = '\0';
    memset(active_sample_bar, CH_HLINE, MAX_IND);
    active_sample_bar[MAX_IND] = '\0';
    memset(active_instrument_bar, CH_HLINE, MAX_IND);
    active_instrument_bar[MAX_IND] = '\0';

    for (i = 0; i < num_voices; i++)
    {
        if (Voice_Stopped(i)) continue;  // skip them stopped voices
        if (i < MAX_IND) active_channel_bar[i] = CH_IND;

        vol = Voice_GetVolume(i); //vol = voice_info[i].volume;
        active_channels++;
        
        if (voice_info[i].s != NULL) // samples
        {
            int sample_idx = (int)(voice_info[i].s - module->samples);
            if (sample_idx >= 0 && sample_idx < module->numsmp && sample_idx < MAX_IND)
            {
                active_sample_bar[sample_idx] = CH_IND;
                sample_active[sample_idx] = 1;
                active_samples++;
            }
        }

        if (voice_info[i].i != NULL) // instruments
        {
            int inst_idx = (int)(voice_info[i].i - module->instruments);
            if (inst_idx >= 0 && inst_idx < module->numins && inst_idx < MAX_IND)
            {
                active_instrument_bar[inst_idx] = CH_IND;
                instrument_active[inst_idx] = 1;
                active_instruments++;
            }
        }

        if (vol > max_volume) max_volume = vol;
    }

    // VU interpolation
    vu_level = (max_volume * MAX_IND) / 256;
    if (vu_level > MAX_IND) vu_level = MAX_IND;

    // song position
    if (module->numpos > 1) progress_level = (module->sngpos * MAX_IND) / (module->numpos - 1);
    else progress_level = 0;

    // row position
    if (current_pattern < module->numpat && module->pattrows)  max_rows = module->pattrows[current_pattern];
    if (max_rows > 0) row_level = (module->patpos * MAX_IND) / (max_rows - 1);
    else row_level = 0;
    if (row_level > MAX_IND) row_level = MAX_IND;
    
    if(!update) // static labels here
    {
        draw_box(41, 3, 78, 11, COL_BLACK_CYAN);
        write_string_attr(43, 3, "Playback", COL_YELLOW_CYAN);

        // static labels once
        write_string_attr(43, 4, "Pat:", COL_BLACK_CYAN);
        write_string_attr(43, 5, "Row:", COL_BLACK_CYAN);
        write_string_attr(43, 6, "Vol:", COL_BLACK_CYAN);
        write_string_attr(43, 8, "Ins:", COL_BLACK_CYAN);
        write_string_attr(43, 9, "Chn:", COL_BLACK_CYAN);
        write_string_attr(43, 10,"Smp:", COL_BLACK_CYAN);        
    }

    // write new data only
    writef2(buf, 48, 4, 3, module->sngpos, cache.sngpos, module->numpos - 1, cache.numpos - 1);
    writef2(buf, 48, 5, 1, module->patpos, cache.patpos, NULL, NULL);
    writef2(buf, 48, 6, 1, volume, cache.volume, NULL, NULL);
    writef2(buf, 48, 8, 2, active_instruments, cache.active_inst, module->numins, cache.numins);
    writef2(buf, 48, 9, 2, active_channels, cache.active_chan, module->numchn, cache.numchn);
    writef2(buf, 48, 10, 2, active_samples, cache.active_samp, module->numsmp, cache.numsmp);

    for (i = 0; i < MAX_IND; i++)
    {
        // song progress
        if(progress_level != cache.progress_level)
        {
            write_char_attr(56 + i, 4, (i < progress_level) ? CH_BLOCK : CH_HLINE, (i < progress_level) ? 0x08 : 0x08);
            if(i == MAX_IND - 1) cache.progress_level = progress_level;
        }

        // rows progress
        if(row_level != cache.row_level)
        {
            write_char_attr(56 + i, 5, (i < row_level) ? CH_BLOCK : CH_HLINE, (i < row_level) ? 0x09 : 0x08);
            if(i == MAX_IND - 1) cache.row_level = row_level;
        }


        // VU meter
        if(vu_level != cache.vu_level)
        {
            int ccol =
            (i < 3)  ? 0x08 :     // gray
            (i < 15) ? 0x02 :     // green
            (i < 18) ? 0x06 :     // yellow
                    0x04;      // red
            write_char_attr(56 + i, 6, (i < vu_level) ? CH_BLOCK : CH_HLINE, (i < vu_level) ? ccol : 0x08);
            if(i == MAX_IND - 1) cache.vu_level = vu_level;
        }

        // indicator bars: instr/chan/samps
        write_char_attr(56 + i, 8, active_instrument_bar[i], (active_instrument_bar[i] == CH_IND) ? 0x0D : 0x08);
        write_char_attr(56 + i, 9, active_channel_bar[i], (active_channel_bar[i] == CH_IND) ? 0x0A : 0x08);
        write_char_attr(56 + i, 10, active_sample_bar[i], (active_sample_bar[i] == CH_IND) ? 0x0E : 0x08);
    }
}

void draw_file_browser()
{
    // TODO
}

void draw_comment_panel(const MODULE *module)
{
    int i, y;
    char *comment = module->comment;
    int line_count = 0;
    int start_line = g_comment_scroll;
    
    draw_box(1, 12, 78, 22, COL_BLACK_CYAN);
    write_string_attr(3, 12, "Comments", COL_YELLOW_CYAN);
    
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
            if (comment[i] == '\n') i++;
            line_count++;
            continue;
        }
        
        // Read line
        while (comment[i] && comment[i] != '\n' && comment[i] != '\r' && len < 74) line[len++] = comment[i++];
        line[len] = '\0';
        
        // Skip CR/LF
        if (comment[i] == '\r') i++;
        if (comment[i] == '\n') i++;

        // end at \0
        if (comment[i] == '\0') 
            g_comment_end = 1;
        else
            g_comment_end = 0;
        
        write_string_attr(3, y++, line, COL_BLACK_CYAN);
        line_count++;
    }
}

void draw_main_panel(const MODULE *module)
{
    /*if (g_view_mode == 0) draw_pattern_viewer(module);
    else draw_comment_panel(module);
    */
    draw_comment_panel(module);
}

void draw_ui(const MODULE *module, int volume, char *s_profile)
{
    int x, y;
    char title[80];

    sprintf(title, "MikPlayer-%d.%d-%s - %s mode", VER_MAJ, VER_MIN, VER_STR, s_profile);

    // background fill
    for (y = 1; y < 24; y++)
        for (x = 0; x < 80; x++) write_char_attr(x, y, CH_SHADE, COL_BLACK_CYAN);

    draw_title_bar(title);
    //draw_menu_bar();
    draw_info_panel(module);
    draw_playback_panel(module, volume, 0);
    draw_main_panel(module);
    draw_status_bar("ESC=Quit SPACE=Pause <-/->=Skip +/-=Vol TAB=View");
}

void update_ui(const MODULE *module, int volume)
{
    draw_playback_panel(module, volume, 1);
    if (g_view_mode == 0) draw_main_panel(module); // Redraw only in pattern mode
}

int process_keyboard(const MODULE *module, int volume)
{
    if (kbhit())
    {
        int ch = getch();
        
        if (ch == 27) return 0; // ESC
        else if (ch == 'd' || ch == 'D') // reset pitch
        {
            pitch_factor = 1.0f;
            //update_ui(g_module, current_volume);
        }
        else if (ch == 'l' || ch == 'L') // Loop mode
        {
            loop_enabled = !loop_enabled;
        }
        else if (ch == '[' || ch == '{')  // pitch down
        {
            pitch_percent -= 1;
            if (pitch_percent < -50) pitch_percent = -50;
            pitch_factor = 1.0f + (pitch_percent / 100.0f);
            //update_ui(g_module, current_volume);
        }
        else if (ch == ']' || ch == '}')  // pitch up
        {
            pitch_percent += 1;
            if (pitch_percent > 100) pitch_percent = 100;
            pitch_factor = 1.0f + (pitch_percent / 100.0f);
            //update_ui(g_module, current_volume);
        }
        else if (ch == 'q' || ch == 'Q') return 0;
        else if (ch == 9) // TAB
        {
            /*g_view_mode = (g_view_mode == 0) ? 1 : 0;
            g_channel_offset = 0;
            g_comment_scroll = 0;
            */
            draw_main_panel(module);
        }
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
                if (g_view_mode == 0 && g_channel_offset > 0)
                {
                    g_channel_offset -= g_max_visible_channels;
                    if (g_channel_offset < 0) g_channel_offset = 0;
                    draw_main_panel(module);
                }
                else if (module->sngpos > 0 && g_view_mode != 0)
                {
                    Player_SetPosition(module->sngpos - 1);
                    update_ui(module, volume);
                }
            }
            else if (ch == 77) // Right arrow
            {
                if (g_view_mode == 0 && (g_channel_offset + g_max_visible_channels) < module->numchn)
                {
                    g_channel_offset += g_max_visible_channels;
                    draw_main_panel(module);
                }
                else if (module->sngpos < module->numpos - 1 && g_view_mode != 0)
                {
                    Player_SetPosition(module->sngpos + 1);
                    update_ui(module, volume);
                }
            }
            else if (ch == 72) // Up arrow 
            {
                if (g_view_mode == 1 && g_comment_scroll > 0)
                {
                    g_comment_scroll--;
                    draw_main_panel(module);
                }
            }
            else if (ch == 80) // Down arrow
            {
                if (g_view_mode == 1)
                {
                    if(!g_comment_end) g_comment_scroll++;
                    draw_main_panel(module);
                }
            }
        }
    }
    return 1;
}

void PitchControlCallback(void)
{

    if (pitch_factor != 1.0f) Player_SetTempo((int)(g_module->bpm * pitch_factor));

    if (pitch_factor == 1.0f)
    {
        // still need to clear any stale base markers for stopped voices
        for (int i = 0; i < g_module->numvoices && i < MAX_VOICES_CAP; ++i)
            if (Voice_Stopped(i) && voice_base_set[i]) voice_base_set[i] = 0;

        return;
    }

    for (int i = 0; i < g_module->numvoices && i < MAX_VOICES_CAP; ++i)
    {
        if (Voice_Stopped(i))
        {
            if (voice_base_set[i]) voice_base_set[i] = 0; // stored base so new notes will capture a fresh base

            continue;
        }

        if (!voice_base_set[i])
        {
            // store base frequency once
            voice_base_freq[i] = Voice_GetFrequency(i);
            voice_base_set[i] = 1;
        }

        // new frequency from stable base
        double newf_d = (double)voice_base_freq[i] * (double)pitch_factor;
        if (newf_d < 1.0) newf_d = 1.0; // clamp to avoid 0
        if (newf_d > 4294967295.0) newf_d = 4294967295.0; //  clamp to ULONG max

        ULONG newf = (ULONG)newf_d;
        Voice_SetFrequency(i, newf);
    }
}

int main(int argc, char *argv[])
{
    clock_t now = 0, last_update = 0;
    int update_interval = CLOCKS_PER_SEC / 10; // 10 times per second
    struct stat file_info;
    int use_memory_load = 0;
    char *s_profile = "default";
    int i;


    printf("\nMikPlayer, ver.%d.%d-%s\n(c) 2025 Dimitar Angelov\n\n", VER_MAJ, VER_MIN, VER_STR);
    
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
        printf("  -a        Set audio initialization verbose messages\n");
        printf("\nSupports: IT, MOD, S3M, XM, etc.\n");
        return 1;
    }

    // command line options
    for (i = 2; i < argc; i++)
    {
        if (strcmp(argv[i], "-386") == 0)
        {
            mix_freq = 11025;
            max_voices = 8;
            force_mono = 1;
            update_interval = CLOCKS_PER_SEC / 4; // 4 times per second
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
        else if (argv[i][0] == '-' && argv[i][1] == 'a') audio_verbose = 1;
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

    if (MikMod_Init(""))
    {
        printf("Could not initialize MikMod: %s\n", MikMod_strerror(MikMod_errno));
        return 1;
    }

    
    if(audio_verbose)
    {
        printf("Environment BLASTER=%s\n", getenv("BLASTER"));
        printf("Supported: \n%s\n", MikMod_InfoDriver());
        printf("Active driver index: %d\n", md_device);
    }

    printf("Audio output (%s): %dHz, %s, %d voices max\n", s_profile, mix_freq, force_mono ? "mono" : "stereo", max_voices);
    
    if (stat(filename, &file_info) == 0)
    {
        file_size = file_info.st_size;
        use_memory_load = (file_size > 0 && file_size < MEM_LOAD_THRESHOLD);
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
            printf("Could not open file for reading!\n");
            MikMod_Exit();
            return 1;
        }
        
        buffer = (char *)malloc(file_size);
        if (!buffer)
        {
            printf("Could not allocate memory for module\n");
            fclose(fp);
            MikMod_Exit();
            return 1;
        }
        
        if (fread(buffer, 1, file_size, fp) != file_size)
        {
            printf("Error reading file\n");
            free(buffer);
            fclose(fp);
            MikMod_Exit();
            return 1;
        }
        fclose(fp);
        
        g_module = Player_LoadMem(buffer, file_size, max_voices, 0);
        free(buffer);
    }
    else
    {
        printf("Streaming %s from disk (%.2f KB) ...\n", filename, file_size / 1024.0);
        g_module = Player_Load(filename, max_voices, 0);
        
    }

    if (!g_module)
    {   
        printf("Could not load module: %s\n", MikMod_strerror(MikMod_errno));
        MikMod_Exit();
        return 1;
    }
    
    Player_Start(g_module);
    Player_SetVolume(current_volume);

    _settextcursor(0x2000);
    draw_ui(g_module, current_volume, s_profile);



    while (process_keyboard(g_module, current_volume)) 
    {
        now = clock();
        MikMod_Update();
        
        // WIP
        //PitchControlCallback();

        // Looping?
        if (loop_enabled && (g_module->sngpos == loop_end)) Player_SetPosition(loop_start);

        if (now - last_update >= update_interval)
        {
            update_ui(g_module, current_volume);
            last_update = now;
        }
        
        if (!Player_Active()) break;
    }
    

    // Cleanup
    _settextcursor(1543);
    _clearscreen(_GCLEARSCREEN);
    printf("Having fun? More info at https://github.com/izne/MikPlayer\n\n");
    Player_Stop();
    Player_Free(g_module);
    MikMod_Exit();
    
    return 0;
}
