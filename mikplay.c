/* 
 * MIKPLAY.C - Simple MOD player using MikMod Static Library in 32-bit protected mode using the DOS32A DPMI extender
 * 
 * Use Open Watcom C compiler
 * Compile the latest MikMod to a 32-bit DOS static library
 * 
 * Compile MikPlay with:
 * wcl386 -l=dos32a -5s -bt=dos -fp5 -fpi87 -mf -oeatxh -w4 -ei -zp8 -zq -dMIKMOD_STATIC=1 -i..\libmikmod-3.3.13\include\ mikplay.c ..\libmikmod-3.3.13\dos\mikmod.lib
 */
 

#include <stdio.h>
#include <conio.h>
#include <time.h>
#include <mikmod.h>


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
    
    if (argc < 2) {
        printf("Usage: mikplay <module>\n");
        printf("Supports: IT, MOD, S3M, and XM.\n");
        return 1;
    }
    
    filename = argv[1];
    
    printf("\nInitializing MikMod Library v.%ld.%ld.%ld ...\n",
           (MikMod_GetVersion() >> 16) & 0xFF,
           (MikMod_GetVersion() >> 8) & 0xFF,
           MikMod_GetVersion() & 0xFF);
    
    MikMod_RegisterAllLoaders();
    MikMod_RegisterAllDrivers();
    
    // init output modes
    md_mode |= DMODE_SOFT_MUSIC | DMODE_SOFT_SNDFX;
    md_mixfreq = 22050; //44100;
    
    if (MikMod_Init("")) {
        printf("Could not initialize MikMod: %s\n", MikMod_strerror(MikMod_errno));
        return 1;
    }
    
    printf("Loading: %s ...\n", filename);
    
    /* Load chunks (streaming from file) */
    module = Player_Load(filename, 64, 0);
    if (!module) {
        printf("Could not load module: %s\n", MikMod_strerror(MikMod_errno));
        MikMod_Exit();
        return 1;
    }
    
    if(module->comment) printf("Comment: %s\n", module->comment);
    printf("Name: %s\n", module->songname);
    printf("Type: %s\n", module->modtype);
    printf("Patterns: %d\n", module->numpos);
    printf("Channels: %d\n", module->numchn);
    printf("Instruments: %d\n", module->numins);
    printf("Samples: %d\n", module->numsmp);
	printf("DAC sampling rate: %d Hz\n", md_mixfreq);
    
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
