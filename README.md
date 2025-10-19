## MikPlayer

### A compact MikMod-based module player for DOS in 32-bit DPMI mode.

#### Setup Open Watcom C

#### Compile MikMod


#### Compile MikPlay
Using the dos32a extender and 486 optimizations:
```
wcl386 -l=dos32a -5s -bt=dos -fp5 -fpi87 -mf -oeatxh -w4 -ei -zp8 -zq -dMIKMOD_STATIC=1 -i..\libmikmod-3.3.13\include\ mikplay.c ..\libmikmod-3.3.13\dos\mikmod.lib
```
