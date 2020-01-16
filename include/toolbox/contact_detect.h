#ifndef CONTACT_DETECT_H
#define CONTACT_DETECT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "shm_data.h"
#include "rdda_base.h"

typedef struct
{
    double cutoff_frequency_HPF[2];
    double sample_time;
    double pressure_threshold[2];
    double reflect_stiffness[2];
    double reflect_distance[2];
    int intersection[2];
} ContactDetectionParams;

typedef struct
{
    double lambda[2];
    double a1[2];
    double b0[2];
    double b1[2];
} ContactDetectionHighPassFilterParams;

typedef struct
{
    double pressure[2];
    double filtered_pressure_HPF[2];
} ContactDetectionPreviousVariable;

void contactDetectionInit(ContactDetectionParams *contactDetectionParams, ContactDetectionHighPassFilterParams *contactDetectionHighPassFilterParams, ContactDetectionPreviousVariable *contactDetectionPreviousVariable, Rdda *rdda);
void contactDetection(ContactDetectionParams *contactDetectionParams, ContactDetectionHighPassFilterParams *contactDetectionHighPassFilterParams, ContactDetectionPreviousVariable *contactDetectionPreviousVariable, Rdda *rdda);

#endif // CONTACT_DETECT_H