#ifndef stm_conv_H
#define stm_conv_H

#include <string>
#include <string.h>
using std::string;

// This file provides a number of useful conversion functions that are generally required to use STM.

/*---------------------------------------------------------------------------*/
/* Flatten/Unflatten Functions                                               */
/*---------------------------------------------------------------------------*/
/*  Flattening and unflattening is the process of taking generic data 
    (i.e. interger, booleans, arrays) and repectivly encoding/decoding to a 
    string so that it can be sent through TCP.  These functions are based on 
    their LabVIEW counterpart.  The LabVIEW STM VIs Flatten data in Big-Endian
    format by default.
*/

    // Flatten Functions
    int intFlatten(int* intpntr, char* charpntr);
    int uintFlatten(unsigned int* uintpntr, char* charpntr);
    int shortFlatten(short* shortpntr, char* charpntr);
    int ushortFlatten(unsigned short* ushortpntr, char* charpntr);
    int sglFlatten(float* floatpntr, char* charpntr);
    int dblFlatten(double* dblpntr, char* charpntr);
    int stringFlatten(string stringobj, char* charpntr);

    // Unflatten Functions
    int intUnflatten(char* charpntr);
    unsigned int uintUnflatten(char* charpntr);
    short shortUnflatten(char* charpntr);
    unsigned short ushortUnflatten(char* charpntr);
    double dblUnflatten(char* charpntr);
    float sglUnflatten(char* charpntr);
    char* stringUnflatten(char* charpntr, int* strsize);

    // Array Flatten Functions
    int intArrFlatten(int* intpntr, char* charpntr, int arrsize);
    int uintArrFlatten(unsigned int* uintpntr, char* charpntr, int arrsize);
    int shortArrFlatten(short* shortpntr, char* charpntr, int arrsize);
    int ushortArrFlatten(unsigned short* ushortpntr, char* charpntr, int arrsize);
    int sglArrFlatten(float* floatpntr, char* charpntr, int arrsize);
    int dblArrFlatten(double* dblpntr, char* charpntr, int arrsize);

    // Array Un-Flatten Functions
    // With any of these functions you must free the returned pointer when done
    int* intArrUnflatten(char* charpntr, int* arrsize);
    unsigned int* uintArrUnflatten(char* charpntr, int* arrsize);
    short* shortArrUnflatten(char* charpntr, int* arrsize);
    unsigned short* ushortArrUnflatten(char* charpntr, int* arrsize);
    double* dblArrUnflatten(char* charpntr, int* arrsize);
    float* sglArrUnflatten(char* charpntr, int* arrsize);

    // 2D Array Un-Flatten Functions
    // With any of these functions you must free xsize arrays under the returned pointer and then free the pointer
    double** dbl2dArrUnflatten(char* charpntr, int* xsize, int* ysize);
    float** sgl2dArrUnflatten(char* charpntr, int* xsize, int* ysize);

#endif /* stm_conv_H */
    