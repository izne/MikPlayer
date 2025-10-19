## MikPlayer

### A compact MikMod-based module player for DOS in 32-bit DPMI mode.
Almost exclusively targeting 486 and slow Pentiums.

#### Setup Open Watcom C
Going to be using the Watcom C compiler, as it is offering some sweet 486 optimizations.

#### Compile MikMod
Inside the libmikmod folder, there is a *dos\* folder with Makefiles for DJGPP and WATCOMC.
```
cd libmikmod-3.3.13
cd dos
wmake -f Makefile.wat
```

#### Compile MikPlay
Using the dos32a extender and optimizations:
```
wcl386 -l=dos32a -5s -bt=dos -fp5 -fpi87 -mf -oeatxh -w4 -ei -zp8 -zq -dMIKMOD_STATIC=1 -i..\libmikmod-3.3.13\include\ mikplay.c ..\libmikmod-3.3.13\dos\mikmod.lib
```
