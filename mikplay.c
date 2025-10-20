/* 
 * MIKPLAY.C - Simple MOD player using MikMod Static Library in 32-bit protected mode using the DOS32A DPMI extender
 * 
 * Use Open Watcom C compiler
 * Compile the latest MikMod to a 32-bit DOS static library
 * 
 * Compile MikPlay with:
 * wcl386 -l=dos32a -5s -bt=dos -fp5 -fpi87 -mf -oeatxh -w4 -ei -zp8 -zq -dMIKMOD_STATIC=1 -i..\libmikmod-3.3.13\include\ mikplay.c ..\libmikmod-3.3.13\dos\mikmod.lib
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
#include <time.h>
#include <sys/stat.h>
#include <mikmod.h>

#define MEM_LOAD_THRESHOLD 6291456  // load to memory if smaller than 6MB
#define VER_MAJ 0
#define VER_MIN 2
#define VER_STR "retrohw"


void update_status(MODULE *module, int volume) {
    int active_channels = 0;
    int i;
    int total_volume = 0;
    int max_volume = 0;
    int vu_level;
    int num_voices = module->numvoices;
    
    // volumeto
    for (i = 0; i < num_voices; i++) {
        if (!Voice_Stopped(i)) {
            int vol = Voice_GetVolume(i);
            active_channels++;
            total_volume += vol;
            if (vol > max_volume) max_volume = vol;
        }
    }
    
    // Fake VU meter level
    vu_level = (max_volume * 10) / 256; // 0-256 range
    if (vu_level > 10) vu_level = 10;   // overdrive
    
    printf("\r%s Pat:%02d/%02d Row:%02d Spd:%02d BPM:%03d Vol:%03d Ch:%02d/%02d VU:[",
		Player_Paused() ? "[PAUSED] " : "[PLAYING]",
           module->sngpos,
           module->numpos - 1,
           module->patpos,
           module->sngspd,
           module->bpm,
           volume,
           active_channels,
           num_voices);
    
    // VU bars
    for (i = 0; i < 10; i++) {
        if (i < vu_level) printf("=");
        else printf(" ");
    }
    printf("] ");
    fflush(stdout);
}

int main(int argc, char *argv[])
{
    MODULE *module;
    char *filename;
    int current_volume = 128;
    clock_t last_update = 0;
    int update_interval = CLOCKS_PER_SEC / 8; // 8 times per second
    struct stat file_info;
    long file_size;
    int use_memory_load = 0;
    int max_voices = 64;
    int mix_freq = 22050;
    int low_cpu_mode = 0;
    int i;
    
    if (argc < 2) {
        printf("\nUsage: mikplay <module_file> [options]\n");
        printf("Options:\n");
        printf("  -386      386 mode: 11kHz, mono, 8 voices, slow display\n");
        printf("  -486      486 mode: 22kHz, stereo, 16 voices\n");
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
        if (strcmp(argv[i], "-386") == 0) {
            mix_freq = 11025;
            max_voices = 8;
            low_cpu_mode = 1;
            update_interval = CLOCKS_PER_SEC; // once per second
            printf("386 mode enabled: 11kHz mono, 8 voices\n");
        }
        else if (strcmp(argv[i], "-486") == 0) {
            mix_freq = 22050;
            max_voices = 16;
            printf("Slow 486 mode enabled: 22kHz stereo, 16 voices\n");
        }
        else if (strcmp(argv[i], "-hifi") == 0) {
            mix_freq = 44100;
            max_voices = 64;
            printf("Hi-Fi mode enabled: 44kHz stereo, 64 voices\n");
        }
        else if (strcmp(argv[i], "-mono") == 0) {
            low_cpu_mode = 1;
            printf("Mono output enabled\n");
        }
        else if (argv[i][0] == '-' && argv[i][1] == 'v') {
            max_voices = atoi(argv[i] + 2);
            printf("Max voices set to: %d\n", max_voices);
        }
        else if (argv[i][0] == '-' && argv[i][1] == 'f') {
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
    
    // init output modes
    md_mode = DMODE_SOFT_MUSIC;
    md_mixfreq = mix_freq;
    if (low_cpu_mode) md_mode &= ~DMODE_STEREO; // disable stereo for 386
    else md_mode |= DMODE_STEREO;
    
    printf("Audio output: %dHz, %s, %d voices max\n",
           mix_freq,
           low_cpu_mode ? "mono" : "stereo",
           max_voices);

    if (MikMod_Init("")) {
        printf("Could not initialize MikMod: %s\n", MikMod_strerror(MikMod_errno));
        return 1;
    }
    
    // Check file size
    if (stat(filename, &file_info) == 0)
    {
        file_size = file_info.st_size;
        if (file_size < MEM_LOAD_THRESHOLD) use_memory_load = 1;
    } 
    else 
        file_size = 0;

    if (use_memory_load)
    {
        FILE *fp;
        char *buffer;
        
        printf("Loading %s to memory (%.2f KB): ...\n", filename, file_size / 1024.0);
        
        fp = fopen(filename, "rb");
        if (!fp) {
            printf("Could not open file for reading\n");
            MikMod_Exit();
            return 1;
        }
        
        buffer = (char *)malloc(file_size);
        if (!buffer) {
            printf("Could not allocate memory for module\n");
            fclose(fp);
            MikMod_Exit();
            return 1;
        }
        
        if (fread(buffer, 1, file_size, fp) != file_size) {
            printf("Error reading file\n");
            free(buffer);
            fclose(fp);
            MikMod_Exit();
            return 1;
        }
        fclose(fp);
        
        module = Player_LoadMem(buffer, file_size, 64, 0);
        free(buffer);
    } 
    else 
    {
        // Load chunks (streaming from file)
        printf("Streaming %s from disk: ...\n", filename);
        module = Player_Load(filename, 64, 0);
    }


    if (!module) {
        printf("Could not load module: %s\n", MikMod_strerror(MikMod_errno));
        MikMod_Exit();
        return 1;
    }
    
    if(module->comment) printf("Comment: %s\n", module->comment);
    printf("Name: %s  Type: %s\n", module->songname, module->modtype);
    printf("Channels: %d  Patterns: %d  Instruments: %d  Samples: %d\n", module->numchn, module->numpos, module->numins, module->numsmp);
    
    Player_Start(module);
    Player_SetVolume(current_volume);

	printf("\n");
    printf("                                                                            ");
    update_status(module, current_volume);
    
    while (1) {
        clock_t now = clock();

        MikMod_Update(); // update audio mixing

        if (now - last_update >= update_interval) { // update periodically
            update_status(module, current_volume);
            last_update = now;
        }
        
        // key press handing
        if (kbhit()) {
            int ch = getch();
            
            if (ch == 27 || ch == 81 || ch == 113) { // ESC or Q or q
                break;
            }
            else if (ch == ' ') { // space
                Player_TogglePause();
                update_status(module, current_volume);
            }
            else if (ch == '+' || ch == '=') {
                if (current_volume < 128) {
                    current_volume += 8;
                    Player_SetVolume(current_volume);
                    update_status(module, current_volume);
                }
            }
            else if (ch == '-' || ch == '_') {
                if (current_volume > 0) {
                    current_volume -= 8;
                    Player_SetVolume(current_volume);
                    update_status(module, current_volume);
                }
            }
            else if (ch == 0) { // extended key (arrows, etc.)
                ch = getch();
                
                if (ch == 75) { // left arrow - skip backward
                    if (module->sngpos > 0) {
                        Player_SetPosition(module->sngpos - 1);
                        update_status(module, current_volume);
                    }
                }
                else if (ch == 77) { // right arrow - skip forward
                    if (module->sngpos < module->numpos - 1) {
                        Player_SetPosition(module->sngpos + 1);
                        update_status(module, current_volume);
                    }
                }
            }
        }
        // eof
        if (!Player_Active()) {
            printf("\nFinished!\n");
            break;
        }
    }
    
    // Cleanup
    Player_Stop();
    Player_Free(module);
    MikMod_Exit();
    
    return 0;
}
